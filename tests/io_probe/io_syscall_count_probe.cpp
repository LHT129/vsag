// Copyright 2024-present the vsag project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <unistd.h>

#include <array>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "io/buffer_io/buffer_io.h"
#include "io/core/read_request.h"
#include "io/policy/configurable_single_read.h"
#include "vsag/allocator.h"

namespace {

std::atomic<uint64_t> pread_count{0};
std::atomic<uint64_t> pwrite_count{0};

enum class PReadFault : uint8_t {
    NONE,
    EINTR_ONCE,
    SHORT_ONCE,
    ERROR_ONCE,
};
std::atomic<PReadFault> pread_fault{PReadFault::NONE};

enum class PWriteFault : uint8_t {
    NONE,
    EINTR_ONCE,
    SHORT_ONCE,
    ZERO_ONCE,
    ERROR_ONCE,
};
std::atomic<PWriteFault> pwrite_fault{PWriteFault::NONE};

}  // namespace

extern "C" ssize_t
__real_pread64(int fd, void* buffer, size_t count, off64_t offset);

extern "C" ssize_t
__real_pwrite64(int fd, const void* buffer, size_t count, off64_t offset);

extern "C" ssize_t
__wrap_pread64(int fd, void* buffer, size_t count, off64_t offset) {
    pread_count.fetch_add(1, std::memory_order_relaxed);
    switch (pread_fault.exchange(PReadFault::NONE, std::memory_order_relaxed)) {
        case PReadFault::EINTR_ONCE:
            errno = EINTR;
            return -1;
        case PReadFault::SHORT_ONCE:
            return __real_pread64(fd, buffer, count / 2, offset);
        case PReadFault::ERROR_ONCE:
            errno = EIO;
            return -1;
        case PReadFault::NONE:
            break;
    }
    return __real_pread64(fd, buffer, count, offset);
}

extern "C" ssize_t
__wrap_pwrite64(int fd, const void* buffer, size_t count, off64_t offset) {
    pwrite_count.fetch_add(1, std::memory_order_relaxed);
    switch (pwrite_fault.exchange(PWriteFault::NONE, std::memory_order_relaxed)) {
        case PWriteFault::EINTR_ONCE:
            errno = EINTR;
            return -1;
        case PWriteFault::SHORT_ONCE:
            return __real_pwrite64(fd, buffer, count / 2, offset);
        case PWriteFault::ZERO_ONCE:
            return 0;
        case PWriteFault::ERROR_ONCE:
            errno = ENOSPC;
            return -1;
        case PWriteFault::NONE:
            break;
    }
    return __real_pwrite64(fd, buffer, count, offset);
}

namespace vsag {
namespace {

class ProbeAllocator : public Allocator {
public:
    std::string
    Name() override {
        return "ProbeAllocator";
    }

    void*
    Allocate(uint64_t size) override {
        return std::malloc(size);
    }

    void
    Deallocate(void* data) override {
        std::free(data);
    }

