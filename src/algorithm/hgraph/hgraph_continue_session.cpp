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

#include "algorithm/hgraph/hgraph_continue_session.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include "algorithm/hgraph/hgraph.h"
#include "utils/search_threshold.h"
#include "vsag_exception.h"

namespace vsag {

std::unique_ptr<SearchSession>
HGraph::OpenSearchSession(const DatasetPtr& query,
                          int64_t k_per_call,
                          const std::string& parameters,
                          const FilterPtr& filter,
                          Allocator* allocator,
                          std::shared_ptr<const InnerIndexInterface> owner) const {
    CHECK_ARGUMENT(owner != nullptr and owner.get() == this,
                   "session requires ownership of this index backend");
    CHECK_ARGUMENT(query != nullptr, "session query must not be null");
    this->validate_knn_args(query, k_per_call);
    auto params = HGraphSearchParameters::FromJson(parameters);
    CHECK_ARGUMENT(params.ef_search >= 1, "ef_search must be positive");
    (void)ParseSearchThreshold(parameters);
    auto* alloc = allocator == nullptr ? allocator_ : allocator;
    auto session = std::make_unique<HGraphContinueSession>(
        *this, std::move(owner), query->DeepCopy(alloc), parameters, filter, alloc);
    session->Initialize(k_per_call);
    return session;
}

HGraphContinueSession::HGraphContinueSession(const HGraph& hgraph,
                                             std::shared_ptr<const InnerIndexInterface> owner,
                                             DatasetPtr query,
                                             std::string parameters,
                                             FilterPtr filter,
                                             Allocator* allocator)
    : hgraph_(std::move(owner), &hgraph),
      query_(std::move(query)),
      parameters_(std::move(parameters)),
      filter_(std::move(filter)),
      allocator_(allocator) {
}

HGraphContinueSession::~HGraphContinueSession() {
    Close();
}

void
HGraphContinueSession::Initialize(int64_t k) {
    // Legacy search keeps excess candidates in discard and marks only this page delivered.
    pending_ = hgraph_->KnnSearch(query_, k, parameters_, filter_, allocator_, iter_ctx_, false);
}

void
HGraphContinueSession::Close() noexcept {
    closed_ = true;
    delete iter_ctx_;
    iter_ctx_ = nullptr;
    pending_.reset();
    query_.reset();
    filter_.reset();
    hgraph_.reset();
}

bool
HGraphContinueSession::HasMore() const noexcept {
    return not closed_ and
           ((pending_ != nullptr and pending_offset_ < pending_->GetDim()) or
            (iter_ctx_ != nullptr and not static_cast<IteratorFilterContext*>(iter_ctx_)->Empty()));
}

tl::expected<DatasetPtr, Error>
HGraphContinueSession::Next(uint64_t max_candidates) {
    try {
        if (not HasMore()) {
            return Dataset::Make()->NumElements(1)->Dim(0);
        }
        CHECK_ARGUMENT(
            max_candidates > 0 and
                max_candidates <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()),
            "max_candidates must be a positive int64");
        if (pending_ == nullptr or pending_offset_ == pending_->GetDim()) {
            pending_ = hgraph_->KnnSearch(query_,
                                          static_cast<int64_t>(max_candidates),
                                          parameters_,
                                          filter_,
                                          allocator_,
                                          iter_ctx_,
                                          false);
            pending_offset_ = 0;
        }
        const auto count =
            std::min(static_cast<int64_t>(max_candidates), pending_->GetDim() - pending_offset_);
        // Result storage must outlive both the session and its backend allocator.
        auto result = Dataset::Make()->Owner(true)->NumElements(1)->Dim(count);
        if (count > 0) {
            result->Ids(new int64_t[count]);
            result->Distances(new float[count]);
            std::copy_n(pending_->GetIds() + pending_offset_,
                        count,
                        const_cast<int64_t*>(result->GetIds()));
            std::copy_n(pending_->GetDistances() + pending_offset_,
                        count,
                        const_cast<float*>(result->GetDistances()));
            const auto extra_size = pending_->GetExtraInfoSize();
            if (extra_size > 0) {
                result->ExtraInfoSize(extra_size)->ExtraInfos(new char[count * extra_size]);
                std::memcpy(const_cast<char*>(result->GetExtraInfos()),
                            pending_->GetExtraInfos() + pending_offset_ * extra_size,
                            count * extra_size);
            }
        }
        result->Statistics(pending_->GetStatistics());
        pending_offset_ += count;
        return result;
    } catch (const VsagException& e) {
        Close();
        return tl::unexpected(e.error_);
    } catch (const std::bad_alloc& e) {
        Close();
        return tl::unexpected(Error(ErrorType::NO_ENOUGH_MEMORY, e.what()));
    } catch (const std::exception& e) {
        Close();
        return tl::unexpected(Error(ErrorType::INTERNAL_ERROR, e.what()));
    }
}
}  // namespace vsag
