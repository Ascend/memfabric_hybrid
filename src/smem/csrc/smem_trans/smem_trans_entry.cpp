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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <memory>
#include <thread>
#include <algorithm>
#include <cstring>

#include "mf_syntactic_sugar.h"
#include "mf_str_util.h"
#include "hybm.h"
#include "hybm_big_mem.h"
#include "hybm_data_op.h"
#include "smem_net_common.h"
#include "smem_store_factory.h"
#include "smem_trans_entry_manager.h"
#include "mf_fault_injection_point.h"
#include "smem_trans_entry.h"

namespace ock {
namespace smem {
// reserve 128GB dram va for malloc per rank, refine to configurable later
constexpr uint64_t TRANS_RESERVE_DRAM_VA_SIZE = 1024ULL * 1024 * 1024 * 128;
constexpr uint64_t TRANS_RESERVE_HBM_VA_SIZE = 1024ULL * 1024 * 1024 * 1024; // 1T

SmemTransEntryPtr SmemTransEntry::Create(const std::string &name, const std::string &storeUrl,
                                         const smem_trans_config_t &config)
{
    /* create entry and initialize */
    SmemTransEntryPtr transEntry;
    auto result = SmemTransEntryManager::Instance().CreateEntryByName(name, storeUrl, config, transEntry);
    if (result != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("create trans entry failed, probably out of memory");
        return nullptr;
    }

    /* initialize */
    result = transEntry->Initialize();
    if (result != SM_OK) {
        SmemTransEntryManager::Instance().RemoveEntryByName(name);
        SM_LOG_AND_SET_LAST_ERROR("initialize trans entry failed, result " << result);
        return nullptr;
    }

    result = transEntry->Join(0);
    if (result != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("trans join failed, ret:" << result);
        SmemTransEntryManager::Instance().RemoveEntryByName(name);
        return nullptr;
    }
    return transEntry;
}

SmemTransEntry::~SmemTransEntry()
{
    UnInitialize();
}

int32_t SmemTransEntry::Initialize()
{
    if (!ParseTransName(name_, workerUniqueId_.address, workerUniqueId_.port, workerUniqueId_.reserved)) {
        return SM_INVALID_PARAM;
    }
    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(CreateGlobalTeam(rankId_), "create global team failed");

    auto options = GenerateHybmOptions();
    options.bmDataOpType = static_cast<hybm_data_op_type>(HYBM_DOP_TYPE_DEFAULT);
    if (config_.dataOpType & SMEMB_DATA_OP_SDMA) {
#if !defined(ASCEND_NPU)
        SM_LOG_ERROR("current memfabric-hybrid binary is not built for ascend npu, can not use device_sdma optype.");
        return SM_ERROR;
#endif
        auto temp = static_cast<uint32_t>(options.bmDataOpType) | HYBM_DOP_TYPE_SDMA;
        options.bmDataOpType = static_cast<hybm_data_op_type>(temp);
    }
    if (config_.dataOpType & SMEMB_DATA_OP_DEVICE_RDMA) {
#if !defined(ASCEND_NPU)
        SM_LOG_ERROR("current memfabric-hybrid binary is not built for ascend npu, can not use device_rdma optype.");
        return SM_ERROR;
#endif
        auto temp = static_cast<uint32_t>(options.bmDataOpType) | HYBM_DOP_TYPE_DEVICE_RDMA;
        options.bmDataOpType = static_cast<hybm_data_op_type>(temp);
    }

    entity_ = hybm_create_entity(entityId_, &options, 0);
    SM_VALIDATE_RETURN(entity_ != nullptr, "create new entity failed.", SM_ERROR);

    auto ret = hybm_reserve_mem_space(entity_, 0);
    SM_VALIDATE_RETURN(ret == SM_OK, "rserve mem failed.", SM_ERROR);

    ret = hybm_export(entity_, nullptr, HYBM_FLAG_EXPORT_ENTITY, &entityInfo_.hybmInfo);
    SM_VALIDATE_RETURN(ret == SM_OK, "HybmExport device info failed: " << ret, SM_ERROR);

    entityInfo_.u.session = workerUniqueId_;
    entityInfo_.u.session.reserved = config_.role;
    return SM_OK;
}

void SmemTransEntry::UnInitialize()
{
    {
        std::lock_guard<std::mutex> lock(addrMapMutex_);
        for (auto &e : addrToSliceMap_) {
            hybm_free_local_memory(entity_, e.second, 1U, 0);
        }
        addrToSliceMap_.clear();
    }
    {
        mf::WriteGuard locker(remoteSliceRwMutex_);
        rankUpdateIdx_.clear();
        remoteSlices_.clear();
        rankToWorkerId_.clear();
        ranksRole_.clear();
        nameToWorkerId_.clear();
    }

    globalGroup_ = nullptr;
    if (entity_ != nullptr) {
        hybm_destroy_entity(entity_, 0);
        entity_ = nullptr;
    }
}

Result SmemTransEntry::CreateGlobalTeam(uint32_t rankId)
{
    SmemGroupChangeCallback joinFunc = std::bind(&SmemTransEntry::JoinHandle, this, std::placeholders::_1);
    SmemGroupChangeCallback updateFunc = std::bind(&SmemTransEntry::UpdateHandle, this, std::placeholders::_1);
    SmemGroupChangeCallback leaveFunc = std::bind(&SmemTransEntry::LeaveHandle, this, std::placeholders::_1);
    SmemGroupOption opt = {0U, rankId, config_.initTimeout * SECOND_TO_MILLSEC,
                           true, joinFunc, updateFunc, leaveFunc};
    SmemGroupEnginePtr group = SmemNetGroupEngine::Create(store_, opt);
    SM_ASSERT_RETURN(group != nullptr, SM_ERROR);

    globalGroup_ = group;
    return SM_OK;
}

Result SmemTransEntry::GroupOpBarrier(int32_t input)
{
    int32_t remoteRet = input;
    int32_t ret = globalGroup_->GroupGatherResult(input, remoteRet);
    if (ret != SM_OK) {
        SM_LOG_ERROR("join barrier failed, result: " << ret);
        return ret;
    }
    if (remoteRet != SM_OK) {
        SM_LOG_ERROR("join barrier, get remote result: " << remoteRet);
        return SM_ERROR;
    }
    return SM_OK;
}

void SmemTransEntry::AddRemoteInfo(uint32_t rk, smem_trans_role_t role, WorkerId &id, std::vector<void *> &global,
                                   std::vector<LocalMapAddress> &local)
{
    mf::WriteGuard locker(remoteSliceRwMutex_);
    rankToWorkerId_[rk] = id;
    ranksRole_[rk] = role;

    if (role == config_.role) { // same role, skip record
        return;
    }
    for (auto i = 0U; i < global.size(); i++) {
        remoteSlices_[id].emplace(local[i].address, LocalMapAddress(global[i], local[i].size));
        SM_LOG_DEBUG("record mem, local_rk:" << rankId_ << " remote_rk:" << rk << " global_addr:0x" << global[i]
                                             << "remote_addr:0x" << local[i].address);
    }
}

smem_trans_role_t SmemTransEntry::QueryRole(uint32_t rk)
{
    mf::ReadGuard locker(remoteSliceRwMutex_);
    auto it = ranksRole_.find(rk);
    if (it == ranksRole_.end()) {
        SM_LOG_ERROR("not found this rank:" << rk << " in ranksRole_");
        return SMEM_TRANS_BUTT;
    }
    return it->second;
}

void SmemTransEntry::AddRemoteInfo(uint32_t rk, std::vector<void *> &global, std::vector<LocalMapAddress> &local)
{
    mf::WriteGuard locker(remoteSliceRwMutex_);
    auto it = rankToWorkerId_.find(rk);
    if (it == rankToWorkerId_.end()) {
        SM_LOG_ERROR("not found this rank:" << rk << " in rankToWorkerId_");
        return;
    }

    WorkerId id = it->second;
    for (auto i = 0U; i < global.size(); i++) {
        remoteSlices_[id].emplace(local[i].address, LocalMapAddress(global[i], local[i].size));
        SM_LOG_DEBUG("record mem, local_rk:" << rankId_ << " remote_rk:" << rk << " global_addr:0x" << global[i]
                                             << "remote_addr:0x" << local[i].address);
    }
}

Result SmemTransEntry::JoinHandle(uint32_t rk)
{
    SM_LOG_INFO("do join func, local_rk: " << rankId_ << " receive_rk: " << rk
                                           << ", rank size is: " << globalGroup_->GetRankSize());

    uint32_t unitSize = sizeof(SmemTransExchangeInfo);
    std::string localInfo;
    if (rk == rankId_) {
        localInfo = std::string((char *) &entityInfo_, sizeof(SmemTransExchangeInfo));
    }
    std::unordered_map<uint32_t, std::string> allInfo;
    std::vector<uint32_t> joined;
    int32_t ret = globalGroup_->GroupGatherPrefixKey(rk, localInfo, allInfo);
    SM_VALIDATE_RETURN(ret == SM_OK, "gather prefix info failed, ret:" << ret, ret);

    for (auto &it : allInfo) {
        if (it.first == rankId_) {
            continue;
        }
        if (it.second.length() % unitSize != 0) {
            SM_LOG_ERROR("receive exchange info size is invalid!, size:" << it.second.length() << " rank:" << it.first);
            ret = SM_INVALID_PARAM;
            goto join_exit;
        }

        WorkerId id;
        smem_trans_role_t role = SMEM_TRANS_BUTT;
        SmemTransExchangeInfo info;
        std::vector<LocalMapAddress> local;
        std::vector<hybm_exchange_info> hybmInfos;
        std::vector<void *> global;
        uint32_t num = it.second.length() / unitSize;
        joined.push_back(it.first);
        for (uint32_t i = 0; i < num; i++) {
            (void)std::copy_n(it.second.c_str() + i * unitSize, unitSize, (char *)&info);
            if (i == 0) { // entity info
                role = static_cast<smem_trans_role_t>(info.u.session.reserved);
                info.u.session.reserved = 0U;
                WorkerIdUnion workerId{info.u.session};
                id = workerId.workerId;
                ret = hybm_import(entity_, &info.hybmInfo, 1U, nullptr, HYBM_FLAG_EXPORT_ENTITY);
                if (ret != SM_OK) {
                    SM_LOG_ERROR("hybm import entity failed, result: " << ret << " remote_rank:" << it.first
                                                                       << " local_rank:" << rankId_);
                    goto join_exit;
                }
            } else { // slice info
                hybmInfos.push_back(info.hybmInfo);
                local.push_back(info.u.address);
            }
        }

        if (!hybmInfos.empty() && role != config_.role) {
            global = std::vector<void *>(hybmInfos.size(), nullptr);
            ret = hybm_import(entity_, hybmInfos.data(), hybmInfos.size(), global.data(), 0);
            if (ret != SM_OK) {
                SM_LOG_ERROR("hybm import slice failed, result: " << ret << " remote_rank:" << it.first
                                                                  << " local_rank:" << rankId_);
                goto join_exit;
            }
        }
        AddRemoteInfo(it.first, role, id, global, local);
        rankUpdateIdx_[it.first] = num;
    }

    ret = GroupOpBarrier(ret);
    if (ret != SM_OK) {
        SM_LOG_ERROR("hybm barrier before mmap failed, result: " << ret);
        goto rollback_exit;
    }

    FIP_START(MMAP, &ret)
    ret = hybm_mmap(entity_, 0);
    FIP_END;
    if (ret != SM_OK) {
        SM_LOG_ERROR("hybm mmap failed, result: " << ret);
    }

join_exit:
    ret = GroupOpBarrier(ret);
    if (ret != SM_OK) {
        SM_LOG_ERROR("hybm barrier after mmap failed, result: " << ret);
        goto rollback_exit;
    }

    SM_LOG_INFO("end join func, local_rk: " << rankId_ << " receive_rk: " << rk << " receive_info_num:"
                                            << allInfo.size() << ", rank size is: " << globalGroup_->GetRankSize());
    return SM_OK;

rollback_exit:
    RemoveRanks(joined);
    return ret;
}

Result SmemTransEntry::UpdateHandle(uint32_t rk)
{
    SM_LOG_INFO("do update func, local_rk: " << rankId_ << " receive_rk: " << rk
                                             << ", rank size is: " << globalGroup_->GetRankSize());

    uint32_t unitSize = sizeof(SmemTransExchangeInfo);
    std::string xinfo;
    if (rk == rankId_) {
        for (auto &e : registedInfo_) {
            xinfo += std::string((char *) &e, sizeof(SmemTransExchangeInfo));
        }
    }

    int32_t ret = globalGroup_->GroupBarrierPrefixKey(rk, xinfo);
    SM_VALIDATE_RETURN(ret == SM_OK, "barrier prefix info failed, ret:" << ret, ret);
    auto role = (rk == rankId_) ? config_.role : QueryRole(rk);
    if (rk != rankId_ && (role != SMEM_TRANS_BUTT && role != config_.role)) {
        SmemTransExchangeInfo info;
        std::vector<LocalMapAddress> local;
        std::vector<hybm_exchange_info> hybmInfos;
        std::vector<void *> global;

        if (xinfo.length() % unitSize != 0) {
            SM_LOG_ERROR("receive exchange info size is invalid!, size:" << xinfo.length() << " rank:" << rk);
            ret = SM_INVALID_PARAM;
            goto update_exit;
        }
        uint32_t num = xinfo.length() / unitSize;
        for (uint32_t i = rankUpdateIdx_[rk]; i < num; i++) { // skip entity info
            (void)std::copy_n(xinfo.c_str() + i * unitSize, unitSize, (char *)&info);
            hybmInfos.push_back(info.hybmInfo);
            local.push_back(info.u.address);
        }
        if (!hybmInfos.empty()) {
            global = std::vector<void *>(hybmInfos.size(), nullptr);
            ret = hybm_import(entity_, hybmInfos.data(), hybmInfos.size(), global.data(), 0);
            if (ret != SM_OK) {
                SM_LOG_ERROR("hybm import slice failed, result: " << ret << " remote_rank:" << rk
                                                                  << " local_rank:" << rankId_);
                goto update_exit;
            }
            AddRemoteInfo(rk, global, local);
            rankUpdateIdx_[rk] = num;
        }

        FIP_START(MMAP, &ret)
        ret = hybm_mmap(entity_, 0);
        FIP_END;
        if (ret != SM_OK) {
            SM_LOG_ERROR("hybm mmap failed, result: " << ret);
        }
    }

update_exit:
    ret = GroupOpBarrier(ret);
    if (ret != SM_OK) {
        SM_LOG_ERROR("hybm barrier after mmap failed, result: " << ret);
        return ret;
    }

    SM_LOG_INFO("end update func, local_rk: " << rankId_ << " receive_rk: " << rk
                                              << ", rank size is: " << globalGroup_->GetRankSize());
    return SM_OK;
}

Result SmemTransEntry::LeaveHandle(uint32_t rk)
{
    SM_LOG_INFO("do leave func, receive_rk: " << rk);
    auto ret = hybm_remove_imported(entity_, rk, 0);
    if (ret != 0) {
        SM_LOG_ERROR("hybm leave failed, result: " << ret);
        return SM_ERROR;
    }
    return SM_OK;
}

Result SmemTransEntry::Join(uint32_t flags)
{
    for (uint32_t i = 0; i < SMEM_GROUP_RETRY_TIME; i++) {
        auto ret = globalGroup_->GroupJoin();
        if (ret == SM_INNER_BUSY) {
            sleep(1U); // sleep 1s
            continue;
        }
        SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "join failed, ret: " << ret);
        SM_LOG_DEBUG("join success. rank:" << rankId_);
        return SM_OK;
    }

