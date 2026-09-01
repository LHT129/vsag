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

#include <liburing.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "io/core/read_request.h"
#include "io/uring_io/uring_io.h"
#include "vsag/allocator.h"
#include "vsag_exception.h"

using IoUring = struct io_uring;
using IoUringCqe = struct io_uring_cqe;

namespace {

enum class Injection : uint8_t {
    None,
    PartialSubmit,
    PartialThenFailure,
    WaitEintr,
    WaitFailure,
    ReverseCompletions,
    CompletionError,
    ShortCompletion,
    PermanentInitFailure,
    TransientInitFailure,
};

std::atomic<Injection> injection{Injection::None};
std::atomic<uint64_t> injection_step{0};
std::atomic<uint64_t> init_calls{0};
std::atomic<uint64_t> exit_calls{0};
std::atomic<uint64_t> submit_calls{0};
std::atomic<uint64_t> submitted_requests{0};
std::atomic<uint64_t> wait_calls{0};
std::atomic<uint64_t> completions{0};

void
ResetCounters(Injection next = Injection::None) {
    injection.store(next, std::memory_order_relaxed);
    injection_step.store(0, std::memory_order_relaxed);
    init_calls.store(0, std::memory_order_relaxed);
    exit_calls.store(0, std::memory_order_relaxed);
    submit_calls.store(0, std::memory_order_relaxed);
    submitted_requests.store(0, std::memory_order_relaxed);
    wait_calls.store(0, std::memory_order_relaxed);
    completions.store(0, std::memory_order_relaxed);
}

bool
TakeFirst(Injection expected) {
    if (injection.load(std::memory_order_relaxed) != expected) {
        return false;
    }
    uint64_t expected_step = 0;
    return injection_step.compare_exchange_strong(expected_step, 1, std::memory_order_relaxed);
}

}  // namespace

extern "C" int
__real_io_uring_queue_init(unsigned entries, IoUring* ring, unsigned flags);

extern "C" void
__real_io_uring_queue_exit(IoUring* ring);

extern "C" int
__real_io_uring_submit(IoUring* ring);

extern "C" int
__real___io_uring_get_cqe(
    IoUring* ring, IoUringCqe** cqe, unsigned submit, unsigned wait_count, sigset_t* signal_mask);

extern "C" int
__wrap_io_uring_queue_init(unsigned entries, IoUring* ring, unsigned flags) {
    init_calls.fetch_add(1, std::memory_order_relaxed);
    if (TakeFirst(Injection::PermanentInitFailure)) {
        return -EPERM;
    }
    if (TakeFirst(Injection::TransientInitFailure)) {
        return -EAGAIN;
    }
    return __real_io_uring_queue_init(entries, ring, flags);
}

extern "C" void
__wrap_io_uring_queue_exit(IoUring* ring) {
    exit_calls.fetch_add(1, std::memory_order_relaxed);
    __real_io_uring_queue_exit(ring);
}

extern "C" int
__wrap_io_uring_submit(IoUring* ring) {
    submit_calls.fetch_add(1, std::memory_order_relaxed);
    Injection mode = injection.load(std::memory_order_relaxed);
    uint64_t step = injection_step.load(std::memory_order_relaxed);
    if (mode == Injection::PartialThenFailure and step == 1) {
        injection_step.store(2, std::memory_order_relaxed);
        return -EIO;
    }
    if ((mode == Injection::PartialSubmit or mode == Injection::PartialThenFailure) and step == 0) {
        unsigned original_tail = ring->sq.sqe_tail;
        unsigned ready = original_tail - ring->sq.sqe_head;
        if (ready > 1) {
            ring->sq.sqe_tail = original_tail - 1;
            int result = __real_io_uring_submit(ring);
            ring->sq.sqe_tail = original_tail;
            injection_step.store(1, std::memory_order_relaxed);
            if (result > 0) {
                submitted_requests.fetch_add(static_cast<uint64_t>(result),
                                             std::memory_order_relaxed);
            }
            return result;
        }
    }
    int result = __real_io_uring_submit(ring);
    if (result > 0) {
        submitted_requests.fetch_add(static_cast<uint64_t>(result), std::memory_order_relaxed);
    }
    return result;
}