    void*
    Reallocate(void* data, uint64_t size) override {
        return std::realloc(data, size);
    }
};

struct SyscallCounts {
    uint64_t write{0};
    uint64_t copy{0};
    uint64_t batch{0};
    uint64_t acquire{0};
};

template <typename IO>
SyscallCounts
RunCompatibilityAPI(const char* api_path, IO& io) {
    constexpr uint64_t iterations = 100;
    constexpr uint64_t batch_size = 8;
    constexpr uint64_t request_size = 128;
    std::vector<uint8_t> output(request_size * batch_size);
    std::vector<uint64_t> sizes(batch_size, request_size);
    std::vector<uint64_t> offsets(batch_size);
    for (uint64_t i = 0; i < batch_size; ++i) {
        offsets[i] = i * 4096;
    }

    const uint8_t value = 0x5A;
    pwrite_count.store(0, std::memory_order_relaxed);
    io.Write(&value, 1, 0);
    SyscallCounts counts;
    counts.write = pwrite_count.load();
    std::cout << api_path << ",write," << counts.write << '\n';

    pread_count.store(0, std::memory_order_relaxed);
    for (uint64_t i = 0; i < iterations; ++i) {
        if (not io.Read(request_size, offsets[i % batch_size], output.data())) {
            std::abort();
        }
    }
    counts.copy = pread_count.load();
    std::cout << api_path << ",copy," << counts.copy << '\n';

    pread_count.store(0, std::memory_order_relaxed);
    for (uint64_t i = 0; i < iterations; ++i) {
        if (not io.MultiRead(output.data(), sizes.data(), offsets.data(), batch_size)) {
            std::abort();
        }
    }
    counts.batch = pread_count.load();
    std::cout << api_path << ",batch," << counts.batch << '\n';

    pread_count.store(0, std::memory_order_relaxed);
    for (uint64_t i = 0; i < iterations; ++i) {
        bool need_release = false;
        const uint8_t* data = io.Read(request_size, offsets[i % batch_size], need_release);
        if (data == nullptr) {
            std::abort();
        }
        if (need_release) {
            io.Release(data);
        }
    }
    counts.acquire = pread_count.load();
    std::cout << api_path << ",acquire," << counts.acquire << '\n';
    return counts;
}

template <typename IO>
SyscallCounts
RunCanonicalAPI(const char* api_path, IO& io) {
    constexpr uint64_t iterations = 100;
    constexpr uint64_t batch_size = 8;
    constexpr uint64_t request_size = 128;
    std::vector<uint8_t> output(request_size * batch_size);
    std::vector<ReadRequest> requests(batch_size);
    for (uint64_t i = 0; i < batch_size; ++i) {
        requests[i] = ReadRequest{output.data() + i * request_size, i * 4096, request_size};
    }

    const uint8_t value = 0x5A;
    pwrite_count.store(0, std::memory_order_relaxed);
    io.WriteAt(0, &value, 1);
    SyscallCounts counts;
    counts.write = pwrite_count.load();
    std::cout << api_path << ",write," << counts.write << '\n';

    pread_count.store(0, std::memory_order_relaxed);
    for (uint64_t i = 0; i < iterations; ++i) {
        if (not io.ReadAt(requests[i % batch_size].offset, request_size, output.data())) {
            std::abort();
        }
    }
    counts.copy = pread_count.load();
    std::cout << api_path << ",copy," << counts.copy << '\n';

    pread_count.store(0, std::memory_order_relaxed);
    for (uint64_t i = 0; i < iterations; ++i) {
        if (not io.ReadMany(requests.data(), requests.size())) {
            std::abort();
        }
    }
    counts.batch = pread_count.load();
    std::cout << api_path << ",batch," << counts.batch << '\n';

    pread_count.store(0, std::memory_order_relaxed);
    for (uint64_t i = 0; i < iterations; ++i) {
        auto lease = io.Acquire(requests[i % batch_size].offset, request_size);
        if (not lease) {
            std::abort();
        }
    }
    counts.acquire = pread_count.load();
    std::cout << api_path << ",acquire," << counts.acquire << '\n';
    return counts;
}

void
VerifyWriteRecovery(BufferIO& io) {
    std::array<uint8_t, 32> source{};
    for (uint64_t i = 0; i < source.size(); ++i) {
        source[i] = static_cast<uint8_t>(i * 17);
    }
    std::array<uint8_t, 32> output{};

    pwrite_count.store(0, std::memory_order_relaxed);
    pwrite_fault.store(PWriteFault::EINTR_ONCE, std::memory_order_relaxed);
    io.WriteAt(4096, source.data(), source.size());
    if (pwrite_count.load(std::memory_order_relaxed) != 2 or
        not io.ReadAt(4096, output.size(), output.data()) or output != source) {
        std::abort();
    }

    output.fill(0);
    pwrite_count.store(0, std::memory_order_relaxed);
    pwrite_fault.store(PWriteFault::SHORT_ONCE, std::memory_order_relaxed);
    io.WriteAt(8192, source.data(), source.size());
    if (pwrite_count.load(std::memory_order_relaxed) != 2 or
        not io.ReadAt(8192, output.size(), output.data()) or output != source) {
        std::abort();
    }

    pwrite_fault.store(PWriteFault::ZERO_ONCE, std::memory_order_relaxed);
    try {
        io.WriteAt(12288, source.data(), source.size());
        std::abort();
    } catch (const VsagException&) {
    }

    pwrite_fault.store(PWriteFault::ERROR_ONCE, std::memory_order_relaxed);
    try {
        io.WriteAt(16384, source.data(), source.size());
        std::abort();
    } catch (const VsagException&) {
    }
}

void
VerifyReadRecovery(BufferIO& io, const std::string& path, Allocator* allocator) {
    constexpr uint64_t offset = 32768;
    std::array<uint8_t, 32> output{};

    pread_count.store(0, std::memory_order_relaxed);
    pread_fault.store(PReadFault::EINTR_ONCE, std::memory_order_relaxed);
    if (not io.ReadAt(offset, output.size(), output.data()) or
        pread_count.load(std::memory_order_relaxed) != 2 or output[0] != 0x5A) {
        std::abort();
    }

    output.fill(0);
    pread_count.store(0, std::memory_order_relaxed);
    pread_fault.store(PReadFault::SHORT_ONCE, std::memory_order_relaxed);
    if (not io.ReadAt(offset, output.size(), output.data()) or
        pread_count.load(std::memory_order_relaxed) != 2 or output[0] != 0x5A) {
        std::abort();
    }

    pread_fault.store(PReadFault::ERROR_ONCE, std::memory_order_relaxed);
    try {
        (void)io.ReadAt(offset, output.size(), output.data());
        std::abort();
    } catch (const VsagException& exception) {
        if (std::string(exception.what()).find("errno=5") == std::string::npos) {
            std::abort();
        }
    }

    PosixFile direct_file(FileOpenOptions{path, false, true, true, FileOwnership::Keep});
    IOEnvironment environment;
    environment.allocator = allocator;
    environment.direct_read = true;
    ConfigurableSingleRead direct_read(environment);
    pread_count.store(0, std::memory_order_relaxed);
    pread_fault.store(PReadFault::EINTR_ONCE, std::memory_order_relaxed);
    auto lease = direct_read.Acquire(direct_file, allocator, offset, output.size());
    if (not lease or pread_count.load(std::memory_order_relaxed) != 2 or lease.Data()[0] != 0x5A) {
        std::abort();
    }
}

void
VerifyDirectReadBufferMove() {
    DirectReadBuffer source(32, 3, 64);
    DirectReadBuffer moved(std::move(source));
    if (source.Base() != nullptr or source.Data() != nullptr or source.SubmitSize() != 0 or
        source.SubmitOffset() != 0 or source.MinimumResultSize() != 0 or
        source.RequestedSize() != 0) {
        std::abort();
    }

    DirectReadBuffer assigned;
    assigned = std::move(moved);
    if (moved.Base() != nullptr or moved.Data() != nullptr or moved.SubmitSize() != 0 or
        moved.SubmitOffset() != 0 or moved.MinimumResultSize() != 0 or moved.RequestedSize() != 0 or
        assigned.RequestedSize() != 32) {
        std::abort();
    }

    const auto* assigned_base = assigned.Base();
    assigned = std::move(assigned);
    if (assigned.Base() != assigned_base or assigned.RequestedSize() != 32) {
        std::abort();
    }
}

}  // namespace
}  // namespace vsag

