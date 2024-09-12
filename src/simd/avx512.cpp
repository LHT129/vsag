
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

#include <avx512fintrin.h>
#include <x86intrin.h>

#include "fp32_simd.h"
#include "sq8_simd.h"
namespace vsag {

#define PORTABLE_ALIGN32 __attribute__((aligned(32)))
#define PORTABLE_ALIGN64 __attribute__((aligned(64)))

float
L2SqrSIMD16ExtAVX512(const void* pVect1v, const void* pVect2v, const void* qty_ptr) {
    float* pVect1 = (float*)pVect1v;
    float* pVect2 = (float*)pVect2v;
    size_t qty = *((size_t*)qty_ptr);
    float PORTABLE_ALIGN64 TmpRes[16];
    size_t qty16 = qty >> 4;

    const float* pEnd1 = pVect1 + (qty16 << 4);

    __m512 diff, v1, v2;
    __m512 sum = _mm512_set1_ps(0);

    while (pVect1 < pEnd1) {
        v1 = _mm512_loadu_ps(pVect1);
        pVect1 += 16;
        v2 = _mm512_loadu_ps(pVect2);
        pVect2 += 16;
        diff = _mm512_sub_ps(v1, v2);
        // sum = _mm512_fmadd_ps(diff, diff, sum);
        sum = _mm512_add_ps(sum, _mm512_mul_ps(diff, diff));
    }

    _mm512_store_ps(TmpRes, sum);
    float res = TmpRes[0] + TmpRes[1] + TmpRes[2] + TmpRes[3] + TmpRes[4] + TmpRes[5] + TmpRes[6] +
                TmpRes[7] + TmpRes[8] + TmpRes[9] + TmpRes[10] + TmpRes[11] + TmpRes[12] +
                TmpRes[13] + TmpRes[14] + TmpRes[15];

    return (res);
}

float
InnerProductSIMD16ExtAVX512(const void* pVect1v, const void* pVect2v, const void* qty_ptr) {
    float PORTABLE_ALIGN64 TmpRes[16];
    float* pVect1 = (float*)pVect1v;
    float* pVect2 = (float*)pVect2v;
    size_t qty = *((size_t*)qty_ptr);

    size_t qty16 = qty / 16;

    const float* pEnd1 = pVect1 + 16 * qty16;

    __m512 sum512 = _mm512_set1_ps(0);

    while (pVect1 < pEnd1) {
        //_mm_prefetch((char*)(pVect2 + 16), _MM_HINT_T0);

        __m512 v1 = _mm512_loadu_ps(pVect1);
        pVect1 += 16;
        __m512 v2 = _mm512_loadu_ps(pVect2);
        pVect2 += 16;
        sum512 = _mm512_add_ps(sum512, _mm512_mul_ps(v1, v2));
    }

    _mm512_store_ps(TmpRes, sum512);
    float sum = TmpRes[0] + TmpRes[1] + TmpRes[2] + TmpRes[3] + TmpRes[4] + TmpRes[5] + TmpRes[6] +
                TmpRes[7] + TmpRes[8] + TmpRes[9] + TmpRes[10] + TmpRes[11] + TmpRes[12] +
                TmpRes[13] + TmpRes[14] + TmpRes[15];

    return sum;
}

namespace AVX512 {
float
FP32ComputeIP(const float* query, const float* codes, uint64_t dim) {
#if defined(ENABLE_AVX512)
    const int n = dim / 16;            // process 16 floats at a time
    __m512 sum = _mm512_setzero_ps();  // initialize to 0
    for (int i = 0; i < n; ++i) {
        __m512 a = _mm512_loadu_ps(query + i * 16);     // load 16 floats from memory
        __m512 b = _mm512_loadu_ps(codes + i * 16);     // load 16 floats from memory
        sum = _mm512_add_ps(sum, _mm512_mul_ps(a, b));  // accumulate the product
    }
    float ip = _mm512_reduce_add_ps(sum);
    ip += AVX2::FP32ComputeIP(query + n * 16, codes + n * 16, dim - n * 16);
    return ip;
#else
    return vsag::Generic::FP32ComputeIP(query, codes, dim);
#endif
}

float
FP32ComputeL2Sqr(const float* query, const float* codes, uint64_t dim) {
#if defined(ENABLE_AVX512)
    const int n = dim / 16;            // process 16 floats at a time
    __m512 sum = _mm512_setzero_ps();  // initialize to 0
    for (int i = 0; i < n; ++i) {
        __m512 a = _mm512_loadu_ps(query + i * 16);  // load 16 floats from memory
        __m512 b = _mm512_loadu_ps(codes + i * 16);  // load 16 floats from memory
        __m512 diff = _mm512_sub_ps(a, b);           // calculate the difference
        sum = _mm512_fmadd_ps(diff, diff, sum);      // accumulate the squared difference
    }
    float l2 = _mm512_reduce_add_ps(sum);
    l2 += AVX2::FP32ComputeL2Sqr(query + n * 16, codes + n * 16, dim - n * 16);
    return l2;
#else
    return vsag::Generic::FP32ComputeL2Sqr(query, codes, dim);
#endif
}

float
SQ8ComputeIP(const float* query,
             const uint8_t* codes,
             const float* lowerBound,
             const float* diff,
             uint64_t dim) {
#if defined(ENABLE_AVX512)
    // Initialize the sum to 0
    __m512 sum = _mm512_setzero_ps();
    uint64_t i = 0;

    // Process the data in 512-bit chunks
    for (; i + 15 < dim; i += 16) {
        // Load data into registers
        __m128i code_values = _mm_loadu_si128(reinterpret_cast<const __m128i*>(codes + i));
        __m512i codes_512 = _mm512_cvtepu8_epi32(code_values);
        __m512 code_floats = _mm512_cvtepi32_ps(codes_512);
        __m512 query_values = _mm512_loadu_ps(query + i);
        __m512 diff_values = _mm512_loadu_ps(diff + i);
        __m512 lowerBound_values = _mm512_loadu_ps(lowerBound + i);

        // Perform calculations
        __m512 scaled_codes =
            _mm512_mul_ps(_mm512_div_ps(code_floats, _mm512_set1_ps(255.0f)), diff_values);
        __m512 adjusted_codes = _mm512_add_ps(scaled_codes, lowerBound_values);
        __m512 val = _mm512_mul_ps(query_values, adjusted_codes);
        sum = _mm512_add_ps(sum, val);
    }
    // Horizontal addition
    float finalResult = _mm512_reduce_add_ps(sum);
    // Process the remaining elements recursively
    finalResult += AVX2::SQ8ComputeIP(query + i, codes + i, lowerBound + i, diff + i, dim - i);
    return finalResult;
#else
    return Generic::SQ8ComputeIP(query, codes, lowerBound, diff, dim);
#endif
}

float
SQ8ComputeL2Sqr(const float* query,
                const uint8_t* codes,
                const float* lowerBound,
                const float* diff,
                uint64_t dim) {
#if defined(ENABLE_AVX512)
    __m512 sum = _mm512_setzero_ps();
    uint64_t i = 0;

    for (; i + 15 < dim; i += 16) {
        // Load data into registers
        __m128i code_values = _mm_loadu_si128(reinterpret_cast<const __m128i*>(codes + i));
        __m512i codes_512 = _mm512_cvtepu8_epi32(code_values);
        __m512 code_floats = _mm512_div_ps(_mm512_cvtepi32_ps(codes_512), _mm512_set1_ps(255.0f));
        __m512 diff_values = _mm512_loadu_ps(diff + i);
        __m512 lowerBound_values = _mm512_loadu_ps(lowerBound + i);
        __m512 query_values = _mm512_loadu_ps(query + i);

        // Perform calculations
        __m512 scaled_codes = _mm512_mul_ps(code_floats, diff_values);
        scaled_codes = _mm512_add_ps(scaled_codes, lowerBound_values);
        __m512 val = _mm512_sub_ps(query_values, scaled_codes);
        val = _mm512_mul_ps(val, val);
        sum = _mm512_add_ps(sum, val);
    }

    // Horizontal addition
    float result = _mm512_reduce_add_ps(sum);
    // Process the remaining elements
    result += AVX2::SQ8ComputeL2Sqr(query + i, codes + i, lowerBound + i, diff + i, dim - i);
    return result;
#else
    return Generic::SQ8ComputeL2Sqr(query, codes, lowerBound, diff, dim);
#endif
}

float
SQ8ComputeCodesIP(const uint8_t* codes1,
                  const uint8_t* codes2,
                  const float* lowerBound,
                  const float* diff,
                  uint64_t dim) {
#if defined(ENABLE_AVX512)
    __m512 sum = _mm512_setzero_ps();
    uint64_t i = 0;

    for (; i + 15 < dim; i += 16) {
        // Load data into registers
        __m128i code1_values = _mm_loadu_si128(reinterpret_cast<const __m128i*>(codes1 + i));
        __m128i code2_values = _mm_loadu_si128(reinterpret_cast<const __m128i*>(codes2 + i));
        __m512i codes1_512 = _mm512_cvtepu8_epi32(code1_values);
        __m512i codes2_512 = _mm512_cvtepu8_epi32(code2_values);
        __m512 code1_floats = _mm512_div_ps(_mm512_cvtepi32_ps(codes1_512), _mm512_set1_ps(255.0f));
        __m512 code2_floats = _mm512_div_ps(_mm512_cvtepi32_ps(codes2_512), _mm512_set1_ps(255.0f));
        __m512 diff_values = _mm512_loadu_ps(diff + i);
        __m512 lowerBound_values = _mm512_loadu_ps(lowerBound + i);

        // Perform calculations
        __m512 scaled_codes1 = _mm512_fmadd_ps(code1_floats, diff_values, lowerBound_values);
        __m512 scaled_codes2 = _mm512_fmadd_ps(code2_floats, diff_values, lowerBound_values);
        __m512 val = _mm512_mul_ps(scaled_codes1, scaled_codes2);
        sum = _mm512_add_ps(sum, val);
    }
    // Horizontal addition
    float result = _mm512_reduce_add_ps(sum);
    // Process the remaining elements
    result += AVX2::SQ8ComputeCodesIP(codes1 + i, codes2 + i, lowerBound + i, diff + i, dim - i);
    return result;
#else
    return Generic::SQ8ComputeCodesIP(codes1, codes2, lowerBound, diff, dim);
#endif
}

float
SQ8ComputeCodesL2Sqr(const uint8_t* codes1,
                     const uint8_t* codes2,
                     const float* lowerBound,
                     const float* diff,
                     uint64_t dim) {
    return Generic::SQ8ComputeCodesL2Sqr(codes1, codes2, lowerBound, diff, dim);
}

}  // namespace AVX512

}  // namespace vsag
