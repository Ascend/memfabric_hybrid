/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include <gtest/gtest.h>
#include "secodefuzz/secodeFuzz.h"

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    DT_Set_Report_Path("./mf_fuzz");
    int ret = RUN_ALL_TESTS();
    return ret;
}