    SM_LOG_ERROR("join timeout. rank:" << rankId_);
    return SM_ERROR;
}

Result SmemTransEntry::Update(uint32_t flags)
{
    for (uint32_t i = 0; i < SMEM_GROUP_RETRY_TIME; i++) {
        auto ret = globalGroup_->GroupUpdate();
        if (ret == SM_INNER_BUSY) {
            sleep(1U); // sleep 1s
            continue;
        }
        SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "update failed, ret: " << ret);
        SM_LOG_DEBUG("update success. rank:" << rankId_);
        return SM_OK;
    }

    SM_LOG_ERROR("update timeout. rank:" << rankId_);
    return SM_ERROR;
}

Result SmemTransEntry::Leave(uint32_t flags)
{
    auto ret = globalGroup_->GroupLeave();
    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "leave failed, ret: " << ret);

    return SM_OK;
}

void SmemTransEntry::StoreSlice(hybm_mem_slice_t slice, void *vaAddr)
{
    std::lock_guard<std::mutex> lock(addrMapMutex_);
    addrToSliceMap_[vaAddr] = slice;
}

hybm_mem_slice_t SmemTransEntry::RemoveSlice(void *addr)
{
    if (addr == nullptr) {
        SM_LOG_ERROR("failed to remove slice, addr is null");
        return nullptr;
    }

    hybm_mem_slice_t slice = nullptr;
    {
        std::lock_guard<std::mutex> lock(addrMapMutex_);
        auto it = addrToSliceMap_.find(addr);
        if (it == addrToSliceMap_.end()) {
            return nullptr;
        }
        slice = it->second;
        addrToSliceMap_.erase(it);
    }

    return slice;
}

