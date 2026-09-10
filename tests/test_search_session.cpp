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

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <set>
#include <vector>

#include "vsag/dataset.h"
#include "vsag/factory.h"
#include "vsag/filter.h"
#include "vsag/index.h"
#include "vsag/index_features.h"
#include "vsag/iterator_context.h"
#include "vsag/search_session.h"

namespace {
constexpr int64_t kCount = 128;
constexpr int64_t kBatch = 8;
constexpr const char* kSearch =
    R"({"hgraph":{"ef_search":128,"parallelism":1,"brute_force_threshold":0.0}})";

vsag::IndexPtr
MakeSessionIndex(bool populate = true) {
    auto created = vsag::Factory::CreateIndex("hgraph", R"({
        "dtype":"float32", "metric_type":"l2", "dim":1,
        "index_param":{"base_quantization_type":"fp32","max_degree":16,
        "ef_construction":128,"use_reorder":false}
    })");
    REQUIRE(created.has_value());
    auto index = created.value();
    if (populate) {
        std::vector<float> vectors(kCount);
        std::vector<int64_t> ids(kCount);
        for (int64_t i = 0; i < kCount; ++i) {
            vectors[i] = static_cast<float>(i);
            // Exercise external labels rather than internal graph offsets.
            ids[i] = 1000 + 3 * i;
        }
        auto base = vsag::Dataset::Make();
        base->NumElements(kCount)
            ->Dim(1)
            ->Ids(ids.data())
            ->Float32Vectors(vectors.data())
            ->Owner(false);
        auto built = index->Build(base);
        REQUIRE(built.has_value());
        REQUIRE(built.value().empty());
        REQUIRE(index->CheckFeature(vsag::IndexFeature::SUPPORT_CONTINUE_SEARCH_SESSION));
        REQUIRE(index->CheckFeature(vsag::IndexFeature::SUPPORT_KNN_ITERATOR_FILTER_SEARCH));
    }
    return index;
}

vsag::DatasetPtr
MakeSessionQuery(float& value) {
    auto query = vsag::Dataset::Make();
    query->NumElements(1)->Dim(1)->Float32Vectors(&value)->Owner(false);
    return query;
}

void
RequireSamePage(const vsag::DatasetPtr& actual, const vsag::DatasetPtr& expected) {
    REQUIRE(actual != nullptr);
    REQUIRE(expected != nullptr);
    REQUIRE(actual->GetDim() == expected->GetDim());
    for (int64_t i = 0; i < actual->GetDim(); ++i) {
        REQUIRE(actual->GetIds()[i] == expected->GetIds()[i]);
        REQUIRE(actual->GetDistances()[i] == expected->GetDistances()[i]);
    }
}

class SessionFilter : public vsag::Filter {
public:
    explicit SessionFilter(bool accept) : accept_(accept) {
    }
    bool
    CheckValid(int64_t) const override {
        return accept_;
    }
    bool
    CheckValid(const char*) const override {
        return accept_;
    }
    float
    ValidRatio() const override {
        return accept_ ? 1.0F : 0.0F;
    }

private:
    bool accept_;
};

// Own even an updated legacy context if a REQUIRE unwinds the test.
struct LegacyContext {
    vsag::IteratorContext* value{nullptr};
    ~LegacyContext() {
        delete value;
    }
};
}  // namespace

TEST_CASE("HGraph search session three batches match legacy labels and recall",
          "[ft][hgraph][search_session]") {
    auto index = MakeSessionIndex();
    float value = -0.25F;
    auto query = MakeSessionQuery(value);
    auto opened = index->OpenSearchSession(query, kBatch, kSearch);
    REQUIRE(opened.has_value());
    auto session = std::move(opened.value());
    REQUIRE(session != nullptr);
    LegacyContext legacy;
    std::set<int64_t> session_labels;
    std::set<int64_t> legacy_labels;
    for (int batch = 0; batch < 3; ++batch) {
        INFO("batch=" << batch);
        REQUIRE(session->HasMore());
        auto actual = session->Next(kBatch);
        auto expected =
            index->KnnSearch(query, kBatch, kSearch, vsag::FilterPtr(nullptr), legacy.value, false);
        REQUIRE(actual.has_value());
        REQUIRE(expected.has_value());
        REQUIRE(actual.value()->GetDim() == kBatch);
        RequireSamePage(actual.value(), expected.value());
        for (int64_t i = 0; i < kBatch; ++i) {
            REQUIRE(session_labels.insert(actual.value()->GetIds()[i]).second);
            REQUIRE(legacy_labels.insert(expected.value()->GetIds()[i]).second);
        }
    }
    // For this one-dimensional dataset the exact nearest 24 labels are known.
    int session_hits = 0;
    int legacy_hits = 0;
    for (int64_t i = 0; i < 3 * kBatch; ++i) {
        session_hits += session_labels.count(1000 + 3 * i);
        legacy_hits += legacy_labels.count(1000 + 3 * i);
    }
    const double session_recall = session_hits / static_cast<double>(3 * kBatch);
    const double legacy_recall = legacy_hits / static_cast<double>(3 * kBatch);
    REQUIRE(session_recall >= legacy_recall);
}

