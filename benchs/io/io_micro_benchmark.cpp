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

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "io/memory_io/memory_io.h"
#include "vsag/allocator.h"

namespace vsag {
namespace {

constexpr uint64_t DATA_SIZE = 64ULL * 1024 * 1024;
constexpr uint64_t OFFSET_COUNT = 1ULL << 16;

class CountingAllocator : public Allocator {
public:
    std::string
    Name() override {
        return "CountingAllocator";
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

    void
    Reset() {
        allocations_.store(0, std::memory_order_relaxed);
        deallocations_.store(0, std::memory_order_relaxed);
        reallocations_.store(0, std::memory_order_relaxed);
    }

    [[nodiscard]] uint64_t
    Allocations() const {
        return allocations_.load(std::memory_order_relaxed);
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

template <typename T>
inline void
DoNotOptimize(const T& value) {
    asm volatile("" : : "g"(value) : "memory");
}

inline void
ClobberMemory() {
    asm volatile("" : : : "memory");
}

struct Samples {
    std::vector<double> values;
    uint64_t checksum{0};
    uint64_t allocations{0};
    uint64_t reallocations{0};
};

struct Statistics {
    double median{0};
    double p95{0};
    double minimum{0};
    double maximum{0};
    double standard_deviation{0};
};

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
        DoNotOptimize(MeasureOne(compatibility, iterations / 10 + 1, ignored));
        DoNotOptimize(MeasureOne(canonical, iterations / 10 + 1, ignored));
    }

    allocator.Reset();
    auto measure = [&](auto&& function, Samples& samples, double& elapsed) {
        const uint64_t allocations_before = allocator.Allocations();
        const uint64_t reallocations_before = allocator.Reallocations();
        samples.checksum ^= MeasureOne(function, iterations, elapsed);
        samples.allocations += allocator.Allocations() - allocations_before;
        samples.reallocations += allocator.Reallocations() - reallocations_before;
    };
    for (uint64_t sample = 0; sample < sample_count; ++sample) {
        double compatibility_time = 0;
        double canonical_time = 0;
        if ((sample & 1U) == 0) {
            measure(compatibility, compatibility_samples, compatibility_time);
            measure(canonical, canonical_samples, canonical_time);
        } else {
            measure(canonical, canonical_samples, canonical_time);
            measure(compatibility, compatibility_samples, compatibility_time);
        }
        compatibility_samples.values.emplace_back(compatibility_time);
        canonical_samples.values.emplace_back(canonical_time);
    }
    return {std::move(compatibility_samples), std::move(canonical_samples)};
}

void
Print(const std::string& api_profile,
      const std::string& operation,
      uint64_t request_size,
      uint64_t iterations,
      const Samples& samples) {
    Statistics stats = Summarize(samples.values);
    std::cout << api_profile << ',' << operation << ',' << request_size << ','
              << samples.values.size() << ',' << iterations << ',' << std::fixed
              << std::setprecision(3) << stats.median << ',' << stats.p95 << ',' << stats.minimum
              << ',' << stats.maximum << ',' << stats.standard_deviation << ','
              << samples.allocations << ',' << samples.reallocations << ',' << samples.checksum
              << '\n';
}

uint64_t
CopyIterations(uint64_t request_size) {
    constexpr uint64_t TARGET_BYTES = 1ULL << 30;
    return std::min<uint64_t>(5'000'000, std::max<uint64_t>(200'000, TARGET_BYTES / request_size));
}

}  // namespace
}  // namespace vsag

int
main(int argc, char** argv) {
    using namespace vsag;
    uint64_t sample_count = 11;
    if (argc == 2) {
        sample_count = std::stoull(argv[1]);
    }
    if (sample_count < 3) {
        std::cerr << "sample count must be at least 3\n";
        return 1;
    }

    CountingAllocator allocator;
    MemoryIO io(&allocator);
    std::vector<uint8_t> source(DATA_SIZE);
    for (uint64_t i = 0; i < source.size(); ++i) {
        source[i] = static_cast<uint8_t>(i * 131 + 17);
    }
    io.WriteAt(0, source.data(), source.size());

    std::vector<uint64_t> random_values(OFFSET_COUNT);
    uint64_t state = 0x9e3779b97f4a7c15ULL;
    for (auto& value : random_values) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        value = state;
    }