void* SmemTransEntry::MallocDram(uint64_t size)
{
    if (size == 0) {
        SM_LOG_ERROR("malloc failed, invalid size 0.");
        return nullptr;
    }

    if (size > TRANS_RESERVE_DRAM_VA_SIZE) {
        SM_LOG_ERROR("malloc failed, invalid size:" << size << ", should be less than or equal "
                                                    << TRANS_RESERVE_DRAM_VA_SIZE);
        return nullptr;
    }

    auto slice = hybm_alloc_local_memory(entity_, HYBM_MEM_TYPE_HOST, size, 0);
    if (slice == nullptr) {
        SM_LOG_ERROR("malloc address with size: " << size << " failed. maybe free mem is not enough");
        return nullptr;
    }

    auto vaAddr = hybm_get_slice_va(entity_, slice);
    if (vaAddr == nullptr) {
        SM_LOG_ERROR("malloc address with size: " << size << " failed. maybe free mem is not enough");
        return nullptr;
    }

    StoreSlice(slice, vaAddr);

    SmemTransExchangeInfo info;
    auto ret = hybm_export(entity_, slice, 0, &info.hybmInfo);
    if (ret != 0) {
        SM_LOG_ERROR("export slice for register address with size: " << size << " failed:" << ret);
        hybm_free_local_memory(entity_, slice, size, 0);
        RemoveSlice(vaAddr);
        return nullptr;
    }

    std::unique_lock<std::mutex> uniqueLock{memMutex_};
    info.u.address = LocalMapAddress(vaAddr, size);
    registedInfo_.emplace_back(info);

    ret = Update(0);
    registedInfo_.clear();

    if (ret != 0) {
        SM_LOG_ERROR("update failed, rk:" << rankId_ << " addr:" << vaAddr << " ret:" << ret);
        hybm_free_local_memory(entity_, slice, size, 0);
        RemoveSlice(vaAddr);
        return nullptr;
    }
    return vaAddr;
}

