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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "io/buffer_io/buffer_io_parameter.h"
#include "io/buffer_io/buffer_io.h"
#include "io/memory_block_io/memory_block_io.h"
#include "io/mmap_io/mmap_io.h"
#include "io/read_cache/page.h"
#include "vsag/allocator.h"

namespace vsag {
namespace {

constexpr uint64_t DATA_SIZE = 16ULL * 1024 * 1024;
constexpr uint64_t OFFSET_COUNT = 1ULL << 16;
constexpr uint64_t BLOCK_SIZE = 64ULL * 1024;

class CountingAllocator : public Allocator {
public:
    std::string
    Name() override {
        return "BackendProfileCountingAllocator";
    }

    void*
    Allocate(uint64_t size) override {
        allocations_.fetch_add(1, std::memory_order_relaxed);
        return std::malloc(size);
    }

    void
    Deallocate(void* data) override {
        deallocations_.fetch_add(1, std::memory_order_relaxed);
        std::free(data);
    }

    void*
    Reallocate(void* data, uint64_t size) override {
        reallocations_.fetch_add(1, std::memory_order_relaxed);
        return std::realloc(data, size);
    }

    [[nodiscard]] uint64_t
    Allocations() const {
        return allocations_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] uint64_t
    Deallocations() const {
        return deallocations_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] uint64_t
    Reallocations() const {
        return reallocations_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<uint64_t> allocations_{0};
    std::atomic<uint64_t> deallocations_{0};
    std::atomic<uint64_t> reallocations_{0};
};

struct CounterSnapshot {
    uint64_t allocations{0};
    uint64_t deallocations{0};
    uint64_t reallocations{0};
};

struct Samples {
    std::vector<double> values;
    uint64_t checksum{0};
    CounterSnapshot counters;
};

struct Statistics {
    double median{0};
    double p95{0};
    double minimum{0};
    double maximum{0};
    double standard_deviation{0};
};

template <typename T>
inline void
DoNotOptimize(const T& value) {
    asm volatile("" : : "g"(value) : "memory");
}

CounterSnapshot
Snapshot(const CountingAllocator& allocator) {
    return {allocator.Allocations(), allocator.Deallocations(), allocator.Reallocations()};
}

CounterSnapshot
Difference(const CounterSnapshot& after, const CounterSnapshot& before) {
    return {after.allocations - before.allocations,
            after.deallocations - before.deallocations,
            after.reallocations - before.reallocations};
}

void
Accumulate(CounterSnapshot& destination, const CounterSnapshot& source) {
    destination.allocations += source.allocations;
    destination.deallocations += source.deallocations;
    destination.reallocations += source.reallocations;
}

Statistics
Summarize(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    double variance = 0;
    for (double value : values) {
        double difference = value - mean;
        variance += difference * difference;
    }
    variance /= values.size();
    uint64_t p95_index = static_cast<uint64_t>(std::ceil(values.size() * 0.95)) - 1;
    return Statistics{values[values.size() / 2],
                      values[p95_index],
                      values.front(),
                      values.back(),
                      std::sqrt(variance)};
}

template <typename Function>
uint64_t
MeasureOne(Function&& function, uint64_t iterations, double& nanoseconds_per_operation) {
    auto begin = std::chrono::steady_clock::now();
    uint64_t checksum = function(iterations);
    auto end = std::chrono::steady_clock::now();
    double nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
    nanoseconds_per_operation = nanoseconds / static_cast<double>(iterations);
    return checksum;
}

template <typename CompatibilityFunction, typename CanonicalFunction>
std::pair<Samples, Samples>
MeasurePair(CompatibilityFunction&& compatibility,
            CanonicalFunction&& canonical,
            CountingAllocator& allocator,
            uint64_t iterations,
            uint64_t sample_count) {
    Samples compatibility_samples;
    Samples canonical_samples;
    compatibility_samples.values.reserve(sample_count);
    canonical_samples.values.reserve(sample_count);

    for (uint64_t warmup = 0; warmup < 2; ++warmup) {
        double ignored = 0;
        DoNotOptimize(MeasureOne(compatibility, iterations / 20 + 1, ignored));
        DoNotOptimize(MeasureOne(canonical, iterations / 20 + 1, ignored));
    }

    auto measure = [&](auto& function, Samples& samples) {
        double time = 0;
        CounterSnapshot before = Snapshot(allocator);
        samples.checksum ^= MeasureOne(function, iterations, time);
        Accumulate(samples.counters, Difference(Snapshot(allocator), before));
        samples.values.emplace_back(time);
    };
    for (uint64_t sample = 0; sample < sample_count; ++sample) {
        if ((sample & 1U) == 0) {
            measure(compatibility, compatibility_samples);
            measure(canonical, canonical_samples);
        } else {
            measure(canonical, canonical_samples);
            measure(compatibility, compatibility_samples);
        }
    }
    return {std::move(compatibility_samples), std::move(canonical_samples)};
}

void
Print(const std::string& profile,
      const std::string& api_profile,
      const std::string& operation,
      uint64_t request_size,
      uint64_t iterations,
      const Samples& samples) {
    Statistics stats = Summarize(samples.values);
    std::cout << profile << ',' << api_profile << ',' << operation << ',' << request_size << ','
              << samples.values.size() << ',' << iterations << ',' << std::fixed
              << std::setprecision(3) << stats.median << ',' << stats.p95 << ',' << stats.minimum
              << ',' << stats.maximum << ',' << stats.standard_deviation << ','
              << samples.counters.allocations << ',' << samples.counters.deallocations << ','
              << samples.counters.reallocations << ',' << samples.checksum << '\n';
}

std::vector<uint64_t>
MakeSameBlockOffsets(const std::vector<uint64_t>& random, uint64_t request_size) {
    std::vector<uint64_t> offsets(random.size());
    uint64_t blocks = DATA_SIZE / BLOCK_SIZE;
    uint64_t in_block_limit = BLOCK_SIZE - request_size + 1;
    for (uint64_t i = 0; i < offsets.size(); ++i) {
        offsets[i] = (random[i] % blocks) * BLOCK_SIZE + ((random[i] >> 32U) % in_block_limit);
    }
    return offsets;
}

std::vector<uint64_t>
MakeCrossBlockOffsets(const std::vector<uint64_t>& random, uint64_t request_size) {
    std::vector<uint64_t> offsets(random.size());
    uint64_t blocks = DATA_SIZE / BLOCK_SIZE - 1;
    for (uint64_t i = 0; i < offsets.size(); ++i) {
        offsets[i] = (random[i] % blocks + 1) * BLOCK_SIZE - request_size / 2;
    }
    return offsets;
}

std::vector<uint64_t>
MakeRandomOffsets(const std::vector<uint64_t>& random, uint64_t request_size) {
    std::vector<uint64_t> offsets(random.size());
    uint64_t limit = DATA_SIZE - request_size + 1;
    for (uint64_t i = 0; i < offsets.size(); ++i) {
        offsets[i] = random[i] % limit;
    }
    return offsets;
}

template <typename IO>
void
BenchmarkCopies(const std::string& profile,
                IO& io,
                CountingAllocator& allocator,
                const std::vector<uint64_t>& offsets,
                uint64_t request_size,
                uint64_t iterations,
                uint64_t sample_count,
                const std::string& operation) {
    std::vector<uint8_t> compatibility_output(request_size);
    std::vector<uint8_t> canonical_output(request_size);
    auto compatibility_copy = [&](uint64_t count) {
        uint64_t checksum = 0;
        for (uint64_t i = 0; i < count; ++i) {
            uint64_t offset = offsets[i & (OFFSET_COUNT - 1)];
            DoNotOptimize(io.Read(request_size, offset, compatibility_output.data()));
            checksum += compatibility_output[i & (request_size - 1)];
        }
        return checksum;
    };
    auto canonical_copy = [&](uint64_t count) {
        uint64_t checksum = 0;
        for (uint64_t i = 0; i < count; ++i) {
            uint64_t offset = offsets[i & (OFFSET_COUNT - 1)];
            DoNotOptimize(io.ReadAt(offset, request_size, canonical_output.data()));
            checksum += canonical_output[i & (request_size - 1)];
        }
        return checksum;
    };
    auto samples = MeasurePair(compatibility_copy, canonical_copy, allocator, iterations, sample_count);
    Print(profile, "compatibility", operation, request_size, iterations, samples.first);
    Print(profile, "canonical", operation, request_size, iterations, samples.second);
}

template <typename IO>
void
BenchmarkAcquire(const std::string& profile,
                 IO& io,
                 CountingAllocator& allocator,
                 const std::vector<uint64_t>& offsets,
                 uint64_t request_size,
                 uint64_t iterations,
                 uint64_t sample_count,
                 const std::string& operation) {
    auto compatibility_acquire = [&](uint64_t count) {
        uint64_t checksum = 0;
        for (uint64_t i = 0; i < count; ++i) {
            bool need_release = false;
            const uint8_t* data =
                io.Read(request_size, offsets[i & (OFFSET_COUNT - 1)], need_release);
            checksum += data[i & (request_size - 1)];
            if (need_release) {
                io.Release(data);
            }
        }
        return checksum;
    };
    auto canonical_acquire = [&](uint64_t count) {
        uint64_t checksum = 0;
        for (uint64_t i = 0; i < count; ++i) {
            auto lease = io.Acquire(offsets[i & (OFFSET_COUNT - 1)], request_size);
            checksum += lease.Data()[i & (request_size - 1)];
        }
        return checksum;
    };
    auto samples =
        MeasurePair(compatibility_acquire, canonical_acquire, allocator, iterations, sample_count);
    Print(profile, "compatibility", operation, request_size, iterations, samples.first);
    Print(profile, "canonical", operation, request_size, iterations, samples.second);
}

template <typename IO>
void
BenchmarkContiguousBatch(const std::string& profile,
                         IO& io,
                         CountingAllocator& allocator,
                         const std::vector<uint64_t>& offsets,
                         uint64_t request_size,
                         uint64_t batch_size,
                         uint64_t iterations,
                         uint64_t sample_count) {
    std::vector<uint64_t> sizes(batch_size, request_size);
    std::vector<uint64_t> batch_offsets(batch_size);
    std::vector<uint8_t> compatibility_output(request_size * batch_size);
    std::vector<uint8_t> canonical_output(request_size * batch_size);
    auto compatibility_batch = [&](uint64_t count) {
        uint64_t checksum = 0;
        for (uint64_t iteration = 0; iteration < count; ++iteration) {
            for (uint64_t i = 0; i < batch_size; ++i) {
                batch_offsets[i] = offsets[(iteration * batch_size + i) & (OFFSET_COUNT - 1)];
            }
            DoNotOptimize(io.MultiRead(
                compatibility_output.data(), sizes.data(), batch_offsets.data(), batch_size));
            checksum += compatibility_output[iteration & (compatibility_output.size() - 1)];
        }
        return checksum;
    };
    auto canonical_batch = [&](uint64_t count) {
        uint64_t checksum = 0;
        for (uint64_t iteration = 0; iteration < count; ++iteration) {
            for (uint64_t i = 0; i < batch_size; ++i) {
                batch_offsets[i] = offsets[(iteration * batch_size + i) & (OFFSET_COUNT - 1)];
            }
            DoNotOptimize(io.MultiRead(
                canonical_output.data(), sizes.data(), batch_offsets.data(), batch_size));
            checksum += canonical_output[iteration & (canonical_output.size() - 1)];
        }
        return checksum;
    };
    auto samples = MeasurePair(compatibility_batch, canonical_batch, allocator, iterations, sample_count);
    Print(profile, "compatibility", "batch_contiguous", request_size, iterations, samples.first);
    Print(profile, "canonical", "batch_contiguous", request_size, iterations, samples.second);
}

}  // namespace
}  // namespace vsag

int
main(int argc, char** argv) {
    using namespace vsag;
    uint64_t sample_count = argc >= 2 ? std::stoull(argv[1]) : 11;
    std::string selected_profile = argc >= 3 ? argv[2] : "all";
    if (sample_count < 3) {
        return 1;
    }

    CountingAllocator allocator;
    std::vector<uint8_t> source(DATA_SIZE);
    for (uint64_t i = 0; i < source.size(); ++i) {
        source[i] = static_cast<uint8_t>(i * 131 + 17);
    }
    std::vector<uint64_t> random(OFFSET_COUNT);
    uint64_t state = 0x9e3779b97f4a7c15ULL;
    for (auto& value : random) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        value = state;
    }

