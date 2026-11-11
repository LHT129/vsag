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

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../test_index.h"
#include "framework/test_dataset.h"
#include "vsag/vsag.h"

namespace fixtures {

/**
 * @struct IndexTestParams
 * @brief Metadata for a registered index that controls test execution.
 */
struct IndexTestParams {
    /// Display name used in test output and registry lookups.
    std::string name;

    /// Factory-level index type string (e.g. "hgraph", "pyramid", "ivf").
    std::string index_type;

    /// Default recall threshold for KnnSearch / RangeSearch assertions.
    float min_recall{0.99F};

    /// Test names to skip for this index.
    std::vector<std::string> skip_tests;
};

/**
 * @class IndexTestAdapter
 * @brief Uniform interface for index-specific test operations.
 *
 * Each index type (hgraph, pyramid, ivf, ...) implements this adapter so that
 * shared, parameterised test templates can operate against any index without
 * knowing the concrete index type.
 */
class IndexTestAdapter {
public:
    virtual ~IndexTestAdapter() = default;

    // ---- Construction -------------------------------------------------------
    /// Create an index with the given *build* parameters.
    virtual vsag::IndexPtr
    create(const std::string& build_params) = 0;

    // ---- Data generation ----------------------------------------------------
    /// Generate a dataset suitable for this index.
    virtual TestDatasetPtr
    generate_dataset(int64_t dim, int64_t count) = 0;

    // ---- Parameter defaults -------------------------------------------------
    /// Return a canonical build-parameter JSON string used for basic tests.
    virtual std::string
    default_build_params() = 0;

    /// Return a canonical search-parameter JSON string used for basic tests.
    virtual std::string
    default_search_params() = 0;

    // ---- Build / add / update -----------------------------------------------
    /// Build base vectors into an already-created index.
    virtual void
    build(vsag::IndexPtr& index, const TestDatasetPtr& dataset) = 0;

    /// Add vectors to an already-built index.
    virtual void
    add(vsag::IndexPtr& index, const TestDatasetPtr& dataset) = 0;

    /// Update a vector by id.
    virtual void
    update_by_id(vsag::IndexPtr& index,
                 int64_t id,
                 const TestDatasetPtr& dataset) = 0;

    // ---- Search -------------------------------------------------------------
    /// Run a k-NN search and return the observed recall.
    virtual float
    knn_search(vsag::IndexPtr& index,
               const TestDatasetPtr& dataset,
               const std::string& search_params) = 0;

    /// Run a range search and return the observed recall.
    virtual float
    range_search(vsag::IndexPtr& index,
                 const TestDatasetPtr& dataset,
                 const std::string& search_params) = 0;

    // ---- Serialization ------------------------------------------------------
    /// Serialize to file and deserialize; return a *new* deserialized index.
    virtual vsag::IndexPtr
    serialize_and_reload(vsag::IndexPtr& index) = 0;

    // ---- Concurrency --------------------------------------------------------
    /// Run concurrent add on the index.
    virtual void
    concurrent_add(vsag::IndexPtr& index,
                   const TestDatasetPtr& dataset) = 0;
};

/**
 * @class IndexTestRegistry
 * @brief Singleton registry that maps index names to their Adapter instances.
 *
 * Used by Catch2 parameterised test templates: a test tagged [hgraph] can
 * instantiate only the registered adapter(s) whose IndexTestParams::index_type
 * matches the tag.
 */
class IndexTestRegistry {
public:
    struct Entry {
        IndexTestParams params;
        std::unique_ptr<IndexTestAdapter> adapter;
    };

    static IndexTestRegistry&
    instance() {
        static IndexTestRegistry registry;
        return registry;
    }

    void
    register_adapter(const std::string& name,
                     std::unique_ptr<IndexTestAdapter> adapter,
                     const IndexTestParams& params) {
        entries_[name] = Entry{params, std::move(adapter)};
    }

    const IndexTestParams*
    params_for(const std::string& name) const {
        auto it = entries_.find(name);
        return it != entries_.end() ? &it->second.params : nullptr;
    }

    IndexTestAdapter*
    adapter_for(const std::string& name) {
        auto it = entries_.find(name);
        return it != entries_.end() ? it->second.adapter.get() : nullptr;
    }

    template <typename F>
    void
    for_each_index(F&& fn) {
        for (auto& [name, entry] : entries_) {
            fn(name, entry.params, *entry.adapter);
        }
    }

    size_t
    size() const {
        return entries_.size();
    }

private:
    IndexTestRegistry() = default;
    std::unordered_map<std::string, Entry> entries_;
};

}  // namespace fixtures
