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

#include <memory>
#include <string>

#include "vsag/dataset.h"
#include "vsag/errors.h"
#include "vsag/expected.hpp"

namespace vsag {

/**
 * @brief Continue-search session produced by creating a SearchSession from an index.
 *
 * Owns the immutable query specification, search state, and lifecycle of a
 * continuing similarity search.  After OpenSearchSession the caller advances
 * the search with Next to receive non-overlapping candidate batches until
 * exhausted or closed.  Does not extend IteratorContext; existing iterator
 * APIs remain unchanged. The query is deep-copied and the backend is retained.
 * Sessions are single-consumer: do not call Next/HasMore/Close concurrently,
 * mutate the index, or mutate the retained filter while a session is open.
 * Exhaustion means the retained approximate search scope is exhausted, not
 * that every vector in the index has been enumerated.
 */
class SearchSession {
public:
    virtual ~SearchSession() = default;

    /**
     * @brief Advance the session and return the next batch of candidates.
     *
     * @param max_candidates  upper bound on returned candidate count.
     * @return a Dataset of at most max_candidates results in output-score order;
     *         empty when no more candidates exist or session is terminal.
     */
    [[nodiscard]] virtual tl::expected<DatasetPtr, Error>
    Next(uint64_t max_candidates) = 0;

    /**
     * @brief Whether the session has at least one pending candidate.
     *
     * true  → Next may return a non-empty batch（not guaranteed）.
     * false → Next is guaranteed to return empty; the search scope is exhausted.
     */
    [[nodiscard]] virtual bool
    HasMore() const noexcept = 0;

    /**
     * @brief Idempotent close.  After close, HasMore() returns false and
     *        Next returns empty without executing search work.  Previously
     *        returned results remain valid.
     */
    virtual void
    Close() noexcept = 0;
};

}  // namespace vsag
