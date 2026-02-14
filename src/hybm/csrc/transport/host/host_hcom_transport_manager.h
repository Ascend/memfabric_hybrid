/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/

#ifndef MF_HYBRID_HOST_HCOM_TRANSPORT_MANAGER_H
#define MF_HYBRID_HOST_HCOM_TRANSPORT_MANAGER_H

#include <mutex>
#include "hybm_transport_manager.h"
#include "hcom_service_c_define.h"
#include "host_hcom_counter_stream.h"
#include "mf_rwlock.h"

namespace ock {
namespace mf {
namespace transport {
namespace host {

struct HcomMemoryRegion {
    uint64_t addr;
    uint64_t size;
    TransportMemoryKey lKey;
    Service_MemoryRegion mr;
};

constexpr size_t KEYPASS_MAX_LEN = 10000;

class HcomTransportManager : public TransportManager {
public:
    static std::shared_ptr<HcomTransportManager> GetInstance()
    {
        static auto instance = std::make_shared<HcomTransportManager>();
        return instance;
    }

    Result OpenDevice(const TransportOptions &options) override;

    Result CloseDevice() override;

    Result RegisterMemoryRegion(const TransportMemoryRegion &mr) override;

    Result UnregisterMemoryRegion(uint64_t addr) override;

    bool QueryHasRegistered(uint64_t addr, uint64_t size) override;

    Result QueryMemoryKey(uint64_t addr, TransportMemoryKey &key) override;

    Result Prepare(const HybmTransPrepareOptions &parma) override;

    Result RemoveRanks(const std::vector<uint32_t> &removedRanks) override;

    Result Connect() override;

    Result AsyncConnect() override;

    Result WaitForConnected(int64_t timeoutNs) override;

    Result UpdateRankOptions(const HybmTransPrepareOptions &param) override;

    const std::string &GetNic() const override;

    Result ReadRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;

    Result ReadRemoteBatchAsync(uint32_t rankId, const CopyDescriptor &descriptor) override;

    Result WriteRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;

    Result WriteRemoteBatchAsync(uint32_t rankId, const CopyDescriptor &descriptor) override;

    Result ReadRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;

    Result WriteRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;

    Result Synchronize(uint32_t rankId) override;

private:
    Result InnerReadRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size);

    Result InnerWriteRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size);

    Result CheckTransportOptions(const TransportOptions &options);

    static Result TransportRpcHcomNewEndPoint(Hcom_Channel newCh, uint64_t usrCtx, const char *payLoad);

    static Result TransportRpcHcomEndPointBroken(Hcom_Channel ch, uint64_t usrCtx, const char *payLoad);

    static Result TransportRpcHcomRequestReceived(Service_Context ctx, uint64_t usrCtx);

    static Result TransportRpcHcomRequestPosted(Service_Context ctx, uint64_t usrCtx);

    static Result TransportRpcHcomOneSideDone(Service_Context ctx, uint64_t usrCtx);

    Result ConnectHcomChannel(uint32_t rankId, const std::string &url);

    void DisConnectHcomChannel(uint32_t rankId, Hcom_Channel ch);

    void HcomChannelDisconnected(uint32_t rankId, Hcom_Channel ch);

    void ForceReConnectHcomChannel(uint32_t rankId);

    Result GetMemoryRegionByAddr(const uint32_t &rankId, const uint64_t &addr, HcomMemoryRegion &mr);

    Result UpdateRankMrInfos(const std::unordered_map<uint32_t, TransportRankPrepareInfo> &opt);

    Result UpdateRankConnectInfos(const std::unordered_map<uint32_t, TransportRankPrepareInfo> &options);

    static int GetCACallBack(const char *name, char **caPath, char **crlPath, Hcom_PeerCertVerifyType *verifyType,
                             Hcom_TlsCertVerify *verify);

    static int GetCertCallBack(const char *name, char **certPath);

    static int GetPrivateKeyCallBack(const char *name, char **priKeyPath, char **keyPass, Hcom_TlsKeyPassErase *erase);

    static int CertVerifyCallBack(void *x509, const char *crlPath);

    static void KeyPassEraseCallBack(char *keyPass, int len);

    int PrepareThreadLocalStream();

private:
    hybm_data_op_type bmOptype_{};
    ReadWriteLock lock_;
    static thread_local HcomCounterStreamPtr stream_;
    std::string localNic_{};
    std::string localIp_{};
    Hcom_Service rpcService_{0};
    uint32_t rankId_{UINT32_MAX};
    uint32_t rankCount_{0};
    std::vector<std::mutex> mrMutex_;
    std::vector<std::vector<HcomMemoryRegion>> mrs_;
    std::vector<std::mutex> channelMutex_;
    std::vector<std::string> nics_;
    std::vector<Hcom_Channel> channels_;
    static hybm_tls_config tlsConfig_;
    static char keyPass_[KEYPASS_MAX_LEN];
    static std::mutex keyPassMutex;
};
} // namespace host
} // namespace transport
} // namespace mf
} // namespace ock

#endif // MF_HYBRID_HOST_HCOM_TRANSPORT_MANAGER_H