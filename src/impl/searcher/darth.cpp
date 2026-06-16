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

#include "darth.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>

#include "darth_ctl.h"
#include "impl/label_table/label_table.h"

namespace vsag::darth {

void
Features::ToArray(float out[11]) const {
    out[0] = n_step;
    out[1] = n_dis;
    out[2] = n_inserts;
    out[3] = first_nn;
    out[4] = closest_nn;
    out[5] = furthest_nn;
    out[6] = avg;
    out[7] = var;
    out[8] = med;
    out[9] = p25;
    out[10] = p75;
}

// ---------------- GBDT evaluator ----------------
//
// Model text format (one token stream), produced by the training script:
//   init <init_score>
//   tree <num_nodes>
//   <node_id> L <leaf_value>
//   <node_id> S <feature> <threshold> <left> <right>
//   ... repeated per node ...
//   tree <num_nodes>
//   ...
// Nodes are listed in id order; node 0 is the root of each tree.

bool
GbdtPredictor::LoadFromFile(const std::string& path) {
    std::ifstream in(path);
    if (not in.is_open()) {
        return false;
    }
    trees_.clear();
    init_score_ = 0.0;
    std::string tok;
    Tree* cur = nullptr;
    while (in >> tok) {
        if (tok == "init") {
            in >> init_score_;
        } else if (tok == "tree") {
            int n = 0;
            in >> n;
            trees_.emplace_back();
            cur = &trees_.back();
            cur->nodes.resize(n);
        } else {
            // tok is a node id
            int id = std::stoi(tok);
            std::string kind;
            in >> kind;
            Node& nd = cur->nodes[id];
            if (kind == "L") {
                in >> nd.leaf_value;
                nd.feature = -1;
                nd.left = nd.right = -1;
            } else {  // "S"
                in >> nd.feature >> nd.threshold >> nd.left >> nd.right;
                nd.leaf_value = 0.0F;
                nd.default_left = true;
            }
        }
    }
    loaded_ = not trees_.empty();
    return loaded_;
}

float
GbdtPredictor::Predict(const float feats[11]) const {
    double sum = init_score_;
    for (const auto& tree : trees_) {
        int idx = 0;
        // walk to a leaf
        while (tree.nodes[idx].feature >= 0) {
            const Node& nd = tree.nodes[idx];
            if (feats[nd.feature] <= nd.threshold) {
                idx = nd.left;
            } else {
                idx = nd.right;
            }
        }
        sum += tree.nodes[idx].leaf_value;
    }
    if (sum < 0.0) {
        sum = 0.0;
    }
    if (sum > 1.0) {
        sum = 1.0;
    }
    return static_cast<float>(sum);
}

// ---------------- Session ----------------

void
Session::Begin() {
    next_check = (mode == Mode::kPredict) ? ipi : log_every;
    first_nn = -1.0F;
    n_inserts = 0;
    terminated = false;
}

Session&
session() {
    static thread_local Session s;
    return s;
}

// ---------------- feature extraction ----------------

static void
compute_features(Features& f,
                 uint32_t dist_cmp,
                 uint32_t hops,
                 const DistanceHeap* heap,
                 uint64_t topk,
                 Session& s) {
    f.n_step = static_cast<float>(hops);
    f.n_dis = static_cast<float>(dist_cmp);
    f.n_inserts = static_cast<float>(s.n_inserts);
    f.first_nn = s.first_nn < 0 ? 0.0F : s.first_nn;

    const uint64_t n = heap->Size();
    const auto* data = heap->GetData();  // contiguous DistanceRecord buffer
    if (n == 0) {
        f.closest_nn = f.furthest_nn = f.avg = f.var = f.med = f.p25 = f.p75 = 0.0F;
        return;
    }
    // Scan the result-set distances once (O(k)); copy for percentile sorting.
    static thread_local std::vector<float> buf;
    buf.clear();
    buf.reserve(n);
    double sum = 0.0;
    double sum2 = 0.0;
    float mn = data[0].first;
    float mx = data[0].first;
    for (uint64_t i = 0; i < n; i++) {
        float d = data[i].first;
        buf.push_back(d);
        sum += d;
        sum2 += static_cast<double>(d) * d;
        mn = std::min(mn, d);
        mx = std::max(mx, d);
    }
    double mean = sum / static_cast<double>(n);
    f.closest_nn = mn;
    f.furthest_nn = mx;
    f.avg = static_cast<float>(mean);
    f.var = static_cast<float>(sum2 / static_cast<double>(n) - mean * mean);
    std::sort(buf.begin(), buf.end());
    auto pct = [&](double p) -> float {
        if (n == 1) {
            return buf[0];
        }
        double idx = p * static_cast<double>(n - 1);
        auto lo = static_cast<size_t>(idx);
        double frac = idx - static_cast<double>(lo);
        if (lo + 1 < n) {
            return static_cast<float>(buf[lo] * (1.0 - frac) + buf[lo + 1] * frac);
        }
        return buf[lo];
    };
    f.med = pct(0.5);
    f.p25 = pct(0.25);
    f.p75 = pct(0.75);
}

// true recall so far = |result-set inner ids ∩ ground-truth| / topk
static float
true_recall(const DistanceHeap* heap,
            uint64_t topk,
            const Session& s,
            const LabelTable* label_table) {
    if (s.gt_count == 0) {
        return 0.0F;
    }
    // Recall is over the user's k nearest (== s.gt_count), NOT the ef-sized heap.
    // The result heap may hold up to ef entries; take the k closest by distance.
    const uint64_t n = heap->Size();
    const auto* data = heap->GetData();
    const uint64_t k = s.gt_count;
    // collect (distance, label) and partial-sort to the k smallest distances
    static thread_local std::vector<std::pair<float, int64_t>> items;
    items.clear();
    items.reserve(n);
    for (uint64_t i = 0; i < n; i++) {
        int64_t label = label_table != nullptr
                            ? static_cast<int64_t>(label_table->GetLabelById(data[i].second))
                            : static_cast<int64_t>(data[i].second);
        items.emplace_back(data[i].first, label);
    }
    uint64_t take = items.size() < k ? items.size() : k;
    std::partial_sort(
        items.begin(), items.begin() + take, items.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
    std::set<int64_t> gt(s.gt_ids, s.gt_ids + s.gt_count);
    uint32_t hit = 0;
    for (uint64_t i = 0; i < take; i++) {
        if (gt.count(items[i].second) != 0U) {
            ++hit;
        }
    }
    return static_cast<float>(hit) / static_cast<float>(k);
}

bool
checkpoint(uint32_t dist_cmp,
           uint32_t hops,
           const DistanceHeap* result_heap,
           uint64_t topk,
           const LabelTable* label_table) {
    Session& s = session();
    if (s.mode == Mode::kOff) {
        return false;
    }
    if (dist_cmp < s.next_check) {
        return false;
    }

    Features f;
    compute_features(f, dist_cmp, hops, result_heap, topk, s);

    if (s.mode == Mode::kLog) {
        LogRow row;
        f.ToArray(row.feats);
        row.true_recall = true_recall(result_heap, topk, s, label_table);
        if (s.sink != nullptr) {
            s.sink->push_back(row);
        }
        s.next_check = dist_cmp + s.log_every;
        return false;  // logging never terminates
    }

    // predict mode
    float arr[11];
    f.ToArray(arr);
    float rp = s.model != nullptr ? s.model->Predict(arr) : 0.0F;
    if (rp >= s.target_recall + s.safety_margin) {
        s.terminated = true;
        return true;  // early-terminate
    }
    // adaptive prediction interval: pi = mpi + (ipi - mpi) * (Rt - Rp)
    float gap = s.target_recall - rp;
    if (gap < 0) {
        gap = 0;
    }
    auto pi =
        static_cast<uint32_t>(static_cast<float>(s.mpi) + static_cast<float>(s.ipi - s.mpi) * gap);
    if (pi < s.mpi) {
        pi = s.mpi;
    }
    s.next_check = dist_cmp + pi;
    return false;
}

}  // namespace vsag::darth

// ---------------- control shim (primitive-typed, harness-facing) ----------------
namespace vsag::darth_ctl {

static vsag::darth::GbdtPredictor g_model;
static thread_local std::vector<vsag::darth::LogRow> g_log;
static thread_local std::vector<vsag::InnerIdType> g_gt;

bool
LoadModel(const char* path) {
    return g_model.LoadFromFile(path);
}

void
BeginLog(const int64_t* gt_ids, uint32_t gt_count, uint32_t log_every) {
    auto& s = vsag::darth::session();
    g_gt.assign(gt_ids, gt_ids + gt_count);
    s.mode = vsag::darth::Mode::kLog;
    s.gt_ids = g_gt.data();
    s.gt_count = gt_count;
    s.log_every = log_every == 0 ? 1 : log_every;
    s.sink = &g_log;
    s.Begin();
}

void
BeginPredict(float target_recall, uint32_t ipi, uint32_t mpi, float safety_margin) {
    auto& s = vsag::darth::session();
    s.mode = vsag::darth::Mode::kPredict;
    s.model = &g_model;
    s.target_recall = target_recall;
    s.ipi = ipi == 0 ? 1 : ipi;
    s.mpi = mpi == 0 ? 1 : mpi;
    s.safety_margin = safety_margin;
    s.Begin();
}

void
Off() {
    vsag::darth::session().mode = vsag::darth::Mode::kOff;
}

bool
WasTerminated() {
    return vsag::darth::session().terminated;
}

uint64_t
LogRowCount() {
    return g_log.size();
}

uint64_t
DumpLog(float* feats_out, float* recall_out, uint64_t max_rows) {
    uint64_t n = g_log.size() < max_rows ? g_log.size() : max_rows;
    for (uint64_t i = 0; i < n; i++) {
        for (int j = 0; j < 11; j++) {
            feats_out[i * 11 + j] = g_log[i].feats[j];
        }
        recall_out[i] = g_log[i].true_recall;
    }
    g_log.clear();
    return n;
}

}  // namespace vsag::darth_ctl

// Keep darth_ctl symbols in the shared library. They are only called from the
// external POC harness, so the linker would otherwise GC this translation unit
// out of libvsag.so. Referencing their addresses from a symbol that the search
// loop pulls in (via session()) forces retention. (POC-only scaffolding.)
namespace vsag::darth {
volatile void* g_darth_ctl_anchor[] = {
    reinterpret_cast<void*>(&vsag::darth_ctl::LoadModel),
    reinterpret_cast<void*>(&vsag::darth_ctl::BeginLog),
    reinterpret_cast<void*>(&vsag::darth_ctl::BeginPredict),
    reinterpret_cast<void*>(&vsag::darth_ctl::Off),
    reinterpret_cast<void*>(&vsag::darth_ctl::WasTerminated),
    reinterpret_cast<void*>(&vsag::darth_ctl::LogRowCount),
    reinterpret_cast<void*>(&vsag::darth_ctl::DumpLog),
};
const void*
darth_ctl_keepalive() {
    return reinterpret_cast<const void*>(g_darth_ctl_anchor);
}
}  // namespace vsag::darth