TEST_CASE("HGraph search session splits pending batch without skipping or duplicate labels",
          "[ut][hgraph][search_session]") {
    auto index = MakeSessionIndex();
    float value = -0.25F;
    auto opened = index->OpenSearchSession(MakeSessionQuery(value), kBatch, kSearch);
    REQUIRE(opened.has_value());
    auto session = std::move(opened.value());
    std::vector<int64_t> expected_ids;
    std::vector<float> expected_distances;
    {
        LegacyContext legacy;
        for (int batch = 0; batch < 2; ++batch) {
            auto expected = index->KnnSearch(MakeSessionQuery(value),
                                             kBatch,
                                             kSearch,
                                             vsag::FilterPtr(nullptr),
                                             legacy.value,
                                             false);
            REQUIRE(expected.has_value());
            REQUIRE(expected.value()->GetDim() == kBatch);
            for (int64_t i = 0; i < kBatch; ++i) {
                expected_ids.push_back(expected.value()->GetIds()[i]);
                expected_distances.push_back(expected.value()->GetDistances()[i]);
            }
        }
    }
    std::set<int64_t> labels;
    int64_t offset = 0;
    // The first two calls split the initial eight candidates; the third resumes search.
    for (int64_t requested : {3, 5, 8}) {
        INFO("requested=" << requested << ", offset=" << offset);
        REQUIRE(session->HasMore());
        auto page = session->Next(requested);
        REQUIRE(page.has_value());
        REQUIRE(page.value() != nullptr);
        REQUIRE(page.value()->GetDim() == requested);
        for (int64_t i = 0; i < requested; ++i) {
            REQUIRE(page.value()->GetIds()[i] == expected_ids[offset + i]);
            REQUIRE(page.value()->GetDistances()[i] == expected_distances[offset + i]);
            REQUIRE(labels.insert(page.value()->GetIds()[i]).second);
        }
        offset += requested;
    }
    REQUIRE(offset == 16);
    REQUIRE(labels.size() == 16);
}

TEST_CASE("HGraph search session Close is idempotent and Next is empty",
          "[ut][hgraph][search_session]") {
    auto index = MakeSessionIndex();
    float value = -0.25F;
    auto opened = index->OpenSearchSession(MakeSessionQuery(value), kBatch, kSearch);
    REQUIRE(opened.has_value());
    auto session = std::move(opened.value());
    auto first = session->Next(kBatch);
    REQUIRE(first.has_value());
    REQUIRE(first.value()->GetDim() == kBatch);
    const auto saved_id = first.value()->GetIds()[0];
    const auto saved_distance = first.value()->GetDistances()[0];
    for (int repeat = 0; repeat < 2; ++repeat) {
        session->Close();
        REQUIRE_FALSE(session->HasMore());
        auto empty = session->Next(kBatch);
        REQUIRE(empty.has_value());
        REQUIRE(empty.value() != nullptr);
        REQUIRE(empty.value()->GetDim() == 0);
        REQUIRE_FALSE(session->HasMore());
    }
    session.reset();
    REQUIRE(first.value()->GetIds()[0] == saved_id);
    REQUIRE(first.value()->GetDistances()[0] == saved_distance);
}

TEST_CASE("HGraph search session empty index is terminal", "[ut][hgraph][search_session]") {
    auto index = MakeSessionIndex(false);
    float value = -0.25F;
    auto opened = index->OpenSearchSession(MakeSessionQuery(value), kBatch, kSearch);
    REQUIRE(opened.has_value());
    for (int repeat = 0; repeat < 2; ++repeat) {
        auto empty = opened.value()->Next(kBatch);
        REQUIRE(empty.has_value());
        REQUIRE(empty.value() != nullptr);
        REQUIRE(empty.value()->GetDim() == 0);
        REQUIRE_FALSE(opened.value()->HasMore());
    }
}

