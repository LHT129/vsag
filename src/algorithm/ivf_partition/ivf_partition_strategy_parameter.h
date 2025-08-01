
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
#include <fmt/format.h>

#include "data_cell/bucket_datacell_parameter.h"
#include "gno_imi_parameter.h"
#include "inner_string_params.h"
#include "parameter.h"
#include "typing.h"

namespace vsag {

enum class IVFNearestPartitionTrainerType {
    RandomTrainer = 0,
    KMeansTrainer = 1,
};

enum class IVFPartitionStrategyType {
    IVF = 0,
    GNO_IMI = 1,
};

class IVFPartitionStrategyParameters : public Parameter {
public:
    explicit IVFPartitionStrategyParameters();

    void
    FromJson(const JsonType& json) override;

    JsonType
    ToJson() const override;

    bool
    CheckCompatibility(const vsag::ParamPtr& other) const override;

public:
    IVFNearestPartitionTrainerType partition_train_type{
        IVFNearestPartitionTrainerType::KMeansTrainer};
    IVFPartitionStrategyType partition_strategy_type{IVFPartitionStrategyType::IVF};
    GNOIMIParameterPtr gnoimi_param{nullptr};
};

using IVFPartitionStrategyParametersPtr = std::shared_ptr<IVFPartitionStrategyParameters>;
}  // namespace vsag
