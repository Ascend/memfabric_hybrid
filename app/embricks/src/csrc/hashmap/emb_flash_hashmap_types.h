/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Embricks is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MEMFABRIC_HYBRID_EMB_FLASH_HASHMAP_TYPES_H
#define MEMFABRIC_HYBRID_EMB_FLASH_HASHMAP_TYPES_H

#include "emb_common_includes.h"

namespace ock {
namespace emb {
namespace hashmap {

/**
 * constant values
 */
constexpr uint32_t kBucketSize = 64;
constexpr uint32_t kMemBlockSize = UN2MB;
constexpr uint32_t kBucketsPerMemBlock = UN2MB / kBucketSize;
constexpr uint64_t kInvalidMapKey = UINT64_MAX;
constexpr uint64_t kInvalidMapValue = UINT64_MAX;
constexpr uint32_t kSubMapCount = 5;
constexpr uint32_t kMapPrimesCount = 256;
const uint32_t kMapPrimes[kMapPrimesCount] = {
    2,          3,          5,          7,          11,         13,         17,         19,         23,
    29,         31,         37,         41,         43,         47,         53,         59,         61,
    67,         71,         73,         79,         83,         89,         97,         103,        109,
    113,        127,        137,        139,        149,        157,        167,        179,        193,
    199,        211,        227,        241,        257,        277,        293,        313,        337,
    359,        383,        409,        439,        467,        503,        541,        577,        619,
    661,        709,        761,        823,        887,        953,        1031,       1109,       1193,
    1289,       1381,       1493,       1613,       1741,       1879,       2029,       2179,       2357,
    2549,       2753,       2971,       3209,       3469,       3739,       4027,       4349,       4703,
    5087,       5503,       5953,       6427,       6949,       7517,       8123,       8783,       9497,
    10273,      11113,      12011,      12983,      14033,      15173,      16411,      17749,      19183,
    20753,      22447,      24281,      26267,      28411,      30727,      33223,      35933,      38873,
    42043,      45481,      49201,      53201,      57557,      62233,      67307,      72817,      78779,
    85229,      92203,      99733,      107897,     116731,     126271,     136607,     147793,     159871,
    172933,     187091,     202409,     218971,     236897,     256279,     277261,     299951,     324503,
    351061,     379787,     410857,     444487,     480881,     520241,     562841,     608903,     658753,
    712697,     771049,     834181,     902483,     976369,     1056323,    1142821,    1236397,    1337629,
    1447153,    1565659,    1693859,    1832561,    1982627,    2144977,    2320627,    2510653,    2716249,
    2938679,    3179303,    3439651,    3721303,    4026031,    4355707,    4712381,    5098259,    5515729,
    5967347,    6456007,    6984629,    7556579,    8175383,    8844859,    9569143,    10352717,   11200489,
    12117689,   13109983,   14183539,   15345007,   16601593,   17961079,   19431899,   21023161,   22744717,
    24607243,   26622317,   28802401,   31160981,   33712729,   36473443,   39460231,   42691603,   46187573,
    49969847,   54061849,   58488943,   63278561,   68460391,   74066549,   80131819,   86693767,   93793069,
    101473717,  109783337,  118773397,  128499677,  139022417,  150406843,  162723577,  176048909,  190465427,
    206062531,  222936881,  241193053,  260944219,  282312799,  305431229,  330442829,  357502601,  386778277,
    418451333,  452718089,  489790921,  529899637,  573292817,  620239453,  671030513,  725980837,  785430967,
    849749479,  919334987,  994618837,  1076067617, 1164186217, 1259520799, 1362662261, 1474249943, 1594975441,
    1725587117, 1866894511, 2019773507, 2185171673, 2364114217, 2557710269, 2767159799, 2993761039, 3238918481,
    3504151727, 3791104843, 4101556399, 4294967291};

/**
 * Spinlock for linked bucket
 */
struct BucketSpinLock {
public:
    void Lock() noexcept;
    void UnLock() noexcept;

private:
    uint64_t lock = 0;
} __attribute__((aligned(8)));

EM_ALWAYS_INLINE void BucketSpinLock::Lock() noexcept
{
    while (!__sync_bool_compare_and_swap(&lock, 0, 1)) {}
}

EM_ALWAYS_INLINE void BucketSpinLock::UnLock() noexcept
{
    __atomic_store_n(&lock, 0, __ATOMIC_SEQ_CST);
}

/**
 * @brief options for recover
 */
struct FlashHashmapRecoverOptions {
    int fd = -1;
    bool useMmapMemory = false;
};

} // namespace hashmap
} // namespace emb
} // namespace ock

#endif // MEMFABRIC_HYBRID_EMB_FLASH_HASHMAP_TYPES_H
