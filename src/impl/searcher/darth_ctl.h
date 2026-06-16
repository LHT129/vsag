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

// DARTH control shim (POC) — primitive-typed API usable by an external harness
// without pulling in internal vsag headers. All functions operate on the
// calling thread's DARTH session.

#include <cstdint>

namespace vsag::darth_ctl {

// Load a serialized GBDT model into a process-global predictor; returns true
// on success. The model is owned internally and reused across queries.
bool
LoadModel(const char* path);

// Configure the current thread for LOG mode (training-data collection).
// gt_ids/gt_count describe the query's ground-truth inner ids; rows are
// accumulated internally and retrieved via DumpLog.
void
BeginLog(const int64_t* gt_ids, uint32_t gt_count, uint32_t log_every);

// Configure the current thread for PREDICT mode using the loaded model.
void
BeginPredict(float target_recall, uint32_t ipi, uint32_t mpi, float safety_margin);

// Turn DARTH off for the current thread (default).
void
Off();

// Whether the most recent PREDICT-mode search ended via early termination.
bool
WasTerminated();

// Number of accumulated training rows (across all BeginLog queries).
uint64_t
LogRowCount();

// Copy accumulated training rows into caller buffers (row-major features
// [count*11] + recalls [count]); returns rows written (<= max_rows). Clears
// the internal buffer afterwards.
uint64_t
DumpLog(float* feats_out, float* recall_out, uint64_t max_rows);

}  // namespace vsag::darth_ctl
