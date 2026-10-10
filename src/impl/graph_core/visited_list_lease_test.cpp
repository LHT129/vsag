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

#include "impl/graph_core/visited_list_lease.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include "impl/allocator/default_allocator.h"
#include "unittest.h"

using namespace vsag;

namespace {

constexpr uint64_t kListSize = 1'000;
constexpr int kRounds = 32;

// The pool's memory_usage_ only ever grows (it is bumped when a list is
// constructed and never decremented on return), so it cannot witness a leak.
// A leaked list instead forces the next TakeOne to construct a fresh
// VisitedList, which shows up as allocation traffic. Counting allocations is
// therefore the observable proxy for "was the list returned".
//
// Note that the pool's free-list container does allocate a constant amount of
// churn as lists move in and out, so an exact zero-growth assertion would be
// wrong. The discriminating signal is the trend: a lease that returns its list
// settles at a small constant, while a list that is never returned costs one
// construction per iteration and the count tracks the iteration count.
class CountingAllocator : public DefaultAllocator {
public:
    void*
    Allocate(uint64_t size) override {
        allocate_count_.fetch_add(1, std::memory_order_relaxed);
        return DefaultAllocator::Allocate(size);
    }

    void
    Deallocate(void* p) override {
        DefaultAllocator::Deallocate(p);
    }

    uint64_t
    Allocations() const {
        return allocate_count_.load(std::memory_order_relaxed);
    }

    void
    Reset() {
        allocate_count_.store(0, std::memory_order_relaxed);
    }

private:
    std::atomic<uint64_t> allocate_count_{0};
};

/// Allocations incurred by running `body` `kRounds` times with the counter reset
/// beforehand.
template <typename Body>
uint64_t
MeasureRound(CountingAllocator& allocator, Body&& body) {
    allocator.Reset();
    body();
    return allocator.Allocations();
}

}  // namespace

TEST_CASE("VisitedListLease settles instead of growing across rounds", "[ut][VisitedListLease]") {
    CountingAllocator allocator;
    VisitedListPool pool(1, &allocator, kListSize, &allocator);
    // Let the pool's own containers and this thread's shard reach steady state.
    for (int i = 0; i < kRounds; ++i) {
        VisitedListLease lease(&pool);
    }

    const auto first = MeasureRound(allocator, [&] {
        for (int i = 0; i < kRounds; ++i) {
            VisitedListLease lease(&pool);
            REQUIRE_FALSE(lease.Empty());
            REQUIRE(lease.Get() != nullptr);
            // The list must be usable while leased.
            REQUIRE_FALSE(lease.Get()->TestAndSet(kListSize - 1));
        }
    });
    const auto second = MeasureRound(allocator, [&] {
        for (int i = 0; i < kRounds; ++i) {
            VisitedListLease lease(&pool);
        }
    });

    // Steady state: the second round costs no more than the first. A leaking
    // implementation would cost about one construction per iteration, i.e. the
    // allocation count would scale with kRounds rather than stay flat.
    REQUIRE(second <= first);
    REQUIRE(second < static_cast<uint64_t>(kRounds));
}

TEST_CASE("VisitedListLease returns the list when a non-std exception escapes",
          "[ut][VisitedListLease]") {
    CountingAllocator allocator;
    VisitedListPool pool(1, &allocator, kListSize, &allocator);
    for (int i = 0; i < kRounds; ++i) {
        VisitedListLease lease(&pool);
    }

    // A hand-written TakeOne/ReturnOne pair leaks exactly here: `throw 42` is not
    // caught by `catch (const std::exception&)`, so the return is skipped. With
    // RAII the list goes back regardless of how the scope exits.
    const auto cost = MeasureRound(allocator, [&] {
        for (int i = 0; i < kRounds; ++i) {
            try {
                VisitedListLease lease(&pool);
                throw 42;
            } catch (int) {
            }
        }
    });
    REQUIRE(cost < static_cast<uint64_t>(kRounds));
}

