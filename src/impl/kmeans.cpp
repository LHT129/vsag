
// Copyright 2024-present the vsag project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "kmeans.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <random>

#include "simd/fp32_simd.h"

namespace vsag {

KMeans::KMeans(uint64_t dim, uint64_t k) : dim_(dim), k_(k) {
    centroids_.resize(k * dim);
    is_initial_ = true;
}
void
KMeans::Train(const std::vector<DataType>& data, int iter) {
    if (is_initial_) {
        KMeans::RandomSampleVectors(this->centroids_, data, this->k_, this->dim_);
        is_initial_ = false;
    }
    while (iter > 0) {
        auto groups = KMeans::Group(data, this->centroids_, this->dim_);
        for (auto i = 0; i < this->k_; ++i) {
            KMeans::CalNewCentroids(
                this->centroids_.data() + i * dim_, groups[i], data, this->dim_);
        }
        iter--;
    }
}

void
KMeans::Train(const DataType* data, int iter) {
}

void
KMeans::RandomSampleVectors(std::vector<float>& vecs,
                            const std::vector<DataType>& inputs,
                            uint64_t sample_count,
                            uint64_t dim) {
    vecs.resize(sample_count * dim);
    if (inputs.size() % dim != 0 || inputs.size() < dim * sample_count) {
        // todo
        return;
    }
    auto total_count = inputs.size() % dim;
    std::vector<uint64_t> pool(total_count);
    std::iota(pool.begin(), pool.end(), 0);
    std::shuffle(pool.begin(), pool.end(), std::mt19937(std::random_device()()));
    for (auto i = 0; i < sample_count; ++i) {
        for (auto j = 0; j < dim; ++j) {
            vecs[i * dim + j] = inputs[pool[i] * dim + j];
        }
    }
}
std::vector<std::vector<uint64_t>>
KMeans::Group(const std::vector<DataType>& data,
              const std::vector<float>& centroids,
              uint64_t dim) {
    auto k = centroids.size() / dim;  // todo check valid
    std::vector<std::vector<uint64_t>> result(k);
    auto data_count = data.size() / dim;
    std::vector<uint64_t> groups(data_count);
    for (auto i = 0; i < data_count; ++i) {
        auto cur_data = data.data() + i * dim;
        auto min_dist = std::numeric_limits<float>::max();
        for (auto j = 0; j < k; ++j) {
            auto dist = FP32ComputeL2Sqr(cur_data, centroids.data() + j * dim, dim);
            if (dist < min_dist) {
                min_dist = dist;
                groups[i] = j;
            }
        }
    }
    uint64_t idx = 0;
    for (const auto& group : groups) {
        result[group].emplace_back(idx);
        ++idx;
    }

    return result;
}
void
KMeans::CalNewCentroids(float* new_centroids,
                        const std::vector<uint64_t>& ids,
                        const std::vector<DataType>& data,
                        uint64_t dim) {
    memset(new_centroids, 0, dim * sizeof(float));
    auto count = ids.size();
    for (const auto& id : ids) {
        for (auto j = 0; j < dim; ++j) {
            new_centroids[j] += data[id * dim + j];
        }
    }

    for (auto j = 0; j < dim; ++j) {
        new_centroids[j] /= static_cast<float>(count);
    }
}
void
KMeans::Train(std::vector<float>& result,
              const std::vector<DataType>& data,
              uint64_t dim,
              uint64_t k,
              int iter) {
    RandomSampleVectors(result, data, k, dim);
    while (iter > 0) {
        auto groups = KMeans::Group(data, result, dim);
        for (auto i = 0; i < k; ++i) {
            KMeans::CalNewCentroids(result.data() + i * dim, groups[i], data, dim);
        }
        iter--;
    }
}
void
KMeans::Train(float* result, const DataType* data, uint64_t dim, uint64_t k, int iter) {
}

}  // namespace vsag
