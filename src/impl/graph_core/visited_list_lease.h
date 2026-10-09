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
#include <utility>

#include "utils/visited_list.h"

namespace vsag {

/**
 * @brief RAII lease over a VisitedListPool.
 *
 * Holding a VisitedList and returning it by hand is only safe while the guarded
 * scope cannot exit early: any escaping exception skips the ReturnOne call. The
 * pool is not a fixed-size free list -- TakeOne constructs a new VisitedList and
 * grows the pool's memory accounting whenever every sub-pool is empty -- so a
 * missed return grows memory monotonically instead of merely stalling a later
 * TakeOne. This guard returns the list from the destructor, so early returns and
 * exceptions both release it.
 *
 * Both HGraph and Pyramid lease visited lists for graph traversal; this is the
 * single implementation shared by them. The guarded list may be borrowed from the
 * caller instead of taken from the pool, which is how a search reuses one list
 * across several graphs.
 */
class VisitedListLease {
public:
    /// Take a fresh list from the pool. The pool must outlive the lease.
    explicit VisitedListLease(VisitedListPool* pool)
        : pool_(pool), list_(pool == nullptr ? nullptr : pool->TakeOne()) {
    }

    /// Borrow a caller-owned list. The lease resets it but never returns it.
    explicit VisitedListLease(VisitedListPtr borrowed)
        : pool_(nullptr), list_(std::move(borrowed)) {
        if (list_ != nullptr) {
            list_->Reset();
        }
    }

    ~VisitedListLease() {
        if (list_ != nullptr and pool_ != nullptr) {
            pool_->ReturnOne(list_);
        }
    }

    VisitedListLease(const VisitedListLease&) = delete;
    VisitedListLease&
    operator=(const VisitedListLease&) = delete;
    VisitedListLease(VisitedListLease&&) = delete;
    VisitedListLease&
    operator=(VisitedListLease&&) = delete;

    [[nodiscard]] const VisitedListPtr&
    Get() const {
        return list_;
    }

    [[nodiscard]] bool
    Empty() const {
        return list_ == nullptr;
    }

private:
    // Pool to return to; null when the list was borrowed rather than taken.
    VisitedListPool* pool_{nullptr};
    VisitedListPtr list_{nullptr};
};

}  // namespace vsag