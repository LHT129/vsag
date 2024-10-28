
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

#include <nlohmann/json.hpp>
#include <string>

#include "vsag/constants.h"

namespace vsag {
class HGraphParameters {
public:

    HGraphParameters() {
        param_json_ = nlohmann::json::parse(param_str_);
    };

    inline void SetDim(int64_t dim) {
        this->param_json_[DIM] = dim;
    }

    inline void SetIndexParamUseReorder(bool reorder) {
        this->param_json_["index_param"][HGRAPH_USE_REORDER_KEY] = reorder;
    }

    inline void SetIndexParamGraphIOType(std::string io_type_str) {
        auto& json = this->param_json_["index_param"][HGRAPH_GRAPH_KEY];
        json[IO_TYPE_KEY] = io_type_str; // TODO(LHT): check valid
    }

    inline void SetIndexParamGraphIOParamBlockSize(uint64_t size) {
        auto& json = this->param_json_["index_param"][HGRAPH_GRAPH_KEY][IO_PARAMS_KEY];
        json[BLOCK_IO_BLOCK_SIZE_KEY] = size;
    }

    inline void SetIndexParamGraphParamMaxDegree(uint32_t max_degree) {
        auto& json = this->param_json_["index_param"][HGRAPH_GRAPH_KEY][GRAPH_PARAMS_KEY];
        json[GRAPH_PARAM_MAX_DEGREE] = max_degree;
    }

    inline void SetIndexParamGraphParamInitCapacity(uint64_t init_capacity) {
        auto& json = this->param_json_["index_param"][HGRAPH_GRAPH_KEY][GRAPH_PARAMS_KEY];
        json[GRAPH_PARAM_INIT_MAX_CAPACITY] = init_capacity;
    }

    inline void SetIndexParamBasicCodesIOType(std::string io_type_str) {
        auto& json = this->param_json_["index_param"][HGRAPH_BASE_CODES_KEY];
        json[IO_TYPE_KEY] = io_type_str; // TODO(LHT): check valid
    }

    inline void SetIndexParamBasicCodesIOParamBlockSize(uint64_t size) {
        auto& json = this->param_json_["index_param"][HGRAPH_BASE_CODES_KEY][IO_PARAMS_KEY];
        json[BLOCK_IO_BLOCK_SIZE_KEY] = size;
    }

    inline void SetIndexParamBasicCodesQuantizationType(std::string quantization_type) {
        auto& json = this->param_json_["index_param"][HGRAPH_BASE_CODES_KEY];
        json[QUANTIZATION_PARAMS_KEY] = quantization_type; // TODO(LHT): check valid
    }

    inline void SetIndexParamPreciseCodesIOType(std::string io_type_str) {
        auto& json = this->param_json_["index_param"][HGRAPH_PRECISE_CODES_KEY];
        json[IO_TYPE_KEY] = io_type_str; // TODO(LHT): check valid
    }

    inline void SetIndexParamPreciseCodesIOParamBlockSize(uint64_t size) {
        auto& json = this->param_json_["index_param"][HGRAPH_PRECISE_CODES_KEY][IO_PARAMS_KEY];
        json[BLOCK_IO_BLOCK_SIZE_KEY] = size;
    }

    inline void SetIndexParamPreciseCodesQuantizationType(std::string quantization_type) {
        auto& json = this->param_json_["index_param"][HGRAPH_PRECISE_CODES_KEY];
        json[QUANTIZATION_PARAMS_KEY] = quantization_type; // TODO(LHT): check valid
    }

    std::string param_str_{DEFAULT_PARAM};
    nlohmann::json param_json_{};

private:
    static constexpr auto DEFAULT_PARAM = R"(
        {
            "index_type": "hgraph",
            "metric_type": "l2",
            "dim": 128,
            "data_type": "FP32",
            "index_param": {
                "use_reorder": false,
                "graph": {
                    "io_type": "block_memory",
                    "io_params": {
                        "block_size": 134217728
                    },
                    "type": "NSW",
                    "graph_params": {
                        "max_degree": 32,
                        "init_capacity": 1000000
                    }
                },
                "base_codes": {
                    "io_type": "block_memory",
                    "io_params": {
                        "block_size": 134217728
                    },
                    "codes_type": "flatten_codes",
                    "codes_param": {},
                    "quantization_type": "pq",
                    "quantization_params": {
                        "subspace": 64,
                        "nbits": 8
                    }
                },
                "precise_codes": {
                    "io_type": "aio_ssd",
                    "io_params": {},
                    "codes_type": "flatten_codes",
                    "codes_param": {},
                    "quantization_type": "sq8",
                    "quantization_params": {}
                },
                "build_params": {
                    "ef_construction": 300,
                    "build_thread_count": 5
                }
            }
        }
        )";
};
}  // namespace vsag