    std::cout
        << "profile,api_profile,operation,request_size,samples,iterations,median_ns,"
           "p95_ns,min_ns,max_ns,stddev_ns,allocations,deallocations,reallocations,checksum\n";

    if (selected_profile == "all" or selected_profile == "block") {
        MemoryBlockIO block(BLOCK_SIZE, &allocator);
        block.WriteAt(0, source.data(), source.size());
        for (uint64_t request_size : {128ULL, 4096ULL}) {
            auto same_offsets = MakeSameBlockOffsets(random, request_size);
            auto cross_offsets = MakeCrossBlockOffsets(random, request_size);
            BenchmarkCopies("block",
                            block,
                            allocator,
                            same_offsets,
                            request_size,
                            1'000'000,
                            sample_count,
                            "copy_same");
            BenchmarkCopies("block",
                            block,
                            allocator,
                            cross_offsets,
                            request_size,
                            1'000'000,
                            sample_count,
                            "copy_cross");
            BenchmarkAcquire("block",
                             block,
                             allocator,
                             same_offsets,
                             request_size,
                             2'000'000,
                             sample_count,
                             "acquire_same");
            BenchmarkAcquire("block",
                             block,
                             allocator,
                             cross_offsets,
                             request_size,
                             200'000,
                             sample_count,
                             "acquire_cross");
        }
    }

