
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

#include <cstdint>

namespace vsag {
namespace generic {
float
SQ4UniformComputeCodesIP(const uint8_t* codes1, const uint8_t* codes2, uint64_t dim);
}  // namespace generic

#if defined(ENABLE_SSE)
namespace sse {
float
SQ4UniformComputeCodesIP(const uint8_t* codes1, const uint8_t* codes2, uint64_t dim);
}  // namespace sse
#endif

#if defined(ENABLE_AVX2)
namespace avx2 {
float
SQ4UniformComputeCodesIP(const uint8_t* codes1, const uint8_t* codes2, uint64_t dim);
}  // namespace avx2
#endif

#if defined(ENABLE_AVX512)
namespace avx512 {
float
SQ4UniformComputeCodesIP(const uint8_t* codes1, const uint8_t* codes2, uint64_t dim);
}  // namespace avx512
#endif

using SQ4UniformComputeCodesType = float (*)(const uint8_t* codes1,
                                             const uint8_t* codes2,
                                             uint64_t dim);
extern SQ4UniformComputeCodesType SQ4UniformComputeCodesIP;
}  // namespace vsag