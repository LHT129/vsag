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

// DARTH declarative-recall early termination (POC, scope B).
// A learned GBDT recall predictor invoked at adaptive intervals during the
// HGraph base-layer search; stops once predicted recall >= target recall.
//
// The model is a C++ object that cannot pass through the JSON KnnSearch API,
// so DARTH state is carried in a thread-local session that the harness sets
// before each search and the search loop reads via the checkpoint hook.

#include <cstdint>
#include <string>
#include <vector>

#include "impl/heap/distance_heap.h"
#include "utils/pointer_define.h"

namespace vsag {
class LabelTable;
}  // namespace vsag

namespace vsag::darth {

// 11 features, in the paper's order (Table 1).
struct Features {
    float n_step{0};       // index: search hops at base layer
    float n_dis{0};        // index: number of distance calculations
    float n_inserts{0};    // index: number of result-set inserts
    float first_nn{0};     // nn-distance: distance of first NN found
    float closest_nn{0};   // nn-distance: current closest NN distance
    float furthest_nn{0};  // nn-distance: current furthest (k-th) NN distance
    float avg{0};          // nn-stats: mean of result-set distances
    float var{0};          // nn-stats: variance
    float med{0};          // nn-stats: median
    float p25{0};          // nn-stats: 25th percentile
    float p75{0};          // nn-stats: 75th percentile

    void
    ToArray(float out[11]) const;
};

// Minimal GBDT (regression tree ensemble) evaluator. Loads a model serialized
// by the offline LightGBM training script (custom compact text format).
class GbdtPredictor {
public:
    bool
    LoadFromFile(const std::string& path);

    bool
    Loaded() const {
        return loaded_;
    }

    // Predict current recall from the 11 features.
    float
    Predict(const float feats[11]) const;

private:
    struct Node {
        int feature;        // split feature index; -1 for leaf
        float threshold;    // split threshold (<= goes left)
        int left;           // child node index
        int right;          // child node index
        float leaf_value;   // value if leaf
        bool default_left;  // direction for nan (unused; no nan here)
    };
    struct Tree {
        std::vector<Node> nodes;  // nodes[0] is root
    };
    std::vector<Tree> trees_;
    double init_score_{0.0};
    bool loaded_{false};
};

enum class Mode { kOff = 0, kLog = 1, kPredict = 2 };

// A training-data log row: features + the true recall at that checkpoint.
struct LogRow {
    float feats[11];
    float true_recall;
};

// Thread-local DARTH session. The harness configures it per query before
// calling KnnSearch; the search loop reads it through the checkpoint hook.
struct Session {
    Mode mode{Mode::kOff};

    // --- predict mode ---
    const GbdtPredictor* model{nullptr};
    float target_recall{0.0F};
    uint32_t ipi{0};            // initial / max prediction interval (in distance calcs)
    uint32_t mpi{0};            // minimum prediction interval
    float safety_margin{0.0F};  // require rp >= Rt + margin to stop (lowers RQUT)

    // --- log mode ---
    // Ground-truth inner ids for the current query (to measure true recall).
    const InnerIdType* gt_ids{nullptr};
    uint32_t gt_count{0};
    uint32_t log_every{1};      // log a sample every N distance calcs
    std::vector<LogRow>* sink;  // training rows are appended here

    // --- per-query mutable runtime state (reset by Begin) ---
    uint32_t next_check{0};  // next dist_cmp at which to act
    float first_nn{-1.0F};   // captured at first result insert
    uint32_t n_inserts{0};   // result-set insert counter
    bool terminated{false};  // predict-mode: target reached

    void
    Begin();
};

// Returns the thread-local session (created on first use).
Session&
session();

// Called from the search loop after each distance-calc batch.
// - returns true  => the loop should early-terminate
// - returns false => keep searching
// `result_heap` is the current top-k result set; `dist_cmp`/`hops` are counters.
bool
checkpoint(uint32_t dist_cmp,
           uint32_t hops,
           const DistanceHeap* result_heap,
           uint64_t topk,
           const LabelTable* label_table);

}  // namespace vsag::darth