extern "C" int
__wrap___io_uring_get_cqe(
    IoUring* ring, IoUringCqe** cqe, unsigned submit, unsigned wait_count, sigset_t* signal_mask) {
    wait_calls.fetch_add(1, std::memory_order_relaxed);
    if (TakeFirst(Injection::WaitEintr)) {
        return -EINTR;
    }
    if (TakeFirst(Injection::WaitFailure)) {
        return -EIO;
    }
    int result = __real___io_uring_get_cqe(ring, cqe, submit, wait_count, signal_mask);
    if (result < 0) {
        return result;
    }
    completions.fetch_add(1, std::memory_order_relaxed);
    Injection mode = injection.load(std::memory_order_relaxed);
    if (mode == Injection::ReverseCompletions and
        injection_step.load(std::memory_order_relaxed) == 0) {
        unsigned head = *ring->cq.khead;
        unsigned tail = *ring->cq.ktail;
        if (tail - head > 1) {
            unsigned first = head & *ring->cq.kring_mask;
            unsigned last = (tail - 1) & *ring->cq.kring_mask;
            std::swap(ring->cq.cqes[first], ring->cq.cqes[last]);
            *cqe = &ring->cq.cqes[first];
        }
        injection_step.store(1, std::memory_order_relaxed);
    } else if (mode == Injection::CompletionError and
               injection_step.exchange(1, std::memory_order_relaxed) == 0) {
        (*cqe)->res = -EIO;
    } else if (mode == Injection::ShortCompletion and
               injection_step.exchange(1, std::memory_order_relaxed) == 0) {
        (*cqe)->res = 0;
    }
    return result;
}