TEST_CASE("HGraph search session reject-all filter is terminal", "[ut][hgraph][search_session]") {
    auto index = MakeSessionIndex();
    float value = -0.25F;
    auto opened = index->OpenSearchSession(
        MakeSessionQuery(value), kBatch, kSearch, std::make_shared<SessionFilter>(false));
    REQUIRE(opened.has_value());
    for (int repeat = 0; repeat < 2; ++repeat) {
        auto empty = opened.value()->Next(kBatch);
        REQUIRE(empty.has_value());
        REQUIRE(empty.value() != nullptr);
        REQUIRE(empty.value()->GetDim() == 0);
        REQUIRE_FALSE(opened.value()->HasMore());
    }
}

TEST_CASE("HGraph search session deeply copies query before first Next",
          "[ut][hgraph][search_session]") {
    auto index = MakeSessionIndex();
    float original = -0.25F;
    auto control = index->OpenSearchSession(MakeSessionQuery(original), kBatch, kSearch);
    REQUIRE(control.has_value());
    std::unique_ptr<vsag::SearchSession> session;
    {
        float mutable_value = original;
        auto query = MakeSessionQuery(mutable_value);
        auto opened = index->OpenSearchSession(query, kBatch, kSearch);
        REQUIRE(opened.has_value());
        session = std::move(opened.value());
        // Mutate both borrowed vector storage and Dataset metadata before Next.
        mutable_value = 127.25F;
        query->Dim(0)->NumElements(0);
        auto actual = session->Next(kBatch);
        auto expected = control.value()->Next(kBatch);
        REQUIRE(actual.has_value());
        REQUIRE(expected.has_value());
        REQUIRE(actual.value()->GetDim() == kBatch);
        RequireSamePage(actual.value(), expected.value());
    }
    // The caller's Dataset and vector storage no longer exist for later pages.
    for (int batch = 1; batch < 3; ++batch) {
        auto actual = session->Next(kBatch);
        auto expected = control.value()->Next(kBatch);
        REQUIRE(actual.has_value());
        REQUIRE(expected.has_value());
        REQUIRE(actual.value()->GetDim() == kBatch);
        RequireSamePage(actual.value(), expected.value());
    }
}

TEST_CASE("HGraph search session retains index and destructor releases ownership",
          "[ut][hgraph][search_session]") {
    // Cover destruction of both an unopened search and a partially consumed one.
    for (bool consume : {false, true}) {
        INFO("consume=" << consume);
        auto index = MakeSessionIndex();
        auto filter = std::make_shared<SessionFilter>(true);
        std::weak_ptr<SessionFilter> weak_filter = filter;
        float value = -0.25F;
        auto opened = index->OpenSearchSession(MakeSessionQuery(value), kBatch, kSearch, filter);
        REQUIRE(opened.has_value());
        auto session = std::move(opened.value());
        std::vector<std::vector<int64_t>> expected_ids;
        std::vector<std::vector<float>> expected_distances;
        if (consume) {
            // Collect the baseline before dropping the public index and destroy
            // its iterator so it cannot accidentally keep session state alive.
            LegacyContext legacy;
            for (int batch = 0; batch < 3; ++batch) {
                auto expected = index->KnnSearch(
                    MakeSessionQuery(value), kBatch, kSearch, filter, legacy.value, false);
                REQUIRE(expected.has_value());
                REQUIRE(expected.value()->GetDim() == kBatch);
                // Copy into standard-library storage: legacy Dataset buffers
                // may use the index allocator and must die before index.reset().
                expected_ids.emplace_back(expected.value()->GetIds(),
                                          expected.value()->GetIds() + kBatch);
                expected_distances.emplace_back(expected.value()->GetDistances(),
                                                expected.value()->GetDistances() + kBatch);
            }
        }
        index.reset();
        filter.reset();
        // The public wrapper may die; the underlying index must remain usable.
        REQUIRE_FALSE(weak_filter.expired());
        vsag::DatasetPtr retained_page;
        if (consume) {
            for (int batch = 0; batch < 3; ++batch) {
                auto page = session->Next(kBatch);
                REQUIRE(page.has_value());
                REQUIRE(page.value()->GetDim() == kBatch);
                for (int64_t i = 0; i < kBatch; ++i) {
                    REQUIRE(page.value()->GetIds()[i] == expected_ids[batch][i]);
                    REQUIRE(page.value()->GetDistances()[i] == expected_distances[batch][i]);
                }
                if (batch == 0) {
                    retained_page = page.value();
                }
            }
        }
        // Deliberately omit Close: the destructor must own all cleanup.
        session.reset();
        REQUIRE(weak_filter.expired());
        if (retained_page) {
            REQUIRE(retained_page->GetDim() == kBatch);
            for (int64_t i = 0; i < kBatch; ++i) {
                REQUIRE(retained_page->GetIds()[i] == expected_ids.front()[i]);
                REQUIRE(retained_page->GetDistances()[i] == expected_distances.front()[i]);
            }
        }
    }
}