    std::string suffix = std::to_string(static_cast<uint64_t>(getpid()));
    if (selected_profile == "all" or selected_profile == "mmap") {
        std::string path = "/tmp/vsag_mmap_profile_" + suffix;
        std::filesystem::remove(path);
        MMapIO mmap(path, &allocator);
        mmap.WriteAt(0, source.data(), source.size());
        for (uint64_t request_size : {128ULL, 4096ULL}) {
            auto offsets = MakeRandomOffsets(random, request_size);
            BenchmarkCopies("mmap",
                            mmap,
                            allocator,
                            offsets,
                            request_size,
                            1'000'000,
                            sample_count,
                            "copy_random");
            BenchmarkAcquire("mmap",
                             mmap,
                             allocator,
                             offsets,
                             request_size,
                             2'000'000,
                             sample_count,
                             "acquire_random");
        }
    }

    if (selected_profile == "all" or selected_profile == "buffer") {
        std::string path = "/tmp/vsag_buffer_profile_" + suffix;
        std::filesystem::remove(path);
        BufferIO buffer(path, &allocator);
        buffer.WriteAt(0, source.data(), source.size());
        for (uint64_t request_size : {128ULL, 4096ULL}) {
            auto offsets = MakeRandomOffsets(random, request_size);
            BenchmarkCopies("buffer",
                            buffer,
                            allocator,
                            offsets,
                            request_size,
                            100'000,
                            sample_count,
                            "copy_cached");
            BenchmarkAcquire("buffer",
                             buffer,
                             allocator,
                             offsets,
                             request_size,
                             100'000,
                             sample_count,
                             "acquire_cached");
            BenchmarkContiguousBatch("buffer",
                                     buffer,
                                     allocator,
                                     offsets,
                                     request_size,
                                     32,
                                     5'000,
                                     sample_count);
        }
    }

