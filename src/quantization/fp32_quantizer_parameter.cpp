
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

#include "fp32_quantizer_parameter.h"

#include "inner_string_params.h"

namespace vsag {
FP32QuantizerParameter::FP32QuantizerParameter()
    : QuantizerParameter(QUANTIZATION_TYPE_VALUE_FP32) {
}

void
FP32QuantizerParameter::FromJson(const JsonType& json) {
    if (json.contains(HOLD_MOLDS)) {
        hold_molds = json[HOLD_MOLDS];
    }
}

JsonType
FP32QuantizerParameter::ToJson() const {
    JsonType json;
    json[QUANTIZATION_TYPE_KEY] = QUANTIZATION_TYPE_VALUE_FP32;
    return json;
}
}  // namespace vsag
