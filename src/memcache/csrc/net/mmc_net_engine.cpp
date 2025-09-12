/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "mmc_ref.h"
#include "mmc_net_engine.h"
#include "mmc_net_engine_acc.h"

namespace ock {
namespace mmc {
NetEnginePtr NetEngine::Create()
{
    return MmcMakeRef<NetEngineAcc>().Get();
}

}
}