Result SmemTransEntry::FreeDram(void *address)
{
    if (address == nullptr) {
        SM_LOG_ERROR("invalid address pointer.");
        return SM_INVALID_PARAM;
    }

    auto slice = RemoveSlice(address);
    if (slice == nullptr) {
        SM_LOG_ERROR("failed to free, invalid address:" << address);
        return SM_ERROR;
    }

    return hybm_free_local_memory(entity_, slice, 0, 0);
}

Result SmemTransEntry::RegisterLocalMemory(const void *address, uint64_t size, uint32_t flags)
{
    std::vector<std::pair<const void *, size_t>> regMemories;
    regMemories.emplace_back(address, size);
    return RegisterLocalMemories(regMemories, flags);
}

Result SmemTransEntry::RegisterLocalMemories(const std::vector<std::pair<const void *, size_t>> &regMemories,
                                             uint32_t flags)
{
    if (entity_ == nullptr) {
        SM_LOG_ERROR("not create entity.");
        return SM_ERROR;
    }

    if (regMemories.empty()) {
        return SM_OK;
    }

    for (auto it : regMemories) {
        if (it.first == nullptr || it.second == 0) {
            SM_LOG_ERROR("input address or size is invalid.");
            return SM_INVALID_PARAM;
        }
    }

    auto alignedMemories = regMemories;
    for (auto &it : alignedMemories) {
        AlignMemory(it.first, it.second);
    }
    auto mm = CombineMemories(alignedMemories);
    std::unique_lock<std::mutex> uniqueLock{memMutex_};
    for (auto &m : mm) {
        auto ret = RegisterOneMemory(m.first, m.second, flags);
        if (ret != 0) {
            registedInfo_.clear();
            return ret;
        }
    }

    auto ret = Update(0);
    registedInfo_.clear();
    SM_VALIDATE_RETURN(ret == SM_OK, "update failed, rk:" << rankId_ << " ret:" << ret, ret);
    return SM_OK;
}