namespace vsag {
namespace {

class ProbeAllocator : public Allocator {
public:
    std::string
    Name() override {
        return "UringAioProbeAllocator";
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

[[noreturn]] void
Fail(const std::string& message) {
    std::cerr << "FAIL," << message << '\n';
    std::exit(1);
}

void
Require(bool condition, const std::string& message) {
    if (not condition) {
        Fail(message);
    }
}

std::vector<uint8_t>
MakeData(uint64_t size) {
    std::vector<uint8_t> data(size);
    for (uint64_t i = 0; i < size; ++i) {
        data[i] = static_cast<uint8_t>(i * 67 + 29);
    }
    return data;
}

struct ScatterBatch {
    explicit ScatterBatch(uint64_t count, uint64_t data_size)
        : destinations(count), requests(count) {
        for (uint64_t i = 0; i < count; ++i) {
            uint64_t size = i % 47 + 1;
            uint64_t offset = (i * 7907 + i * i * 19) % (data_size - size + 1);
            destinations[i].resize(size);
            requests[i] = {destinations[i].data(), offset, size};
        }
    }

    void
    Verify(const std::vector<uint8_t>& data) const {
        for (uint64_t i = 0; i < requests.size(); ++i) {
            Require(std::memcmp(destinations[i].data(),
                                data.data() + requests[i].offset,
                                requests[i].size) == 0,
                    "scatter mismatch at request " + std::to_string(i));
        }
    }

    std::vector<std::vector<uint8_t>> destinations;
    std::vector<ReadRequest> requests;
};

void
RunSuccessful(UringIO& io,
              const std::vector<uint8_t>& data,
              Injection mode,
              const std::string& label,
              uint64_t count = 37) {
    ScatterBatch batch(count, data.size());
    ResetCounters(mode);
    Require(io.ReadMany(batch.requests.data(), batch.requests.size()), label + " returned false");
    Require(submitted_requests.load(std::memory_order_relaxed) == count,
            label + " changed the submitted request count");
    Require(completions.load(std::memory_order_relaxed) == count,
            label + " changed the completion count");
    batch.Verify(data);
    std::cout << "PASS," << label << ',' << init_calls.load() << ',' << submit_calls.load() << ','
              << submitted_requests.load() << ',' << wait_calls.load() << ',' << completions.load()
              << ',' << exit_calls.load() << '\n';
}

void
RunFailureThenRecovery(UringIO& io,
                       const std::vector<uint8_t>& data,
                       Injection mode,
                       const std::string& label) {
    ScatterBatch batch(37, data.size());
    ResetCounters(mode);
    bool threw = false;
    try {
        (void)io.ReadMany(batch.requests.data(), batch.requests.size());
    } catch (const VsagException&) {
        threw = true;
    }
    Require(threw, label + " did not throw");
    uint64_t failed_submits = submit_calls.load(std::memory_order_relaxed);
    uint64_t failed_submitted = submitted_requests.load(std::memory_order_relaxed);
    uint64_t failed_waits = wait_calls.load(std::memory_order_relaxed);
    uint64_t failed_completions = completions.load(std::memory_order_relaxed);
    uint64_t failed_exits = exit_calls.load(std::memory_order_relaxed);
    RunSuccessful(io, data, Injection::None, label + "_recovery");
    std::cout << "PASS," << label << ',' << failed_submits << ',' << failed_submitted << ','
              << failed_waits << ',' << failed_completions << ',' << failed_exits << '\n';
}

int
RunFallbackMode(ProbeAllocator& allocator, const std::vector<uint8_t>& data, Injection mode) {
    std::string path =
        "/tmp/vsag_uring_fallback_" + std::to_string(static_cast<uint64_t>(getpid()));
    std::filesystem::remove(path);
    UringIOContextPool pool(0, &allocator);
    IOEnvironment environment = MakeDefaultIOEnvironment(&allocator);
    environment.uring_context_pool = &pool;
    UringIO io(path, environment);
    io.WriteAt(0, data.data(), data.size());
    ScatterBatch first(37, data.size());
    ResetCounters(mode);
    Require(io.ReadMany(first.requests.data(), first.requests.size()),
            "first fallback read failed");
    first.Verify(data);
    ScatterBatch second(37, data.size());
    Require(io.ReadMany(second.requests.data(), second.requests.size()),
            "second fallback/recovery read failed");
    second.Verify(data);
    if (mode == Injection::PermanentInitFailure) {
        Require(init_calls.load() == 1, "permanent failure retried ring initialization");
        Require(submit_calls.load() == 0, "permanent failure did not use sequential fallback");
    } else {
        Require(init_calls.load() == 2, "transient failure did not retry initialization");
        Require(submit_calls.load() == 1, "transient failure did not recover io_uring");
    }
    std::cout << "PASS,fallback," << init_calls.load() << ',' << submit_calls.load() << ','
              << submitted_requests.load() << '\n';
    return 0;
}

}  // namespace
}  // namespace vsag

int
main(int argc, char** argv) {
    using namespace vsag;
    ProbeAllocator allocator;
    auto data = MakeData(8 * 1024 * 1024 + 113);
    if (argc == 2 and std::string(argv[1]) == "permanent-fallback") {
        return RunFallbackMode(allocator, data, Injection::PermanentInitFailure);
    }
    if (argc == 2 and std::string(argv[1]) == "transient-fallback") {
        return RunFallbackMode(allocator, data, Injection::TransientInitFailure);
    }

    std::string suffix = std::to_string(static_cast<uint64_t>(getpid()));
    std::string path = "/tmp/vsag_uring_submission_" + suffix;
    std::filesystem::remove(path);

    UringIOContextPool pool(1, &allocator);
    IOEnvironment environment = MakeDefaultIOEnvironment(&allocator);
    environment.uring_context_pool = &pool;
    UringIO io(path, environment);
    io.WriteAt(0, data.data(), data.size());

    std::cout << "result,case,init,submit_calls,submitted,waits,completions,exits\n";
    RunSuccessful(io, data, Injection::None, "normal_sliced", 1037);
    RunSuccessful(io, data, Injection::ReverseCompletions, "reverse_completions", 100);
    RunSuccessful(io, data, Injection::WaitEintr, "wait_eintr");
    RunSuccessful(io, data, Injection::PartialSubmit, "partial_submit_retry");
    RunFailureThenRecovery(io, data, Injection::PartialThenFailure, "partial_submit_failure");
    RunFailureThenRecovery(io, data, Injection::CompletionError, "completion_error");
    RunFailureThenRecovery(io, data, Injection::ShortCompletion, "short_completion");
    RunFailureThenRecovery(io, data, Injection::WaitFailure, "wait_failure");
    return 0;
}
