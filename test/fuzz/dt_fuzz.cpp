/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <iostream>
#include <string>
#include <atomic>
#include "dt_fuzz.h"
std::atomic<uint64_t> countMFFUzz(0);
void MFFUzzCountNumber(char *name)
{
    if (countMFFUzz.fetch_add(1) >= CAPI_FUZZ_COUNT) {
        countMFFUzz.store(0);
    }
    if (countMFFUzz % 10 == 0) {
        std::cout << std::string(name) << " is the loop of :" << countMFFUzz << std::endl;
    }
}