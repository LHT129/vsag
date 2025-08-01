
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

#include "sparse_term_datacell.h"

namespace vsag {

void
SparseTermDataCell::Query(float* global_dists,
                          const SparseTermComputerPtr& computer,
                          const bool only_collect_id) const {
    while (computer->HasNextTerm()) {
        auto it = computer->NextTermIter();
        auto term = computer->GetTerm(it);
        if (computer->HasNextTerm()) {
            auto next_it = it + 1;
            auto next_term = computer->GetTerm(next_it);
            if (next_term >= term_ids_.size()) {
                continue;
            }
            __builtin_prefetch(term_ids_[next_term].data(), 0, 3);
            __builtin_prefetch(term_datas_[next_term].data(), 0, 3);
        }
        if (term >= term_ids_.size()) {
            continue;
        }
        computer->ScanForAccumulate(it,
                                    term_ids_[term].data(),
                                    term_datas_[term].data(),
                                    term_pruned_sizes_[term],
                                    global_dists);
    }
    computer->ResetTerm();
}

template <InnerSearchMode mode>
void
SparseTermDataCell::InsertHeap(float* dists,
                               const SparseTermComputerPtr& computer,
                               MaxHeap& heap,
                               const InnerSearchParam& param,
                               uint32_t offset_id) const {
    uint32_t id = 0;
    float cur_heap_top = std::numeric_limits<float>::max();
    auto n_candidate = param.ef;
    auto radius = param.radius;

    if constexpr (mode == InnerSearchMode::RANGE_SEARCH) {
        // note that radius = 1 - ip -> radius - 1 = 0 - ip
        // the dist in heap is equal to 0 - ip
        // thus, we need to compare dist with radius - 1
        cur_heap_top = radius - 1;
    }

    while (computer->HasNextTerm()) {
        auto it = computer->NextTermIter();
        auto term = computer->GetTerm(it);
        if (term >= term_ids_.size()) {
            continue;
        }

        uint32_t i = 0;
        if constexpr (mode == InnerSearchMode::KNN_SEARCH) {
            if (heap.size() < n_candidate) {
                for (; i < term_pruned_sizes_[term]; i++) {
                    id = term_ids_[term][i];

                    heap.emplace(dists[id], id + offset_id);
                    cur_heap_top = heap.top().first;
                    dists[id] = 0;

                    if (heap.size() == n_candidate) {
                        break;
                    }
                }
            }
        }

        for (; i < term_pruned_sizes_[term]; i++) {
            id = term_ids_[term][i];

            if (dists[id] >= cur_heap_top) [[likely]] {
                dists[id] = 0;
                continue;
            } else {
                heap.emplace(dists[id], id + offset_id);
            }
            if constexpr (mode == InnerSearchMode::KNN_SEARCH) {
                if (heap.size() > n_candidate) [[likely]] {
                    heap.pop();
                }
                cur_heap_top = heap.top().first;
            }
            if constexpr (mode == InnerSearchMode::RANGE_SEARCH) {
                cur_heap_top = radius - 1;
            }
            dists[id] = 0;
        }
    }
    computer->ResetTerm();
}

SparseTermComputerPtr
SparseTermDataCell::FactoryComputer(const SparseVector& sparse_query) {
    auto computer = std::make_shared<SparseTermComputer>(query_prune_ratio_, allocator_);
    computer->SetQuery(sparse_query);
    return computer;
}

void
SparseTermDataCell::TermPrune() {
    // use this function after all vectors have been inserted
    for (auto i = 0; i < term_sizes_.size(); i++) {
        if (term_sizes_[i] <= 1) {
            term_pruned_sizes_[i] = term_sizes_[i];
        } else {
            term_pruned_sizes_[i] =
                static_cast<uint32_t>(static_cast<float>(term_sizes_[i]) * term_prune_ratio_);
        }
    }
}

void
SparseTermDataCell::DocPrune(Vector<std::pair<uint32_t, float>>& sorted_base) const {
    // use this function when inserting
    if (sorted_base.size() <= 1) {
        return;
    }
    auto pruned_doc_len =
        static_cast<uint32_t>(static_cast<float>(sorted_base.size()) * doc_prune_ratio_);
    sorted_base.resize(pruned_doc_len);
}

void
SparseTermDataCell::InsertVector(const SparseVector& sparse_base, uint32_t base_id) {
    // resize term
    uint32_t max_term_id = 0;
    for (auto i = 0; i < sparse_base.len_; i++) {
        auto term_id = sparse_base.ids_[i];
        max_term_id = std::max(max_term_id, term_id);
    }
    ResizeTermList(max_term_id + 1);

    Vector<std::pair<uint32_t, float>> sorted_base(allocator_);
    sort_sparse_vector(sparse_base, sorted_base);

    // doc prune
    DocPrune(sorted_base);

    // insert vector
    for (auto& item : sorted_base) {
        auto term = item.first;
        auto val = item.second;
        term_ids_[term].push_back(base_id);
        term_datas_[term].push_back(val);
        term_sizes_[term] += 1;
    }
}

void
SparseTermDataCell::ResizeTermList(InnerIdType new_term_capacity) {
    if (new_term_capacity <= this->term_capacity_) {
        return;
    }
    this->term_capacity_ = new_term_capacity;
    term_ids_.resize(term_capacity_, Vector<uint32_t>(allocator_));
    term_datas_.resize(term_capacity_, Vector<float>(allocator_));
    term_sizes_.resize(term_capacity_, 0);
    term_pruned_sizes_.resize(term_capacity_, 0);
}

void
SparseTermDataCell::Serialize(StreamWriter& writer) const {
    StreamWriter::WriteObj(writer, term_capacity_);
    for (auto i = 0; i < term_capacity_; i++) {
        StreamWriter::WriteVector(writer, term_ids_[i]);
        StreamWriter::WriteVector(writer, term_datas_[i]);
    }
    StreamWriter::WriteVector(writer, term_sizes_);
    StreamWriter::WriteVector(writer, term_pruned_sizes_);
}

void
SparseTermDataCell::Deserialize(StreamReader& reader) {
    uint32_t term_capacity;
    StreamReader::ReadObj(reader, term_capacity);
    ResizeTermList(term_capacity);
    for (auto i = 0; i < term_capacity_; i++) {
        StreamReader::ReadVector(reader, term_ids_[i]);
        StreamReader::ReadVector(reader, term_datas_[i]);
    }
    StreamReader::ReadVector(reader, term_sizes_);
    StreamReader::ReadVector(reader, term_pruned_sizes_);
}

template void
SparseTermDataCell::InsertHeap<InnerSearchMode::KNN_SEARCH>(float* dists,
                                                            const SparseTermComputerPtr& computer,
                                                            MaxHeap& heap,
                                                            const InnerSearchParam& param,
                                                            uint32_t offset_id) const;

template void
SparseTermDataCell::InsertHeap<InnerSearchMode::RANGE_SEARCH>(float* dists,
                                                              const SparseTermComputerPtr& computer,
                                                              MaxHeap& heap,
                                                              const InnerSearchParam& param,
                                                              uint32_t offset_id) const;

}  // namespace vsag
