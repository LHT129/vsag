
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

#include <cstring>
#include <limits>
#include <vector>

#include "impl/kmeans.h"
#include "quantizer.h"
namespace vsag {
template <int NBits = 8, MetricType metricType = MetricType::METRIC_TYPE_L2SQR>
class ProductQuantizer : public Quantizer<ProductQuantizer<NBits, metricType>> {
public:
    explicit ProductQuantizer(uint64_t dim, int subSpaceCount);

    bool
    TrainImpl(const DataType* data, uint64_t count);

    bool
    EncodeOneImpl(const DataType* data, uint8_t* codes) const;

    bool
    EncodeBatchImpl(const DataType* data, uint8_t* codes, uint64_t count);

    bool
    DecodeOneImpl(const uint8_t* codes, DataType* data);

    bool
    DecodeBatchImpl(const uint8_t* codes, DataType* data, uint64_t count);

    inline float
    ComputeImpl(const uint8_t* codes1, const uint8_t* codes2);

    inline void
    ProcessQueryImpl(const DataType* query,
                     Computer<ProductQuantizer<NBits, metricType>>& computer) const;

    inline void
    ComputeDistImpl(Computer<ProductQuantizer<NBits, metricType>>& computer,
                    const uint8_t* codes,
                    float* dists) const;

private:
    std::vector<DataType> codebook_{};
    uint64_t subSpaceCount_{0};
    bool haveIsolatedSubSpace_{false};
    uint64_t dimPerSubSpace_{0};
    uint64_t isolatedDim_{0};
};

template <int NBits, MetricType metricType>
ProductQuantizer<NBits, metricType>::ProductQuantizer(uint64_t dim, int subSpaceCount)
    : Quantizer<ProductQuantizer<NBits, metricType>>(dim), subSpaceCount_(subSpaceCount) {
    dimPerSubSpace_ = (dim + subSpaceCount - 1) / subSpaceCount;
    if (dimPerSubSpace_ * subSpaceCount != dim) {
        this->haveIsolatedSubSpace_ = true;
        isolatedDim_ = dim % dimPerSubSpace_;
    }
}

template <int NBits, MetricType metricType>
bool
ProductQuantizer<NBits, metricType>::TrainImpl(const DataType* data, uint64_t count) {
    if (this->isTrained_) {
        return true;
    }



    this->isTrained_ = true;
}

}  // namespace vsag
