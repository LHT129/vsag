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
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include "io/buffer_io/buffer_io.h"
#include "io/buffer_io/buffer_io_parameter.h"
#include "io/memory_io/memory_io.h"
#include "io/noncontinuous_io/noncontinuous_allocator.h"
#include "io/noncontinuous_io/noncontinuous_io.h"
#include "io/read_cache/page.h"
#include "vsag/allocator.h"

#ifndef IO_BENCH_BUILD
#define IO_BENCH_BUILD "unknown"
#endif

namespace vsag {
namespace {

constexpr uint64_t DATA_SIZE = 64ULL * 1024 * 1024;
constexpr uint64_t OFFSET_COUNT = 1ULL << 16;
constexpr uint64_t BATCH_SIZE = 32;

class CountingAllocator : public Allocator {
public:
    std::string
    Name() override {
        return "StorageProfileCountingAllocator";
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

struct Statistics {
    double median{0};
    double p95{0};
    double minimum{0};
    double maximum{0};
    double standard_deviation{0};
};

struct Samples {
    std::vector<double> values;
    CounterSnapshot counters;
    uint64_t checksum{0};
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
Samples
Measure(Function&& function,
        CountingAllocator& allocator,
        uint64_t iterations,
        uint64_t sample_count) {
    Samples result;
    result.values.reserve(sample_count);
    DoNotOptimize(function(iterations / 20 + 1));
    for (uint64_t sample = 0; sample < sample_count; ++sample) {
        CounterSnapshot before = Snapshot(allocator);
        auto begin = std::chrono::steady_clock::now();
        result.checksum ^= function(iterations);
        auto end = std::chrono::steady_clock::now();
        Accumulate(result.counters, Difference(Snapshot(allocator), before));
        double nanoseconds =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
        result.values.emplace_back(nanoseconds / static_cast<double>(iterations));
    }
    return result;
}

void
Print(const std::string& profile,
      const std::string& operation,
      uint64_t request_size,
      uint64_t batch_size,
      uint64_t iterations,
      const Samples& samples) {
    Statistics statistics = Summarize(samples.values);
    std::cout << IO_BENCH_BUILD << ',' << profile << ',' << operation << ',' << request_size << ','
              << batch_size << ',' << samples.values.size() << ',' << iterations << ','
              << std::fixed << std::setprecision(3) << statistics.median << ',' << statistics.p95
              << ',' << statistics.minimum << ',' << statistics.maximum << ','
              << statistics.standard_deviation << ',' << samples.counters.allocations << ','
              << samples.counters.deallocations << ',' << samples.counters.reallocations << ','
              << samples.checksum << '\n';
}

std::vector<uint64_t>
MakeRandomOffsets(uint64_t request_size) {
    std::vector<uint64_t> offsets(OFFSET_COUNT);
    uint64_t state = 0x9e3779b97f4a7c15ULL;
    uint64_t limit = DATA_SIZE - request_size + 1;
    for (auto& offset : offsets) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        offset = state % limit;
    }
    return offsets;
}

void
PrepareFile(BufferIO& io) {
    std::vector<uint8_t> data(DATA_SIZE);
    for (uint64_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(i * 131 + 17);
    }
    io.Write(data.data(), data.size(), 0);
}

void
ConfigureCache(BufferIO& io, uint64_t page_count) {
    auto parameter = std::make_shared<BufferIOParameter>();
    parameter->enable_read_cache_ = true;
    parameter->read_cache_total_size_ = page_count * Page::DEFAULT_PAGE_SIZE;
    io.EnableReadCache(parameter);
}

void
WarmCache(BufferIO& io) {
    std::vector<uint8_t> page(Page::DEFAULT_PAGE_SIZE);
    for (uint64_t offset = 0; offset < DATA_SIZE; offset += Page::DEFAULT_PAGE_SIZE) {
        uint64_t size = std::min<uint64_t>(Page::DEFAULT_PAGE_SIZE, DATA_SIZE - offset);
        if (not io.Read(size, offset, page.data())) {
            throw std::runtime_error("cache warmup read failed");
        }
    }
}

void
BenchmarkBuffer(const std::string& path,
                const std::string& profile,
                CountingAllocator& allocator,
                uint64_t sample_count) {
    BufferIO io(path, &allocator);
    PrepareFile(io);
    if (profile == "cache_hit") {
        ConfigureCache(io, DATA_SIZE / Page::DEFAULT_PAGE_SIZE);
        WarmCache(io);
    } else if (profile == "cache_miss" or profile == "duplicate_miss") {
        ConfigureCache(io, 4);
    } else if (profile != "disabled") {
        throw std::runtime_error("unknown buffer profile");
    }

    for (uint64_t request_size : {128ULL, 4096ULL}) {
        auto offsets = MakeRandomOffsets(request_size);
        std::vector<uint8_t> output(request_size * BATCH_SIZE);

        bool miss_profile = profile == "cache_miss" or profile == "duplicate_miss";
        uint64_t single_iterations = miss_profile ? 2'000 : 100'000;
        auto copy = [&](uint64_t iterations) {
            uint64_t checksum = 0;
            for (uint64_t i = 0; i < iterations; ++i) {
                if (not io.Read(request_size, offsets[i % offsets.size()], output.data())) {
                    throw std::runtime_error("copy read failed");
                }
                checksum += output[(i * 17) % request_size];
            }
            return checksum;
        };
        Print(profile,
              "copy",
              request_size,
              1,
              single_iterations,
              Measure(copy, allocator, single_iterations, sample_count));

        auto acquire = [&](uint64_t iterations) {
            uint64_t checksum = 0;
            for (uint64_t i = 0; i < iterations; ++i) {
                bool need_release = false;
                const uint8_t* data =
                    io.Read(request_size, offsets[i % offsets.size()], need_release);
                if (data == nullptr) {
                    throw std::runtime_error("acquire read failed");
                }
                checksum += data[(i * 17) % request_size];
                if (need_release) {
                    io.Release(data);
                }
            }
            return checksum;
        };
        Print(profile,
              "acquire",
              request_size,
              1,
              single_iterations,
              Measure(acquire, allocator, single_iterations, sample_count));

        uint64_t batch_iterations = miss_profile ? 100 : 5'000;
        std::vector<uint64_t> sizes(BATCH_SIZE, request_size);
        std::vector<uint64_t> batch_offsets(BATCH_SIZE);
        auto batch = [&](uint64_t iterations) {
            uint64_t checksum = 0;
            for (uint64_t i = 0; i < iterations; ++i) {
                for (uint64_t j = 0; j < BATCH_SIZE; ++j) {
                    uint64_t index = (i * BATCH_SIZE + j) % offsets.size();
                    if (profile == "duplicate_miss") {
                        index = (i * 8 + j % 8) % offsets.size();
                    }
                    batch_offsets[j] = offsets[index];
                }
                if (not io.MultiRead(
                        output.data(), sizes.data(), batch_offsets.data(), BATCH_SIZE)) {
                    throw std::runtime_error("batch read failed");
                }
                checksum += output[(i * 31) % output.size()];
            }
            return checksum;
        };
        Print(profile,
              "batch",
              request_size,
              BATCH_SIZE,
              batch_iterations,
              Measure(batch, allocator, batch_iterations, sample_count));
    }
}

void
BenchmarkNonContinuous(CountingAllocator& allocator, uint64_t sample_count) {
    NonContinuousAllocator extent_allocator(&allocator);
    NonContinuousIO<MemoryIO> io(&extent_allocator, &allocator, &allocator);
    NonContinuousIO<MemoryIO> spacer(&extent_allocator, &allocator, &allocator);
    constexpr uint64_t chunk_size = 4096;
    constexpr uint64_t chunk_count = 512;
    std::vector<uint8_t> chunk(chunk_size);
    std::vector<uint8_t> spacer_chunk(chunk_size, 0xA5);
    for (uint64_t chunk_id = 0; chunk_id < chunk_count; ++chunk_id) {
        for (uint64_t i = 0; i < chunk.size(); ++i) {
            chunk[i] = static_cast<uint8_t>((chunk_id * chunk_size + i) * 131 + 17);
        }
        io.Write(chunk.data(), chunk.size(), chunk_id * chunk_size);
        spacer.Write(spacer_chunk.data(), spacer_chunk.size(), chunk_id * chunk_size);
    }

    for (uint64_t request_size : {128ULL, 4096ULL}) {
        std::vector<uint64_t> sizes(BATCH_SIZE, request_size);
        std::vector<uint64_t> offsets(BATCH_SIZE);
        std::vector<uint8_t> output(request_size * BATCH_SIZE);
        uint64_t iterations = request_size == 128 ? 20'000 : 5'000;
        auto batch = [&](uint64_t count) {
            uint64_t checksum = 0;
            for (uint64_t i = 0; i < count; ++i) {
                for (uint64_t j = 0; j < BATCH_SIZE; ++j) {
                    uint64_t chunk_id = (i * BATCH_SIZE + j) % (chunk_count - 1);
                    offsets[j] = chunk_id * chunk_size + chunk_size - request_size / 2;
                }
                if (not io.MultiRead(output.data(), sizes.data(), offsets.data(), BATCH_SIZE)) {
                    throw std::runtime_error("non-continuous batch read failed");
                }
                checksum += output[(i * 31) % output.size()];
            }
            return checksum;
        };
        Print("noncontinuous",
              "batch",
              request_size,
              BATCH_SIZE,
              iterations,
              Measure(batch, allocator, iterations, sample_count));
    }
}

}  // namespace
}  // namespace vsag

int
main(int argc, char** argv) {
    using namespace vsag;
    if (argc != 4) {
        std::cerr << "usage: io_storage_profile_benchmark <data-path> <samples> "
                     "<disabled|cache_hit|cache_miss|duplicate_miss|noncontinuous>\n";
        return 1;
    }
    uint64_t sample_count = std::stoull(argv[2]);
    if (sample_count < 3) {
        return 1;
    }
    std::cout
        << "build,profile,operation,request_size,batch_size,samples,iterations,median_ns,"
           "p95_ns,min_ns,max_ns,stddev_ns,allocations,deallocations,reallocations,checksum\n";
    CountingAllocator allocator;
    std::string profile = argv[3];
    if (profile == "noncontinuous") {
        BenchmarkNonContinuous(allocator, sample_count);
    } else {
        BenchmarkBuffer(argv[1], profile, allocator, sample_count);
    }
    return 0;
}