TEST_CASE("VisitedListLease returns the list when a std exception escapes",
          "[ut][VisitedListLease]") {
    CountingAllocator allocator;
    VisitedListPool pool(1, &allocator, kListSize, &allocator);
    for (int i = 0; i < kRounds; ++i) {
        VisitedListLease lease(&pool);
    }

    const auto cost = MeasureRound(allocator, [&] {
        for (int i = 0; i < kRounds; ++i) {
            try {
                VisitedListLease lease(&pool);
                throw std::runtime_error("search failed");
            } catch (const std::runtime_error&) {
            }
        }
    });
    REQUIRE(cost < static_cast<uint64_t>(kRounds));
}

TEST_CASE("A never-returned list costs one construction per iteration", "[ut][VisitedListLease]") {
    // Pins the discrimination the tests above rely on: if the pool returned
    // leaked lists for free, the growth assertions would be vacuous.
    CountingAllocator allocator;
    VisitedListPool pool(1, &allocator, kListSize, &allocator);

    std::vector<VisitedListPtr> held;
    held.reserve(kRounds);
    const auto cost = MeasureRound(allocator, [&] {
        for (int i = 0; i < kRounds; ++i) {
            held.push_back(pool.TakeOne());
        }
    });
    // The pool is seeded with exactly one list, so a thread that hashes to the
    // seeded sub-pool reuses it on its first take and constructs only kRounds - 1
    // times. Allowing that single reuse keeps the guard honest: it still fails if
    // construction ever stops being per-iteration, which is what makes the growth
    // assertions above meaningful.
    REQUIRE(cost >= static_cast<uint64_t>(kRounds) - 1);

    for (auto& list : held) {
        pool.ReturnOne(list);
    }
}

TEST_CASE("VisitedListLease returns concurrently taken lists", "[ut][VisitedListLease]") {
    CountingAllocator allocator;
    VisitedListPool pool(1, &allocator, kListSize, &allocator);

    {
        VisitedListLease first(&pool);
        // A second concurrent lease cannot share the first list.
        VisitedListLease second(&pool);
        REQUIRE(first.Get() != second.Get());
    }
    // Both lists are back, so a later round reuses them instead of growing.
    for (int i = 0; i < kRounds; ++i) {
        VisitedListLease lease(&pool);
    }
    const auto first = MeasureRound(allocator, [&] {
        for (int i = 0; i < kRounds; ++i) {
            VisitedListLease lease(&pool);
        }
    });
    const auto second = MeasureRound(allocator, [&] {
        for (int i = 0; i < kRounds; ++i) {
            VisitedListLease lease(&pool);
        }
    });
    REQUIRE(second <= first);
}

TEST_CASE("VisitedListLease borrows a caller-owned list without returning it",
          "[ut][VisitedListLease]") {
    CountingAllocator allocator;
    VisitedListPool pool(1, &allocator, kListSize, &allocator);

    auto borrowed = pool.TakeOne();
    for (int i = 0; i < kRounds; ++i) {
        VisitedListLease lease(&pool);
    }

    // A borrowed lease must not push the caller's list back into the pool: the
    // caller still owns it, so steady-state churn must not grow.
    const auto first = MeasureRound(allocator, [&] {
        for (int i = 0; i < kRounds; ++i) {
            VisitedListLease lease(borrowed);
        }
    });
    const auto second = MeasureRound(allocator, [&] {
        for (int i = 0; i < kRounds; ++i) {
            VisitedListLease lease(borrowed);
        }
    });
    REQUIRE(first == 0);
    REQUIRE(second == 0);

    pool.ReturnOne(borrowed);
}

TEST_CASE("VisitedListLease resets a borrowed list", "[ut][VisitedListLease]") {
    CountingAllocator allocator;
    VisitedListPool pool(1, &allocator, kListSize, &allocator);

    auto borrowed = pool.TakeOne();
    // Mark a slot so we can observe that the lease reset the list.
    REQUIRE_FALSE(borrowed->TestAndSet(7));
    {
        VisitedListLease lease(borrowed);
        REQUIRE(lease.Get() == borrowed);
        // Borrowing resets the list, so the previously marked slot is clear.
        REQUIRE_FALSE(lease.Get()->TestAndSet(7));
    }
    pool.ReturnOne(borrowed);
}

TEST_CASE("VisitedListLease tolerates a null pool", "[ut][VisitedListLease]") {
    VisitedListLease lease(static_cast<VisitedListPool*>(nullptr));
    REQUIRE(lease.Empty());
    REQUIRE(lease.Get() == nullptr);
}