    std::cout << "api_profile,operation,request_size,samples,iterations,median_ns,p95_ns,"
                 "min_ns,max_ns,stddev_ns,allocations,reallocations,checksum\n";

    for (uint64_t request_size : {32ULL, 128ULL, 512ULL, 4096ULL}) {
        // The benchmark matrix intentionally uses powers of two so checksum sampling stays cheap.
        if (request_size == 0 or (request_size & (request_size - 1)) != 0) {
            std::cerr << "request size must be a power of two: " << request_size << '\n';
            return 1;
        }
        std::array<uint8_t, 4096> compatibility_output{};
        std::array<uint8_t, 4096> canonical_output{};
        uint64_t iterations = CopyIterations(request_size);
        auto compatibility_copy = [&](uint64_t count) {
            uint64_t checksum = 0;
            uint64_t limit = DATA_SIZE - request_size + 1;
            for (uint64_t i = 0; i < count; ++i) {
                uint64_t offset = random_values[i & (OFFSET_COUNT - 1)] % limit;
                bool result = io.Read(request_size, offset, compatibility_output.data());
                DoNotOptimize(result);
                DoNotOptimize(compatibility_output.data());
                ClobberMemory();
                checksum += compatibility_output[i & (request_size - 1)];
            }
            return checksum;
        };
        auto canonical_copy = [&](uint64_t count) {
            uint64_t checksum = 0;
            uint64_t limit = DATA_SIZE - request_size + 1;
            for (uint64_t i = 0; i < count; ++i) {
                uint64_t offset = random_values[i & (OFFSET_COUNT - 1)] % limit;
                bool result = io.ReadAt(offset, request_size, canonical_output.data());
                DoNotOptimize(result);
                DoNotOptimize(canonical_output.data());
                ClobberMemory();
                checksum += canonical_output[i & (request_size - 1)];
            }
            return checksum;
        };
        auto copy_samples =
            MeasurePair(compatibility_copy, canonical_copy, allocator, iterations, sample_count);
        Print("compatibility", "copy", request_size, iterations, copy_samples.first);
        Print("canonical", "copy", request_size, iterations, copy_samples.second);

        uint64_t acquire_iterations = 5'000'000;
        auto compatibility_acquire = [&](uint64_t count) {
            uint64_t checksum = 0;
            uint64_t limit = DATA_SIZE - request_size + 1;
            for (uint64_t i = 0; i < count; ++i) {
                uint64_t offset = random_values[i & (OFFSET_COUNT - 1)] % limit;
                bool need_release = false;
                const uint8_t* data = io.Read(request_size, offset, need_release);
                DoNotOptimize(data);
                checksum += data[i & (request_size - 1)];
                if (need_release) {
                    io.Release(data);
                }
            }
            return checksum;
        };
        auto canonical_acquire = [&](uint64_t count) {
            uint64_t checksum = 0;
            uint64_t limit = DATA_SIZE - request_size + 1;
            for (uint64_t i = 0; i < count; ++i) {
                uint64_t offset = random_values[i & (OFFSET_COUNT - 1)] % limit;
                auto lease = io.Acquire(offset, request_size);
                DoNotOptimize(lease.Data());
                checksum += lease.Data()[i & (request_size - 1)];
            }
            return checksum;
        };
        auto acquire_samples = MeasurePair(
            compatibility_acquire, canonical_acquire, allocator, acquire_iterations, sample_count);
        Print("compatibility", "acquire", request_size, acquire_iterations, acquire_samples.first);
        Print("canonical", "acquire", request_size, acquire_iterations, acquire_samples.second);
    }
    return 0;
}
