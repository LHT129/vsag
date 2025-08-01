
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

#include "sparse_index_parameters.h"

#include "inner_string_params.h"

namespace vsag {

void
SparseIndexParameters::FromJson(const JsonType& json) {
    if (json.contains(SPARSE_NEED_SORT)) {
        need_sort = json[SPARSE_NEED_SORT];
    }
}

JsonType
SparseIndexParameters::ToJson() const {
    JsonType json;
    json[SPARSE_NEED_SORT] = need_sort;
    return json;
}

}  // namespace vsag
