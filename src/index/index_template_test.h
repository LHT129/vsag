
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

#include "../logger.h"
#include "catch2/catch_template_test_macros.hpp"
#include "nlohmann/json.hpp"
#include "vsag/index.h"

namespace vsag {

class IndexUnitTest {
public:
    explicit IndexUnitTest(IndexPtr& index, int dim) : index_(index), dim_(dim) {
        vsag::logger::set_level(vsag::logger::level::debug);
    };

    void
    TestBuild();

    void
    TestKnnSearchBehavior(const nlohmann::json& params,
                          bool is_build = false,
                          bool is_valid_param = true);

    IndexPtr index_{nullptr};

    DatasetPtr
    GenerateOneDataset(int dim, int64_t count);

    int dim_{0};

    int base_count_{10};

private:
    std::vector<std::tuple<std::vector<int64_t>, std::vector<float>>> dataset_resources_;
};
}  // namespace vsag