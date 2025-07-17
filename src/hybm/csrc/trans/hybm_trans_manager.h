/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
*/
#ifndef MF_HYBRID_TRANS_MANAGER_H
#define MF_HYBRID_TRANS_MANAGER_H

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
    HybmTransManager() {};
    virtual ~HybmTransManager() = default;

    /*
     * 1、本地IP（NIC、Device）
     * @return 0 if successful
     */
    virtual Result OpenDevice(HybmTransOptions &options) = 0;

    virtual Result CloseDevice() = 0;;

    /*
     * 2、注册内存
     * @return 0 if successful
     */
    virtual Result RegisterMemoryRegion(const HybmTransMemReg &input, HybmTransKey &key) = 0;;
    virtual Result UnregisterMemoryRegion(const void *addr) = 0;

    /*
     * 3、建链前的准备工作
     * @return 0 if successful
     */
    virtual Result Prepare(const HybmTransPrepareOptions &options) = 0;

    /*
     * 4、建链
     * @return 0 if successful
     */
    virtual Result Connect(const std::unordered_map<uint32_t, std::string> &nics, int mode) = 0;

    /*
     * 异步建链
     * @return 0 if successful
     */
    virtual Result AsyncConnect(const std::unordered_map<uint32_t, std::string> &nics, int mode) = 0;

    /*
     * 等待异步建链完成
     * @return 0 if successful
     */
    virtual Result WaitForConnected(int64_t timeoutNs) = 0;

    /*
     * 查询
     * @return 0 if successful
     */
    virtual std::string GetNic() = 0;

    /*
     * 根据addr
     * @return 0 if successful
     */
    virtual Result QueryKey(void* addr, HybmTransKey& key) = 0;
};

}
}

#endif // MF_HYBRID_TRANS_MANAGER_H