static std::string uniqueToString(const WorkerId &unique)
{
    std::ostringstream oss;
    constexpr int WIDTH = 2;
    for (size_t i = 0; i < unique.size(); ++i) {
        oss << std::hex << std::setw(WIDTH) << std::setfill('0') << static_cast<int>(unique[i]);
        if (i < unique.size() - 1) {
            oss << ":";
        }
    }
    return oss.str();
}

Result SmemTransEntry::SyncTransfer(void *localAddr, const std::string &remoteUniqueId, void *remoteAddr,
                                    size_t dataSize, smem_bm_copy_type opcode, void *stream, uint32_t flags)
{
    return BatchSyncTransfer(&localAddr, remoteUniqueId, &remoteAddr, &dataSize, 1U, opcode, stream, flags);
}

Result SmemTransEntry::TransformAddr(Local2GlobalMap &maps, std::vector<void *> &addr, void *remoteAddrs[],
                                     const size_t dataSizes[], uint32_t size)
{
    for (auto i = 0U; i < size; i++) {
        if (remoteAddrs[i] == nullptr) {
            addr[i] = nullptr;
            continue;
        }

        auto pos = maps.lower_bound(remoteAddrs[i]);
        if (pos == maps.end()) {
            SM_LOG_ERROR("remote address[" << i << "] " << remoteAddrs[i] << " is invalid.");
            return SM_INVALID_PARAM;
        }

        if (dataSizes != nullptr &&
            (const uint8_t *)remoteAddrs[i] + dataSizes[i] > (const uint8_t *)(pos->first) + pos->second.size) {
            SM_LOG_ERROR("address[" << i << "], size[" << i << "]=" << dataSizes[i] << " out of range.");
            return SM_INVALID_PARAM;
        }

        addr[i] = (uint8_t *)pos->second.address + ((const uint8_t *)remoteAddrs[i] - (const uint8_t *)(pos->first));
    }
    return SM_OK;
}

