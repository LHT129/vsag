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

#include <string>

#include "../index_test_adapter.h"

namespace fixtures {

/**
 * @brief Minimal adapter for the HGraph index.
 *
 * The defaults here mirror the common-case parameters used across
 * test_hgraph.cpp.  Phase 2/3 hardening will add dataset factory helpers
 * and more nuanced build-param generators.
 */
class HGraphTestAdapter : public IndexTestAdapter {
public:
    vsag::IndexPtr
    create(const std::string& build_params) override {
        auto result = vsag::Factory::CreateIndex("hgraph", build_params);
        REQUIRE(result.has_value());
        return result.value();
    }

    TestDatasetPtr
    generate_dataset(int64_t dim, int64_t count) override {
        auto dataset = TestDataset::CreateTestDataset(static_cast<uint64_t>(dim), static_cast<uint64_t>(count));
        REQUIRE(dataset != nullptr);
        return dataset;
    }

    std::string
    default_build_params() override {
        // Simple 2-layer HGraph with default quantisation.
        return R"(
        {
            "dtype": "float32",
            "metric_type": "l2",
            "dim": 128,
            "hgraph": {
                "ef_construction": 200,
                "M": 16,
                "base_quantization_type": "sq8",
                "use_reference": true,
                "build_with_hgraph": false
            }
        }
        )";
    }

    std::string
    default_search_params() override {
        return R"(
        {
            "hgraph": {
                "ef_search": 100
            }
        }
        )";
    }

    void
    build(vsag::IndexPtr& index, const TestDatasetPtr& dataset) override {
        TestIndex::TestBuildIndex(index, dataset, true);
    }

    void
    add(vsag::IndexPtr& index, const TestDatasetPtr& dataset) override {
        TestIndex::TestAddIndex(index, dataset, true);
    }

    void
    update_by_id(vsag::IndexPtr& index,
                 int64_t /*id*/,
                 const TestDatasetPtr& /*dataset*/) override {
        // update_by_id not applicable for HGraph; no-op.
    }

    float
    knn_search(vsag::IndexPtr& index,
               const TestDatasetPtr& dataset,
               const std::string& search_params) override {
        return TestIndex::TestKnnSearch(index, dataset, search_params, 0.99F, true);
    }

    float
    range_search(vsag::IndexPtr& index,
                 const TestDatasetPtr& dataset,
                 const std::string& search_params) override {
        return TestIndex::TestRangeSearch(index, dataset, search_params, 0.99F, true);
    }

    vsag::IndexPtr
    serialize_and_reload(vsag::IndexPtr& index) override {
        auto reloaded = vsag::Factory::CreateIndex("hgraph", default_build_params());
        REQUIRE(reloaded.has_value());
        TestIndex::TestSerializeFile(index, reloaded.value(), nullptr, default_search_params(), true);
        return reloaded.value();
    }

    void
    concurrent_add(vsag::IndexPtr& index,
                   const TestDatasetPtr& dataset) override {
        TestIndex::TestConcurrentAdd(index, dataset, true);
    }
};

}  // namespace fixtures
