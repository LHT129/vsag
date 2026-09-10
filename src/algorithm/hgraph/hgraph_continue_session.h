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

#include "impl/filter/iterator_filter.h"
#include "vsag/filter.h"
#include "vsag/search_session.h"

namespace vsag {
class HGraph;
class InnerIndexInterface;

class HGraphContinueSession final : public SearchSession {
public:
    HGraphContinueSession(const HGraph& hgraph,
                          std::shared_ptr<const InnerIndexInterface> owner,
                          DatasetPtr query,
                          std::string parameters,
                          FilterPtr filter,
                          Allocator* allocator);
    ~HGraphContinueSession() override;
    HGraphContinueSession(const HGraphContinueSession&) = delete;
    HGraphContinueSession&
    operator=(const HGraphContinueSession&) = delete;

    void
    Initialize(int64_t k);
    tl::expected<DatasetPtr, Error>
    Next(uint64_t max_candidates) override;
    bool
    HasMore() const noexcept override;
    void
    Close() noexcept override;

private:
    // Strong backend ownership keeps graph state and its allocator alive.
    std::shared_ptr<const HGraph> hgraph_;
    DatasetPtr query_;
    std::string parameters_;
    FilterPtr filter_;
    Allocator* allocator_;
    // Created only by the legacy iterator overload; exclusively owned by this session.
    IteratorContext* iter_ctx_{nullptr};
    DatasetPtr pending_;
    int64_t pending_offset_{0};
    bool closed_{false};
};
}  // namespace vsag