Result SmemTransEntry::BatchSyncTransfer(void *localAddrs[], const std::string &remoteUniqueId, void *remoteAddrs[],
                                         const size_t dataSizes[], uint32_t batchSize, smem_bm_copy_type opcode,
                                         void *stream, uint32_t flags)
{
    SM_VALIDATE_RETURN(localAddrs != nullptr, "invalid localAddrs, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(remoteAddrs != nullptr, "invalid remoteAddrs, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(dataSizes != nullptr, "invalid dataSizes, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(batchSize != 0, "invalid batchSize, which is 0", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(flags == 0 || flags == COPY_EXTEND_FLAG, "invalid flags", SM_INVALID_PARAM);
    for (auto i = 0U; i < batchSize; i++) {
        SM_VALIDATE_RETURN(localAddrs[i] != nullptr, "localAddrs, which is null", SM_INVALID_PARAM);
        SM_VALIDATE_RETURN(remoteAddrs[i] != nullptr, "remoteAddrs, which is null", SM_INVALID_PARAM);
        SM_VALIDATE_RETURN(dataSizes[i] != 0, "invalid dataSizes, which is 0", SM_INVALID_PARAM);
    }
    WorkerId unique;
    auto ret = ParseNameToUniqueId(remoteUniqueId, unique);
    if (ret != 0) {
        return ret;
    }

    std::vector<void *> mappedAddress(batchSize);

    mf::ReadGuard locker(remoteSliceRwMutex_);
    auto it = remoteSlices_.find(unique);
    if (it == remoteSlices_.end()) {
        SM_LOG_ERROR("session:(" << remoteUniqueId << ")(" << uniqueToString(unique) << ") not found.");
        return SM_INVALID_PARAM;
    }

    ret = TransformAddr(it->second, mappedAddress, remoteAddrs, dataSizes, batchSize);
    SM_ASSERT_RETURN_NOLOG(ret == SM_OK, ret);

    uint32_t flag = flags | ((stream != nullptr) ? ASYNC_COPY_FLAG : 0);
    switch (opcode) {
        case SMEMB_COPY_L2G: {
            hybm_batch_copy_params copyParams = {localAddrs, mappedAddress.data(), dataSizes, batchSize};
            ret = hybm_data_batch_copy(entity_, &copyParams, HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, stream, flag);
        } break;
        case SMEMB_COPY_G2L: {
            hybm_batch_copy_params copyParams = {mappedAddress.data(), localAddrs, dataSizes, batchSize};
            ret = hybm_data_batch_copy(entity_, &copyParams, HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE, stream, flag);
        } break;
        default:
            SM_LOG_ERROR("unexpect copy type[" << opcode << "] is invalid.");
            return SM_INVALID_PARAM;
    }
    if (ret != 0) {
        SM_LOG_ERROR("batch copy data failed:" << ret);
    }
    return ret;
}

Result SmemTransEntry::BatchQuantTransfer(smem_trans_quant_copy_param_t *params, smem_bm_copy_type opcode)
{
    SM_VALIDATE_RETURN(params->localAddrs != nullptr, "invalid localAddrs, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(params->remoteAddrs != nullptr, "invalid remoteAddrs, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(params->dataSizes != nullptr, "invalid dataSizes, which is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(params->batchSize != 0, "invalid batchSize, which is 0", SM_INVALID_PARAM);
    for (auto i = 0U; i < params->batchSize; i++) {
        SM_VALIDATE_RETURN(params->localAddrs[i] != nullptr, "localAddrs, which is null", SM_INVALID_PARAM);
        SM_VALIDATE_RETURN(params->remoteAddrs[i] != nullptr, "remoteAddrs, which is null", SM_INVALID_PARAM);
        SM_VALIDATE_RETURN(params->dataSizes[i] != 0, "invalid dataSizes, which is 0", SM_INVALID_PARAM);
    }
    WorkerId unique;
    auto ret = ParseNameToUniqueId(params->remoteUniqueId, unique);
    if (ret != 0) {
        return ret;
    }

    std::vector<void *> mappedAddress(params->batchSize);
    std::vector<void *> scaleAddress(params->batchSize);
    std::vector<void *> offsetAddress(params->batchSize);

    mf::ReadGuard locker(remoteSliceRwMutex_);
    auto it = remoteSlices_.find(unique);
    if (it == remoteSlices_.end()) {
        SM_LOG_ERROR("session:(" << params->remoteUniqueId << ")(" << uniqueToString(unique) << ") not found.");
        return SM_INVALID_PARAM;
    }

    ret = TransformAddr(it->second, mappedAddress, params->remoteAddrs, params->dataSizes, params->batchSize);
    SM_ASSERT_RETURN_NOLOG(ret == SM_OK, ret);
    ret = TransformAddr(it->second, scaleAddress, reinterpret_cast<void **>(params->scale), nullptr,
                        params->batchSize);
    SM_ASSERT_RETURN_NOLOG(ret == SM_OK, ret);
    ret = TransformAddr(it->second, offsetAddress, reinterpret_cast<void **>(params->offset), nullptr,
                        params->batchSize);
    SM_ASSERT_RETURN_NOLOG(ret == SM_OK, ret);

    uint32_t flag = ((params->stream != nullptr) ? ASYNC_COPY_FLAG : 0);
    switch (opcode) {
        case SMEMB_COPY_L2G: {
            hybm_quant_copy_params copyParams = {params->localAddrs, mappedAddress.data(), params->dataSizes,
                                                 scaleAddress.data(), offsetAddress.data(),
                                                 params->batchSize, params->unitNum,
                                                 params->stream, params->inputType, flag};
            ret = hybm_data_quant_copy(entity_, &copyParams);
        } break;
        case SMEMB_COPY_G2L:
        default:
            SM_LOG_ERROR("unexpect copy type[" << opcode << "] is invalid.");
            return SM_INVALID_PARAM;
    }
    if (ret != 0) {
        SM_LOG_ERROR("batch quant copy data failed:" << ret);
    }
    return ret;
}

bool SmemTransEntry::ParseTransName(const std::string &name, ock::mf::net_addr_t &ip, uint16_t &port,
                                    uint16_t &reserved)
{
    UrlExtraction extraction;
    std::vector<std::string> splitRes = mf::StrUtil::Split(name, '_');
    int ret = extraction.ExtractIpPortFromUrl(std::string("tcp://").append(splitRes[0]));
    if (ret != 0) {
        SM_LOG_ERROR("parse name failed: " << ret);
        return false;
    }

    struct in6_addr addr6;
    if (inet_pton(AF_INET6, extraction.ip.c_str(), &addr6) == 1) {
        ip.ip.ipv6 = addr6;
        ip.type = ock::mf::IpV6;
    } else {
        struct in_addr addr4;
        if (inet_pton(AF_INET, extraction.ip.c_str(), &addr4) != 1) {
            SM_LOG_ERROR("Invalid IP address format: " << extraction.ip);
            return false;
        }
        ip.ip.ipv4.s_addr = ntohl(addr4.s_addr);
        ip.type = ock::mf::IpV4;
    }
    port = extraction.port;
    long tmpPid = 0;
    size_t resSize = 2UL;
    if (splitRes.size() < resSize || !mf::StrUtil::String2Int<long>(splitRes[1], tmpPid)) {
        SM_LOG_INFO("split pid info from uniqueid failed.");
    }
    reserved = tmpPid;
    return true;
}

void SmemTransEntry::RemoveRanks(std::vector<uint32_t> &rankSet)
{
    mf::WriteGuard locker(remoteSliceRwMutex_);
    for (auto rankId : rankSet) {
        rankUpdateIdx_.erase(rankId);

        auto it = rankToWorkerId_.find(rankId);
        if (it == rankToWorkerId_.end()) {
            SM_LOG_INFO("not found this rank:" << rankId);
            continue;
        }

        WorkerId id = it->second;
        rankToWorkerId_.erase(rankId);
        remoteSlices_.erase(id);
        ranksRole_.erase(rankId);

        auto ret = hybm_remove_imported(entity_, rankId, 0);
        if (ret != 0) {
            SM_LOG_ERROR("remove rank:" << rankId << " failed: " << ret);
        }
    }
}

Result SmemTransEntry::ParseNameToUniqueId(const std::string &name, WorkerId &uniqueId)
{
    WorkerUniqueId workerUniqueId;
    auto it = nameToWorkerId_.find(name);
    if (it != nameToWorkerId_.end()) {
        /* fast path */
        uniqueId = it->second;
        return SM_OK;
    }
    auto success = ParseTransName(name, workerUniqueId.address, workerUniqueId.port, workerUniqueId.reserved);
    if (!success) {
        SM_LOG_ERROR("parse name failed.");
        return SM_INVALID_PARAM;
    }

    WorkerIdUnion workerId{workerUniqueId};
    uniqueId = workerId.workerId;
    nameToWorkerId_.emplace(name, workerId.workerId);
    return SM_OK;
}

void SmemTransEntry::AlignMemory(const void *&address, uint64_t &size)
{
    constexpr auto NPU_PAGE_SIZE = 2UL * 1024UL * 1024UL;
    constexpr auto NPU_PAGE_MASK = ~(NPU_PAGE_SIZE - 1UL);

    auto pointer = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(address));
    auto alignPtr = (pointer & NPU_PAGE_MASK);
    auto diff = pointer - alignPtr;
    size += diff;
    size = ((size + NPU_PAGE_SIZE - 1) & NPU_PAGE_MASK);
    address = reinterpret_cast<const void *>(alignPtr);
}

std::vector<std::pair<const void *, size_t>>
SmemTransEntry::CombineMemories(std::vector<std::pair<const void *, size_t>> &input)
{
    std::sort(input.begin(), input.end());
    std::vector<std::pair<const void *, size_t>> result;
    auto current = input[0];
    for (auto i = 1U; i < input.size(); i++) {
        if ((const uint8_t *)current.first + current.second >= (const uint8_t *)input[i].first) {
            ptrdiff_t diff = ((const uint8_t *)input[i].first - (const uint8_t *)current.first);
            if (static_cast<size_t>(diff) > std::numeric_limits<size_t>::max() - input[i].second) {
                result.emplace_back(current);
                current = input[i];
                continue;
            }
            current.second = std::max(current.second, diff + input[i].second);
        } else {
            result.emplace_back(current);
            current = input[i];
        }
    }
    result.emplace_back(current);
    return result;
}

Result SmemTransEntry::RegisterOneMemory(const void *address, uint64_t size, uint32_t flags)
{
    auto slice = hybm_register_local_memory(entity_, address, size, 0);
    if (slice == nullptr) {
        SM_LOG_ERROR("register address with size: " << size << " failed!");
        return SM_ERROR;
    }
    SM_LOG_DEBUG("register memory(address with size=" << size << ") return slice=" << slice);

    SmemTransExchangeInfo info;
    auto ret = hybm_export(entity_, slice, 0, &info.hybmInfo);
    if (ret != 0) {
        SM_LOG_ERROR("export slice for register address with size: " << size << " failed:" << ret);
        hybm_free_local_memory(entity_, slice, size, 0);
        return SM_ERROR;
    }

    info.u.address = LocalMapAddress(const_cast<void *>(address), size);
    registedInfo_.emplace_back(info);
    return SM_OK;
}

hybm_options SmemTransEntry::GenerateHybmOptions()
{
    hybm_options options{};
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    options.memType = static_cast<hybm_mem_type>(HYBM_MEM_TYPE_DEVICE | HYBM_MEM_TYPE_HOST);
    options.rankCount = 512U;
    options.rankId = rankId_;
    options.devId = config_.deviceId;
    options.deviceVASpace = 0;
    options.hostVASpace = TRANS_RESERVE_DRAM_VA_SIZE;
    options.maxHBMSize = TRANS_RESERVE_HBM_VA_SIZE;
    options.maxDRAMSize = TRANS_RESERVE_DRAM_VA_SIZE;
    options.scene = HYBM_SCENE_TRANS;
    options.role = config_.role == SMEM_TRANS_SENDER ? HYBM_ROLE_SENDER : HYBM_ROLE_RECEIVER;
    options.dramShmFd = -1;
    options.enable56BitsGva = true; // trans enabled
    bzero(options.transUrl, sizeof(options.transUrl));
    bzero(options.tag, sizeof(options.tag));
    bzero(options.tagOpInfo, sizeof(options.tagOpInfo));

    uint16_t port = 11000 + entityId_;
    auto url = "tcp://127.0.0.1:" + std::to_string(port);

    constexpr size_t NIC_SIZE = sizeof(options.transUrl);
    size_t max_chars = std::min(url.length(), NIC_SIZE - 1);
    std::copy_n(url.c_str(), max_chars, options.transUrl);

    return std::move(options);
}

} // namespace smem
} // namespace ock