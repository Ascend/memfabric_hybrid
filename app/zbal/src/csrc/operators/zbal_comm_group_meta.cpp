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
#include "zbal_comm_group_meta.h"

namespace zbal {
namespace operators {
std::atomic<uint32_t> GroupMetaArranger::gGroupIndex{0};

ZResult GroupMetaArranger::Initialize(const ZBALInitStateExt &extraState) noexcept
{
    if (myMetaGVA_ != 0) {
        ZBAL_LOG_DEBUG("Group meta arranger already initialized");
        return Z_OK;
    }

    myMetaGVA_ = reinterpret_cast<uintptr_t>(extraState.myCommMetaDeviceGva);
    totalMetaSpaceSize_ = extraState.metaSizeOfDevice;
    /* translate to bytes from KB */
    singleMetaSpaceSize_ = static_cast<uint64_t>(extraState.commMetaSpaceSize) * 1024;
    commGroupCap_ = extraState.commGroupCap;

    auto result = Verify();
    if (result != Z_OK) {
        UnInitialize();
        ZBAL_LOG_ERROR("Initialize group meta arranger failed, result: " << result);
        return result;
    }

    ZBAL_LOG_DEBUG("Initialized group meta arranger successfully");

    return Z_OK;
}

void GroupMetaArranger::UnInitialize() noexcept
{
    myMetaGVA_ = 0;
    totalMetaSpaceSize_ = 0;
    singleMetaSpaceSize_ = 0;
    commGroupCap_ = 0;

    /* reset index to 0 */
    gGroupIndex = 0;

    ZBAL_LOG_DEBUG("Un-initialized group meta arranger successfully");
}

ZResult GroupMetaArranger::Verify() noexcept
{
    if (commGroupCap_ == 0) {
        ZBAL_LOG_ERROR("Group count cap cannot be zero");
        return Z_ERROR;
    }

    uint64_t tmpSize = singleMetaSpaceSize_ * commGroupCap_;
    if (totalMetaSpaceSize_ >= tmpSize) {
        return Z_OK;
    }

    ZBAL_LOG_ERROR("Size of meta space is less than single space multiple group count cap. Total meta space size: "
                   << totalMetaSpaceSize_ << " bytes, single group space size: " << singleMetaSpaceSize_
                   << " bytes, group count cap: " << commGroupCap_);
    return Z_ERROR;
}

ZResult GroupMetaArranger::CurrentGroup(uint32_t &index, uintptr_t &groupMetaGVA, uintptr_t &paramGVA,
                                        uintptr_t &addressExchangeGVA) noexcept
{
    auto currentIndex = gGroupIndex.load();
    if (currentIndex >= commGroupCap_) {
        ZBAL_LOG_DEBUG("Get group index failed as out of bound, current index: " << currentIndex << ", commGroupCap_: "
                                                                                 << commGroupCap_);
        return Z_ERROR;
    }

    index = currentIndex;
    return GetGroupByIndex(currentIndex, groupMetaGVA, paramGVA, addressExchangeGVA);
}

void GroupMetaArranger::Move2NextGroup() noexcept
{
    ++gGroupIndex;
}

ZResult GroupMetaArranger::GetGroupByIndex(uint32_t index, uintptr_t &groupMetaGVA, uintptr_t &paramGVA,
                                           uintptr_t &addressExchangeGVA) noexcept
{
    if (index >= commGroupCap_) {
        ZBAL_LOG_DEBUG("Get group failed as out of bound, index: " << index << ", commGroupCap_: " << commGroupCap_);
        return Z_ERROR;
    }

    groupMetaGVA = myMetaGVA_ + singleMetaSpaceSize_ * index;
    paramGVA = groupMetaGVA + GetCommGroupInfoSpaceSize();
    addressExchangeGVA = groupMetaGVA + ZBAL_OPERATE_PARAM_SIZE;

    ZBAL_LOG_DEBUG("Got group: " << index << ", group meta GVA: " << std::hex << groupMetaGVA
                                 << ", paramGVA: " << paramGVA << ", addressExchangeGVA: " << addressExchangeGVA);

    return Z_OK;
}
} // namespace operators
} // namespace zbal