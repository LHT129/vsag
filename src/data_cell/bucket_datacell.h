
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

#include "bucket_interface.h"

namespace vsag {

template <typename QuantTmpl, typename IOTmpl>
class BucketDataCell : public BucketInterface {
public:
    void
    ScanBucketById(float* result_dists,
                   const ComputerInterfacePtr& computer,
                   const BucketIdType& bucket_id) override;

    float
    QueryOneById(const ComputerInterfacePtr& computer,
                 const BucketIdType& bucket_id,
                 const InnerIdType& offset_id) override;

    ComputerInterfacePtr
    FactoryComputer(const float* query) override;

    void
    Train(const float* data, uint64_t count) override;

    void
    InsertVector(const float* vector, BucketIdType bucket_id, LabelType label) override;

    void
    InsertVector(const float* vector, BucketIdType bucket_id) override;

    LabelType*
    GetLabel(BucketIdType bucket_id) override;

    void
    Prefetch(BucketIdType bucket_id, InnerIdType offset_id) override;

    [[nodiscard]] std::string
    GetQuantizerName() override;

    [[nodiscard]] MetricType
    GetMetricType() override;

    [[nodiscard]] InnerIdType
    GetBucketSize(BucketIdType bucket_id) override;

private:
    std::shared_ptr<QuantTmpl> quantizer_;

    Vector<std::shared_ptr<IOTmpl>> datas_;

    Vector<Vector<LabelType>> labels_;
};

}  // namespace vsag