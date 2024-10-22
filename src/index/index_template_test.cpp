
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

#include "index_template_test.h"

#include "fixtures.h"

namespace vsag {

DatasetPtr
IndexUnitTest::GenerateOneDataset(int dim, int64_t count) {
    auto dataset = vsag::Dataset::Make();
    auto ret = fixtures::generate_ids_and_vectors(count, dim);
    this->dataset_resources_.emplace_back(ret);
    auto& [id, vectors] = this->dataset_resources_.back();

    dataset->Dim(dim)->NumElements(1)->Ids(id.data())->Float32Vectors(vectors.data())->Owner(false);
    return dataset;
}

void
IndexUnitTest::TestBuild() {
    constexpr int max_dim = 4096;
    base_count_ = 10;

    SECTION("build with incorrect dim") {
        int times = 3;
        while (times > 0) {
            int dim = static_cast<int>(random() % max_dim);
            while (dim == dim_) {
                dim = static_cast<int>(random() % max_dim);
            }
            auto dataset = this->GenerateOneDataset(dim, base_count_);
            auto result = index_->Build(dataset);
            REQUIRE_FALSE(result.has_value());
            REQUIRE(result.error().type == vsag::ErrorType::INVALID_ARGUMENT);
            --times;
        }
    }

    SECTION("build with correct dim") {
        int dim = static_cast<int>(random() % max_dim);
        auto dataset = this->GenerateOneDataset(dim, base_count_);
        auto result = index_->Build(dataset);
        REQUIRE(result.has_value());
    }
}

void
IndexUnitTest::TestKnnSearchBehavior(const nlohmann::json& params,
                                     bool is_build,
                                     bool is_valid_param) {
    if (not is_build) {
        auto dataset = this->GenerateOneDataset(dim_, base_count_);
        auto build_result = index_->Build(dataset);
        REQUIRE(build_result.has_value());
    }

    auto query = this->GenerateOneDataset(dim_, 1);
    int64_t k = 10;
    if (is_valid_param) {
        SECTION("invalid parameters k is 0") {
            auto result = index_->KnnSearch(query, 0, params.dump());
            REQUIRE_FALSE(result.has_value());
            REQUIRE(result.error().type == vsag::ErrorType::INVALID_ARGUMENT);
        }

        SECTION("invalid parameters k less than 0") {
            auto result = index_->KnnSearch(query, -1, params.dump());
            REQUIRE_FALSE(result.has_value());
            REQUIRE(result.error().type == vsag::ErrorType::INVALID_ARGUMENT);
        }

        SECTION("query length is not 1") {
            auto query2 = this->GenerateOneDataset(dim_, 2);
            auto result = index_->KnnSearch(query, k, params.dump());
            REQUIRE_FALSE(result.has_value());
            REQUIRE(result.error().type == vsag::ErrorType::INVALID_ARGUMENT);
        }

        SECTION("dimension not equal") {
            auto query2 = this->GenerateOneDataset(dim_ - 1, 1);
            auto result = index_->KnnSearch(query, k, params.dump());
            REQUIRE_FALSE(result.has_value());
            REQUIRE(result.error().type == vsag::ErrorType::INVALID_ARGUMENT);
        }

    } else {
        SECTION("invalid parameters") {
            nlohmann::json invalid_params{};
            auto result = index_->KnnSearch(query, k, invalid_params.dump());
            REQUIRE_FALSE(result.has_value());
            REQUIRE(result.error().type == vsag::ErrorType::INVALID_ARGUMENT);
        }
    }
}

}  // namespace vsag