    auto benchmark_buffer_cache = [&](const std::string& profile,
                                      uint64_t page_count,
                                      uint64_t single_iterations,
                                      uint64_t batch_iterations) {
        std::string path = "/tmp/vsag_" + profile + "_" + suffix;
        std::filesystem::remove(path);
        BufferIO io(path, &allocator);
        io.WriteAt(0, source.data(), source.size());
        auto parameter = std::make_shared<BufferIOParameter>();
        parameter->enable_read_cache_ = true;
        parameter->read_cache_total_size_ = page_count * Page::DEFAULT_PAGE_SIZE;
        io.EnableReadCache(parameter);
        for (uint64_t request_size : {128ULL, 4096ULL}) {
            auto offsets = MakeRandomOffsets(random, request_size);
            BenchmarkCopies(profile,
                            io,
                            allocator,
                            offsets,
                            request_size,
                            single_iterations,
                            sample_count,
                            "copy_random");
            BenchmarkAcquire(profile,
                             io,
                             allocator,
                             offsets,
                             request_size,
                             single_iterations,
                             sample_count,
                             "acquire_random");
            BenchmarkContiguousBatch(profile,
                                     io,
                                     allocator,
                                     offsets,
                                     request_size,
                                     32,
                                     batch_iterations,
                                     sample_count);
        }
    };

    if (selected_profile == "buffer_cache_hit") {
        benchmark_buffer_cache(
            "buffer_cache_hit", DATA_SIZE / Page::DEFAULT_PAGE_SIZE, 100'000, 5'000);
    }
    if (selected_profile == "buffer_cache_miss") {
        benchmark_buffer_cache("buffer_cache_miss", 4, 1'000, 50);
    }
    return 0;
}
