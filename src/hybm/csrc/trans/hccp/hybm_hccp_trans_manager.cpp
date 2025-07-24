/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "hybm_hccp_trans_manager.h"
#include <string>
#include <arpa/inet.h>
#include "dl_hcom_api.h"

using namespace ock::mf;

Result HccpTransManager::OpenDevice(HybmTransOptions &options)
{
    return BM_OK;
}

Result HccpTransManager::CloseDevice()
{
    return BM_OK;
}

Result HccpTransManager::RegisterMemoryRegion(const HybmTransMemReg &input, MrInfo &output)
{
    return BM_OK;
}

Result HccpTransManager::UnregisterMemoryRegion(const void *addr)
{
    return BM_OK;
}

Result HccpTransManager::Prepare(const HybmTransPrepareOptions &options)
{
    return BM_OK;
}

Result HccpTransManager::Connect(const std::unordered_map<uint32_t, std::string> &nics, int mode)
{
    return BM_OK;
}

Result HccpTransManager::AsyncConnect(const std::unordered_map<uint32_t, std::string> &nics, int mode)
{
    return BM_OK;
}

Result HccpTransManager::WaitForConnected(int64_t timeoutNs)
{
    return BM_OK;
}

std::string HccpTransManager::GetNic()
{
    return "";
}

Result HccpTransManager::QueryKey(void *addr, HybmTransKey& key)
{
    return BM_OK;
}

Result HccpTransManager::RdmaOneSideTrans(const uint32_t &rankId, const uint64_t &lAddr, const uint64_t &rAddr,
                                          const uint64_t &size, const bool &isGet)
{
    return BM_OK;
}