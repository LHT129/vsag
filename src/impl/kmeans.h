
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

#pragma once

#include <cstdint>
#include <vector>

namespace vsag {
using DataType = float;

class KMeans {
public:
    explicit KMeans(uint64_t dim, uint64_t k);

    void
    Train(const std::vector<DataType>& data, int iter = DEFAULT_ITER);

    void
    Train(const DataType* data, int iter = DEFAULT_ITER);  //TODO maybe use vector_view

    static void
    Train(std::vector<float>& result,
          const std::vector<DataType>& data,
          uint64_t dim,
          uint64_t k,
          int iter = DEFAULT_ITER);

    static void
    Train(float* result,
          const DataType* data,
          uint64_t dim,
          uint64_t k,
          int iter = DEFAULT_ITER);  // TODO maybe use vector_view

    [[nodiscard]] inline const std::vector<float>&
    GetCentroids() const {
        return this->centroids_;
    }

private:
    static void
    RandomSampleVectors(std::vector<float>& vecs,
                        const std::vector<DataType>& inputs,
                        uint64_t sample_count,
                        uint64_t dim);

    static std::vector<std::vector<uint64_t>>
    Group(const std::vector<DataType>& data, const std::vector<float>& centroids, uint64_t dim);

    static void
    CalNewCentroids(float* new_centroids,
                    const std::vector<uint64_t>& ids,
                    const std::vector<DataType>& data,
                    uint64_t dim);

private:
    std::vector<float> centroids_{};

    bool is_initial_{true};

    uint64_t dim_{0};

    uint64_t k_{0};

    const static int DEFAULT_ITER = 30;
};
}  // namespace vsag
