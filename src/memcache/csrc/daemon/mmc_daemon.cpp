/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#include "mmc_meta_service_process.h"

using namespace ock::mmc;


int main(int argc, char* argv[])
{
    return MmcMetaServiceProcess::getInstance().MainForExecutable();
}