int
main() {
    using namespace vsag;
    ProbeAllocator allocator;
    std::string suffix = std::to_string(static_cast<uint64_t>(getpid()));
    std::string compatibility_api_path = "/tmp/vsag_syscall_compatibility_api_" + suffix;
    std::string canonical_api_path = "/tmp/vsag_syscall_canonical_api_" + suffix;
    std::filesystem::remove(compatibility_api_path);
    std::filesystem::remove(canonical_api_path);
    BufferIO compatibility_api(compatibility_api_path, &allocator);
    BufferIO canonical_api(canonical_api_path, &allocator);
    std::vector<uint8_t> source(64 * 1024, 0x5A);
    compatibility_api.Write(source.data(), source.size(), 0);
    canonical_api.WriteAt(0, source.data(), source.size());

    std::cout << "api_path,operation,syscall_calls\n";
    auto compatibility = RunCompatibilityAPI("compatibility_api", compatibility_api);
    auto canonical = RunCanonicalAPI("canonical_api", canonical_api);
    bool counts_match =
        compatibility.write == canonical.write and compatibility.copy == canonical.copy and
        compatibility.batch == canonical.batch and compatibility.acquire == canonical.acquire;
    bool expected_counts = canonical.write == 1 and canonical.copy == 100 and
                           canonical.batch == 800 and canonical.acquire == 100;
    if (not counts_match or not expected_counts) {
        std::cerr << "FAIL: unexpected syscall count\n";
        return 1;
    }
    VerifyReadRecovery(canonical_api, canonical_api_path, &allocator);
    VerifyWriteRecovery(canonical_api);
    VerifyDirectReadBufferMove();
    return 0;
}
