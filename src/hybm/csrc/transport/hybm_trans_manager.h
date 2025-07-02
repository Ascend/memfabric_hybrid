/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MF_HYBRID_HYBM_TRANS_MANAGER_H
#define MF_HYBRID_HYBM_TRANS_MANAGER_H

#include <string>
#include <unordered_map>
#include "hybm_trans_common.h"

namespace ock {
namespace mf {

class HybmTransManager;
using TransManagerPtr = std::shared_ptr<HybmTransManager>;

class HybmTransManager {
public:
    static TransManagerPtr Create(HybmTransType type);

public:
    HybmTrans() {};
    virtual ~HybmTrans() = default;

    /*
     * 1、本地IP（NIC、Device）
     * @return 0 if successful
     */
    virtual Result OpenDevice(HybmTransOptions &options);

    virtual Result CloseDevice();

    /*
     * 2、注册内存
     * @return 0 if successful
     */
    virtual Result RegisterMemoryRegion(const HybmTransMemReg &input, HybmTransKey &key);
    virtual Result UnregisterMemoryRegion(const void *addr);

    /*
     * 3、建链前的准备工作
     * @return 0 if successful
     */
    virtual Result Prepare(const HybmTransPrepareOptions &options);

    /*
     * 4、建链
     * @return 0 if successful
     */
    virtual Result Connect(const std::unordered_map<uint32_t, std::string> &nics, int mode);

    /*
     * 异步建链
     * @return 0 if successful
     */
    virtual Result AsyncConnect(const std::unordered_map<uint32_t, std::string> &nics, int mode);

    /*
     * 等待异步建链完成
     * @return 0 if successful
     */
    virtual Result WaitForConnected(int64_t timeoutNs) = 0;

    /*
     * 查询
     * @return 0 if successful
     */
    virtual std::string GetNic();

    /*
     * 根据addr
     * @return 0 if successful
     */
    virtual Result QueryKey(void* addr, HybmTransKey& key);
};

}
}

#endif // MF_HYBRID_HYBM_TRANS_MANAGER_H
