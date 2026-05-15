/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "zbal_communicator.h"
#include "zbal_comm_group_meta.h"
#include "zbal_npu_communicator_default.h"
#include "zbal_communicator_dummy.h"
#include "zbal_comm_group_id.h"

namespace zbal {
namespace operators {
CommunicatorPtr Communicator::gWorldCommunicator{nullptr};
std::map<uintptr_t, CommunicatorPtr> Communicator::gCommLookupMap;
std::map<std::string, CommunicatorPtr> Communicator::gCommLookupMapByName;
std::mutex Communicator::gMutex;

ZResult Communicator::Create(const zbal_comm_options_t &options, zbal_comm_t *comm, const ZBALInitStateExt &extraState)
{
    /* translate api options to inner options */
    CommGroupOptions commOptions;
    ZBAL_ASSERT_RETURN(options.name != nullptr, Z_INVALID_PARAM);
    ZBAL_ASSERT_RETURN(strlen(options.name) != 0, Z_INVALID_PARAM);
    commOptions.name = std::string(options.name);
    commOptions.worldSize = extraState.worldSize;
    commOptions.groupSize = options.groupSize;
    commOptions.myWorldRank = extraState.worldRankId;
    commOptions.myGroupRank = options.groupRankId;
    commOptions.gva = extraState.gvaDevice;
    commOptions.deviceId = extraState.deviceId;

    std::lock_guard<std::mutex> guard(gMutex);
    /* try to unique id */
    AutoReleaseGroupId tmpGroupId(extraState.commGroupCap, commOptions.groupSize, commOptions.myGroupRank,
                                  commOptions.myWorldRank, commOptions.name);
    auto result = tmpGroupId.Acquire();
    if (result != Z_OK) {
        ZBAL_LOG_AND_SET_LAST_ERROR("Get unique group id failed, result: " << result);
        return result;
    }

    /* init group meta arranger, already prevent initialize multiple time */
    auto &groupMetaArranger = GroupMetaArranger::Instance();
    result = groupMetaArranger.Initialize(extraState);
    if (result != Z_OK) {
        return result;
    }

    /* set size of spaces */
    commOptions.metaSize = groupMetaArranger.GetSingleMetaSpaceSize();
    commOptions.sizeForCommGroupInfo = groupMetaArranger.GetCommGroupInfoSpaceSize();
    commOptions.sizeForParam = groupMetaArranger.GetParamSpaceSize();
    commOptions.sizeForExchangeAddress = groupMetaArranger.GetExchangeSpaceSize();
    commOptions.localDeviceMemSize = ZBALInitState::Instance().ext_.symmetricMemSpace;
    commOptions.groupIndex = tmpGroupId.Id();

    /* get current index and myMetaGva */
    result = groupMetaArranger.GetGroupByIndex(tmpGroupId.Id(), commOptions.myMetaGva, commOptions.myParamDataGva,
                                               commOptions.myAddressExchangeGva);
    ZBAL_VALIDATE_RETURN(result == Z_OK, "Get meta range for group failed, probably out of range", result);

    /* create comm object */
    auto commInner = CreateInner(options.backendType, commOptions, options.isWorldGroup);
    if (commInner == nullptr || commInner->Initialize() != Z_OK) {
        return Z_CREATE_COMM_FAILED;
    }

    result = commInner->AssignGatherGroupId(tmpGroupId);
    ZBAL_VALIDATE_RETURN(result == Z_OK, "Assign group id failed, result: " << result, result);

    *comm = commInner.Get();

    ZBAL_LOG_DEBUG("Created communicator successfully, name: " << commInner->Name() << ", ptr: " << commInner.Get()
                                                               << " on rank:" << options.groupRankId);

    return Z_OK;
}

ZResult Communicator::Destroy(zbal_comm_t comm, uint32_t flags)
{
    std::lock_guard<std::mutex> guard(gMutex);
    CommunicatorPtr tmpComm = reinterpret_cast<Communicator *>(comm);

    ZBAL_LOG_DEBUG("Try to destroy communicator, input ptr " << comm << ", converted ptr: " << tmpComm.Get());

    return Communicator::DestroyInner(tmpComm);
}

void Communicator::DestroyAll()
{
    std::lock_guard<std::mutex> guard(gMutex);
    DestroyAllInner();
}

ZResult Communicator::Lookup(const std::string &name, zbal_comm_t *comm)
{
    std::lock_guard<std::mutex> guard(gMutex);

    CommunicatorPtr tmpComm;
    auto result = LookupInner(name, tmpComm);
    if (result != Z_OK) {
        return result;
    }

    *comm = tmpComm.Get();
    return Z_OK;
}

ZResult Communicator::GetGlobalComm(zbal_comm_t *comm)
{
    if (gWorldCommunicator == nullptr) {
        ZBAL_LOG_ERROR("Can't found global comm.");
        return Z_COMM_NOT_FOUND;
    }

    *comm = gWorldCommunicator.Get();
    return Z_OK;
}

ZResult Communicator::GetCommProperty(const zbal_comm_t comm, zbal_comm_property_t *property)
{
    if (comm == nullptr) {
        return Z_INVALID_PARAM;
    }

    CommunicatorPtr outComm = nullptr;
    {
        if (reinterpret_cast<uintptr_t>(comm) == reinterpret_cast<uintptr_t>(gWorldCommunicator.Get())) {
            outComm = gWorldCommunicator;
            ZBAL_LOG_DEBUG("Found global communicator entity success.");
        } else {
            std::lock_guard<std::mutex> guard(gMutex);

            auto iter = gCommLookupMap.find(reinterpret_cast<uintptr_t>(comm));
            if (iter == gCommLookupMap.end() || iter->second == nullptr) {
                ZBAL_LOG_ERROR("Communicator find failed when get property.");
                return Z_COMM_NOT_EXIST_BY_HANDLE;
            }
            outComm = iter->second;
            ZBAL_LOG_DEBUG("Found communicator entity success.");
        }
    }

    const CommGroupInfo &groupInfo = outComm->GetMetaInfo();
    property->name[0] = '\0'; // TODO
    property->backendType = ZBAL_ASCEND_NPU;
    property->isWorldGroup = 0; // TODO
    property->groupSize = groupInfo.groupSize;
    property->groupRankId = groupInfo.myGroupRank;
    property->symmetricMetaGva = groupInfo.myMetaGva;
    property->myGVA = nullptr; // TODO
    property->myMetaGVA = reinterpret_cast<void *>(groupInfo.myMetaGva);
    property->myMetaGVAForOpParam = reinterpret_cast<void *>(groupInfo.myParamDataGva);
    property->myMetaGVAForOpExchange = reinterpret_cast<void *>(groupInfo.myAddressExchangeGva);
    property->hostMemoryForProfiling = reinterpret_cast<void *>(groupInfo.hostMemoryForProfiling);
    property->devMemoryForProfiling = reinterpret_cast<void *>(groupInfo.devMemoryForProfiling);
    property->sizeOfMetaArea = groupInfo.sizeForCommGroupInfo;
    property->sizeOfMetaForOpParam = groupInfo.sizeForParam;
    property->sizeOfMetaForAddressExchange = groupInfo.sizeForExchangeAddress;
    property->tracePointPerCore = groupInfo.tracePointPerCore;
    property->localDeviceMemSize = groupInfo.localDeviceMemSize;
    return Z_OK;
}

uint32_t Communicator::Count()
{
    std::lock_guard<std::mutex> guard(gMutex);
    return gCommLookupMapByName.size();
}

void Communicator::DumpAllComm()
{
    for (auto &pair : gCommLookupMapByName) {
        CommunicatorPtr comm = pair.second;
        comm->SignalDumpTrace();
    }
}

CommunicatorPtr Communicator::CreateInner(zbal_backend_t backendType, const CommGroupOptions &options,
                                          bool isWorldGroup)
{
    ZBAL_LOG_DEBUG("CommGroupInfo dump: " << options);

    /* lock is acquired by caller already */

    if (gCommLookupMapByName.find(options.name) != gCommLookupMapByName.end()) {
        ZBAL_LOG_AND_SET_LAST_ERROR("Create communicator failed as there is already one named " << options.name);
        return nullptr;
    }

    CommunicatorPtr comm = nullptr;

    switch (backendType) {
        case ZBAL_ASCEND_NPU:
            comm = ZMakeRef<NpuCommunicatorDefault>(options, isWorldGroup, gWorldCommunicator).Get();
            break;
        case ZBAL_BACK_BUTT:
            comm = ZMakeRef<CommunicatorDummy>(options, isWorldGroup, gWorldCommunicator).Get();
            break;
        default:
            ZBAL_LOG_AND_SET_LAST_ERROR("Created communicator failed as backendType is invalid");
            return nullptr;
    }

    if (comm == nullptr) {
        ZBAL_LOG_AND_SET_LAST_ERROR("Create communicator failed, probably out of memory");
        return nullptr;
    }

    comm->ConstructCommGroupInfo(options);
    if (comm->Initialize()) {
        ZBAL_LOG_AND_SET_LAST_ERROR("Initialize communicator failed");
        return nullptr;
    }

    if (isWorldGroup && gWorldCommunicator == nullptr) {
        /*
         * if world group and not created, then
         * 1 created new one (created previously)
         * 2 set to global one
         * 3 increase reference and return
         */
        gWorldCommunicator = comm.Get();
        gCommLookupMapByName.emplace(options.name, comm.Get());
        return comm.Get();
    } else if (isWorldGroup && gWorldCommunicator != nullptr) {
        /*
         * if world group already created and return nullptr,
         * return nullptr directly as its already created
         */
        ZBAL_LOG_AND_SET_LAST_ERROR("Create communicator failed as world group already created");
        return nullptr;
    } else if (!isWorldGroup && gWorldCommunicator == nullptr) {
        /*
         * if not world group and world group not created,
         * here we need to create world group firstly,
         * return nullptr
         */
        ZBAL_LOG_AND_SET_LAST_ERROR("Create communicator failed as world group not created");
        return nullptr;
    } else {
        /*
         * if not world group and world group created
         */
        gCommLookupMap.emplace(reinterpret_cast<uintptr_t>(comm.Get()), comm.Get());
        gCommLookupMapByName.emplace(options.name, comm.Get());
        ZBAL_LOG_DEBUG("Created communicator, name: " << options.name << ", isWorldGroup: " << isWorldGroup);
        return comm.Get();
    }

    return nullptr;
}

ZResult Communicator::DestroyInner(CommunicatorPtr &comm)
{
    ZBAL_VALIDATE_RETURN(comm != nullptr, "Invalid param, comm is null", Z_INVALID_PARAM);

    /* lock is acquired by caller already */

    /* if it is the world one */
    if (comm->isWorldGroup_) {
        if (gCommLookupMap.size() != 0) {
            ZBAL_LOG_AND_SET_LAST_ERROR("Destroy other non world communicator firstly, then destroy the world one");
            return Z_COMM_DESTROY_GLOBAL_LAST;
        }

        if (gWorldCommunicator != nullptr) {
            ZBAL_LOG_INFO("Destroying the world communicator: " << gWorldCommunicator->Name() << " on rank "
                                                                << gWorldCommunicator->GetMetaInfo().myGroupRank);
            gCommLookupMapByName.erase(gWorldCommunicator->Name());
            gWorldCommunicator->DecreaseRef();
            gWorldCommunicator = nullptr;
        }
        return Z_OK;
    }

    /* erase from lookup map directly */
    auto iter = gCommLookupMap.find(reinterpret_cast<uintptr_t>(comm.Get()));
    if (iter == gCommLookupMap.end()) {
        ZBAL_LOG_INFO("Destroy communicator find communicator not existed");
        return Z_OK;
    }

    if (iter->second != nullptr) {
        gCommLookupMapByName.erase(iter->second->Name());
        gCommLookupMap.erase(iter);
        ZBAL_LOG_INFO("Destroying the normal communicator: " << iter->second->Name() << " on rank "
                                                             << iter->second->GetMetaInfo().myGroupRank);
    }

    return Z_OK;
}

void Communicator::DestroyAllInner()
{
    /* lock is acquired by caller already */

    /* clear all other world comm*/
    ZBALInitState::Instance().CommunicatorDestroy(gCommLookupMapByName.size());
    gCommLookupMap.clear();
    gCommLookupMapByName.clear();

    /* clear world one */
    if (gWorldCommunicator != nullptr) {
        gWorldCommunicator->DecreaseRef();
        gWorldCommunicator = nullptr;
    }

    /* reset  */
    GroupMetaArranger::Instance().UnInitialize();
}

ZResult Communicator::LookupInner(const std::string &name, CommunicatorPtr &comm)
{
    auto iter = gCommLookupMapByName.find(name);
    if (iter == gCommLookupMapByName.end()) {
        ZBAL_LOG_INFO_AND_SET_LAST_ERROR("Communicator named " << name << " not existed");
        return Z_COMM_NOT_EXIST_BY_NAME;
    }

    ZBAL_ASSERT_RETURN(iter->second != nullptr, Z_COMM_NOT_EXIST_BY_NAME);

    ZBAL_LOG_DEBUG("Found communicator with name: " << name);
    comm = iter->second;
    return Z_OK;
}
} // namespace operators
} // namespace zbal