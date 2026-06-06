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
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <mockcpp/mockcpp.hpp>

#include "hybm_data_op_sdma.h"
#include "hybm_stream_manager.h"
#include "dl_acl_api.h"
#include "dl_hal_api.h"
#include "dl_hybm_copy_extend.h"
#include "hybm_functions.h"
#include "hybm_data_op.h"
#include "hybm_gva.h"
#include "hybm_mem_segment.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

static void *g_mockParamSpace = nullptr;
static void *g_mockOutput = nullptr;

static void InitMockAddresses()
{
    constexpr uint64_t MOCK_PARAM_SPACE_SIZE = 64 * 1024 * 1024 + 256 * 1024;
    g_mockParamSpace = mmap(nullptr, MOCK_PARAM_SPACE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    g_mockOutput = mmap(nullptr, 4096UL, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

static void CleanupMockAddresses()
{
    if (g_mockParamSpace != nullptr && g_mockParamSpace != MAP_FAILED) {
        munmap(g_mockParamSpace, 4096UL);
        g_mockParamSpace = nullptr;
    }
    if (g_mockOutput != nullptr && g_mockOutput != MAP_FAILED) {
        munmap(g_mockOutput, 4096UL);
        g_mockOutput = nullptr;
    }
}

// 模拟 AclrtMallocHost 函数的实现
int MockAclrtMallocHost(void **ptr, size_t size)
{
    if (ptr == nullptr || size == 0) {
        return -1;
    }
    if (g_mockParamSpace == nullptr) {
        InitMockAddresses();
    }
    *ptr = g_mockParamSpace;
    return 0;
}

// 模拟 AclrtFreeHost 函数的实现
int MockAclrtFreeHost(void *ptr)
{
    return 0;
}

// 模拟 HalHostRegister 函数的实现
int MockHalHostRegister(void *addr, uint64_t size, uint32_t flags, uint32_t devId, void **output)
{
    *output = g_mockOutput;
    return 0;
}

class HybmDataOpSdmaTest : public testing::Test {
public:
    void SetUp() override
    {
        dataOp_ = std::make_shared<ock::mf::HostDataOpSDMA>();
    }

    void TearDown() override
    {
        GlobalMockObject::verify();
        GlobalMockObject::reset();
    }

    void InitMockEnv()
    {
        // 模拟 DlAclApi::AclrtMallocHost 方法
        MOCKER(&ock::mf::DlAclApi::AclrtMallocHost).stubs().will(invoke(MockAclrtMallocHost));

        // 模拟 DlAclApi::AclrtFreeHost 方法
        MOCKER(&ock::mf::DlAclApi::AclrtFreeHost).stubs().will(invoke(MockAclrtFreeHost));

        // 模拟 DlAclApi::AclrtMalloc 方法
        MOCKER(&ock::mf::DlAclApi::AclrtMalloc).stubs().will(returnValue(0));

        // 模拟 DlAclApi::AclrtFree 方法
        MOCKER(&ock::mf::DlAclApi::AclrtFree).stubs().will(returnValue(0));

        // 模拟 DlAclApi::AclrtMemcpy 方法
        MOCKER(&ock::mf::DlAclApi::AclrtMemcpy).stubs().will(returnValue(0));

        // 模拟 DlAclApi::AclrtMemcpyAsync 方法
        MOCKER(&ock::mf::DlAclApi::AclrtMemcpyAsync).stubs().will(returnValue(0));

        // 模拟 DlAclApi::AclrtSynchronizeStream 方法
        MOCKER(&ock::mf::DlAclApi::AclrtSynchronizeStream).stubs().will(returnValue(0));

        // 模拟 DlAclApi::RtMemcpyAsync 方法
        MOCKER(&ock::mf::DlAclApi::RtMemcpyAsync).stubs().will(returnValue(0));

        // 模拟 DlAclApi::GetAscendSocType 方法
        MOCKER(&ock::mf::DlAclApi::GetAscendSocType).stubs().will(returnValue(ock::mf::AscendSocType::ASCEND_910B));

        // 模拟 DlHalApi::HalHostRegister 方法
        MOCKER(&ock::mf::DlHalApi::HalHostRegister).stubs().will(returnValue(0));

        // 模拟 DlHalApi::HalHostUnregisterEx 方法
        MOCKER(&ock::mf::DlHalApi::HalHostUnregisterEx).stubs().will(returnValue(0));

        // 模拟 DlHybmExtendApi::HybmCopyExtend 方法
        MOCKER(&ock::mf::DlHybmExtendApi::HybmCopyExtend).stubs().will(returnValue(0));

        // 模拟 DlHybmExtendApi::HybmBatchCopyExtend 方法
        MOCKER(&ock::mf::DlHybmExtendApi::HybmBatchCopyExtend).stubs().will(returnValue(0));

        // 模拟 DlHybmExtendApi::HybmBatchCopyQuant 方法
        MOCKER(&ock::mf::DlHybmExtendApi::HybmBatchCopyQuant).stubs().will(returnValue(0));

        // 模拟全局函数
        MOCKER(HybmGetInitDeviceId).stubs().will(returnValue(0));
        // 模拟 ock::mf 命名空间中的函数
        MOCKER(&ock::mf::HybmGetInitedLogicDeviceId).stubs().will(returnValue(0));
        MOCKER(&ock::mf::IsArmArch).stubs().will(returnValue(true));

        // 模拟 HybmStreamManager 相关方法
        // 创建一个空指针作为 GetThreadAclStream 的返回值
        void *mockAclStream = nullptr;
        MOCKER(&ock::mf::HybmStreamManager::GetThreadAclStream).stubs().will(returnValue(mockAclStream));

        mockStream_ = std::make_shared<ock::mf::HybmStream>(0, 0, 0);
        MOCKER(&ock::mf::HybmStreamManager::GetThreadHybmStream).stubs().will(returnValue(mockStream_));
        MOCKER(&ock::mf::HybmStreamManager::DestroyAllThreadHybmStream).stubs().will(returnValue(0));
        MOCKER_CPP(&ock::mf::HybmStream::SubmitTasks,
            int32_t (*)(ock::mf::HybmStream *, const ock::mf::StreamTask &)).stubs().will(returnValue(0));
        MOCKER_CPP(&ock::mf::HybmStream::Synchronize,
            int32_t (*)(ock::mf::HybmStream *, uint32_t)).stubs().will(returnValue(0));

        // Make MemSegment::InitDeviceInfo succeed with deterministic ids, so reachability
        // checks behave consistently in UT.
        MOCKER(&ock::mf::DlAclApi::AclrtSetDevice).stubs().will(returnValue(0));
        MOCKER(&ock::mf::DlAclApi::RtDeviceGetBareTgid).stubs().will(returnValue(0));
        MOCKER(&ock::mf::DlAclApi::RtGetLogicDevIdByUserDevId).stubs().will(returnValue(0));
        MOCKER(&ock::mf::DlAclApi::RtGetDeviceInfo).stubs().will(returnValue(0));
        (void)ock::mf::MemSegment::InitDeviceInfo(0);
    }

protected:
    std::shared_ptr<ock::mf::HostDataOpSDMA> dataOp_;
    std::shared_ptr<ock::mf::HybmStream> mockStream_;
};

TEST_F(HybmDataOpSdmaTest, initialize_success)
{
    InitMockEnv();
    // 测试 Initialize 成功场景
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, initialize_already_inited)
{
    InitMockEnv();
    // 测试重复初始化场景
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    // 再次调用 Initialize 应该返回 BM_OK
    ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, initialize_failed)
{
    MOCKER(&ock::mf::DlHalApi::HalHostRegister).stubs().will(returnValue(-1));
    InitMockEnv();
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_ERROR, ret);
}

TEST_F(HybmDataOpSdmaTest, uninitialize)
{
    InitMockEnv();
    // 测试 UnInitialize 场景
    // 先初始化
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    // 调用 UnInitialize
    dataOp_->UnInitialize();
    // 验证资源是否被释放
    // 由于 UnInitialize 主要是释放内存，这里主要测试调用是否成功
    ASSERT_TRUE(true);
}

TEST_F(HybmDataOpSdmaTest, data_copy_local_device_to_global_device)
{
    InitMockEnv();
    // 测试本地设备到全局设备的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopy(params, HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_global_device_to_local_device)
{
    InitMockEnv();
    // 测试全局设备到本地设备的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_local_host_to_global_device)
{
    InitMockEnv();
    // 测试本地主机到全局设备的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_local_host_to_global_device_failed)
{
    MOCKER(&ock::mf::DlAclApi::AclrtMalloc).stubs().will(returnValue(-1)).then(returnValue(0));
    MOCKER(&ock::mf::DlAclApi::AclrtMemcpy).stubs().will(returnValue(-1)).then(returnValue(0));
    std::shared_ptr<ock::mf::HybmStream> mockStream = nullptr;
    MOCKER(&ock::mf::HybmStreamManager::GetThreadHybmStream).stubs().will(returnValue(mockStream));
    InitMockEnv();

    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);
    hybm_copy_params params{};
    ock::mf::ExtOptions options{};
    ret = dataOp_->DataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_DL_FUNCTION_FAILED, ret);
    ret = dataOp_->DataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_DL_FUNCTION_FAILED, ret);
    ret = dataOp_->DataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_ERROR, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_global_device_to_local_host)
{
    InitMockEnv();
    // 测试全局设备到本地主机的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_global_device_to_local_host_failed)
{
    MOCKER(&ock::mf::DlAclApi::AclrtMalloc).stubs().will(returnValue(-1)).then(returnValue(0));
    MOCKER(&ock::mf::DlAclApi::AclrtMemcpy).stubs().will(returnValue(-1));
    std::shared_ptr<ock::mf::HybmStream> mockStream1 = nullptr;
    std::shared_ptr<ock::mf::HybmStream> mockStream2 = std::make_shared<ock::mf::HybmStream>(0, 0, 0);
    MOCKER(&ock::mf::HybmStreamManager::GetThreadHybmStream).stubs()
        .will(returnValue(mockStream1)).then(returnValue(mockStream1)).then(returnValue(mockStream2));
    InitMockEnv();

    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);
    hybm_copy_params params{};
    ock::mf::ExtOptions options{};
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, options);
    ASSERT_EQ(BM_DL_FUNCTION_FAILED, ret);
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, options);
    ASSERT_EQ(BM_ERROR, ret);
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, options);
    ASSERT_EQ(BM_DL_FUNCTION_FAILED, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_global_device_to_global_device)
{
    InitMockEnv();
    // 测试全局设备到全局设备的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_global_device_to_global_host)
{
    InitMockEnv();
    // 测试全局设备到全局主机的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_global_host_to_global_device)
{
    InitMockEnv();
    // 测试全局主机到全局设备的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_global_host_to_global_host)
{
    InitMockEnv();
    // 测试全局主机到全局主机的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_HOST_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_local_device_to_global_host)
{
    InitMockEnv();
    // 测试本地设备到全局主机的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopy(params, HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_local_host_to_global_host)
{
    InitMockEnv();
    // 测试本地主机到全局主机的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_local_host_to_global_host_failed)
{
    MOCKER(&ock::mf::DlAclApi::AclrtMalloc).stubs().will(returnValue(-1)).then(returnValue(0));
    MOCKER(&ock::mf::DlAclApi::AclrtMemcpy).stubs().will(returnValue(-1)).then(returnValue(0));
    std::shared_ptr<ock::mf::HybmStream> mockStream = nullptr;
    MOCKER(&ock::mf::HybmStreamManager::GetThreadHybmStream).stubs().will(returnValue(mockStream));
    InitMockEnv();

    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);
    hybm_copy_params params{};
    ock::mf::ExtOptions options{};
    ret = dataOp_->DataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_DL_FUNCTION_FAILED, ret);
    ret = dataOp_->DataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_DL_FUNCTION_FAILED, ret);
    ret = dataOp_->DataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_ERROR, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_global_host_to_local_host)
{
    InitMockEnv();
    // 测试全局主机到本地主机的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_HOST_TO_LOCAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_global_host_to_local_host_failed)
{
    MOCKER(&ock::mf::DlAclApi::AclrtMalloc).stubs().will(returnValue(-1)).then(returnValue(0));
    MOCKER(&ock::mf::DlAclApi::AclrtMemcpy).stubs().will(returnValue(-1));
    std::shared_ptr<ock::mf::HybmStream> mockStream1 = nullptr;
    std::shared_ptr<ock::mf::HybmStream> mockStream2 = std::make_shared<ock::mf::HybmStream>(0, 0, 0);
    MOCKER(&ock::mf::HybmStreamManager::GetThreadHybmStream).stubs()
        .will(returnValue(mockStream1)).then(returnValue(mockStream1)).then(returnValue(mockStream2));
    InitMockEnv();

    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);
    hybm_copy_params params{};
    ock::mf::ExtOptions options{};
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_HOST_TO_LOCAL_HOST, options);
    ASSERT_EQ(BM_DL_FUNCTION_FAILED, ret);
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_HOST_TO_LOCAL_HOST, options);
    ASSERT_EQ(BM_ERROR, ret);
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_HOST_TO_LOCAL_HOST, options);
    ASSERT_EQ(BM_DL_FUNCTION_FAILED, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_global_host_to_local_device)
{
    InitMockEnv();
    // 测试全局主机到本地设备的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_unsupported_direction)
{
    InitMockEnv();
    // 测试不支持的拷贝方向
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    // 使用一个无效的方向
    ret = dataOp_->DataCopy(params, static_cast<hybm_data_copy_direction>(HYBM_DATA_COPY_DIRECTION_BUTT), options);
    ASSERT_EQ(BM_INVALID_PARAM, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_async)
{
    InitMockEnv();
    // 测试异步数据拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};
    options.flags = ASYNC_COPY_FLAG;

    ret = dataOp_->DataCopyAsync(params, HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_async_local_device_to_global_device)
{
    InitMockEnv();
    // 测试本地设备到全局设备的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopyAsync(params, HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_async_global_device_to_local_device)
{
    InitMockEnv();
    // 测试全局设备到本地设备的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopyAsync(params, HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_async_local_host_to_global_device)
{
    InitMockEnv();
    // 测试本地主机到全局设备的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopyAsync(params, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_async_local_host_to_global_device_failed)
{
    MOCKER(&ock::mf::DlAclApi::AclrtMalloc).stubs().will(returnValue(-1)).then(returnValue(0));
    MOCKER(&ock::mf::DlAclApi::AclrtMemcpy).stubs().will(returnValue(-1)).then(returnValue(0));
    std::shared_ptr<ock::mf::HybmStream> mockStream = nullptr;
    MOCKER(&ock::mf::HybmStreamManager::GetThreadHybmStream).stubs().will(returnValue(mockStream));
    InitMockEnv();

    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);
    hybm_copy_params params{};
    ock::mf::ExtOptions options{};
    ret = dataOp_->DataCopyAsync(params, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_DL_FUNCTION_FAILED, ret);
    ret = dataOp_->DataCopyAsync(params, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_DL_FUNCTION_FAILED, ret);
    ret = dataOp_->DataCopyAsync(params, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_ERROR, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_async_global_device_to_local_host)
{
    InitMockEnv();
    // 测试全局设备到本地主机的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopyAsync(params, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_async_global_device_to_local_host_failed)
{
    MOCKER(&ock::mf::DlAclApi::AclrtMalloc).stubs().will(returnValue(-1)).then(returnValue(0));
    MOCKER(&ock::mf::DlAclApi::AclrtMemcpy).stubs().will(returnValue(-1));
    std::shared_ptr<ock::mf::HybmStream> mockStream1 = nullptr;
    std::shared_ptr<ock::mf::HybmStream> mockStream2 = std::make_shared<ock::mf::HybmStream>(0, 0, 0);
    MOCKER(&ock::mf::HybmStreamManager::GetThreadHybmStream).stubs()
        .will(returnValue(mockStream1)).then(returnValue(mockStream1)).then(returnValue(mockStream2));
    InitMockEnv();

    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);
    hybm_copy_params params{};
    ock::mf::ExtOptions options{};
    ret = dataOp_->DataCopyAsync(params, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, options);
    ASSERT_EQ(BM_DL_FUNCTION_FAILED, ret);
    ret = dataOp_->DataCopyAsync(params, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, options);
    ASSERT_EQ(BM_ERROR, ret);
    ret = dataOp_->DataCopyAsync(params, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, options);
    ASSERT_EQ(BM_DL_FUNCTION_FAILED, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_async_global_device_to_global_device)
{
    InitMockEnv();
    // 测试全局设备到全局设备的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopyAsync(params, HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_async_global_device_to_global_host)
{
    InitMockEnv();
    // 测试全局设备到全局主机的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopyAsync(params, HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_async_global_host_to_global_device)
{
    InitMockEnv();
    // 测试全局主机到全局设备的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopyAsync(params, HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_async_global_host_to_global_host)
{
    InitMockEnv();
    // 测试全局主机到全局主机的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopyAsync(params, HYBM_GLOBAL_HOST_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_async_local_device_to_global_host)
{
    InitMockEnv();
    // 测试本地设备到全局主机的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopyAsync(params, HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_async_local_host_to_global_host)
{
    InitMockEnv();
    // 测试本地主机到全局主机的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopyAsync(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_async_local_host_to_global_host_failed)
{
    MOCKER(&ock::mf::DlAclApi::AclrtMalloc).stubs().will(returnValue(-1)).then(returnValue(0));
    MOCKER(&ock::mf::DlAclApi::AclrtMemcpy).stubs().will(returnValue(-1)).then(returnValue(0));
    std::shared_ptr<ock::mf::HybmStream> mockStream = nullptr;
    MOCKER(&ock::mf::HybmStreamManager::GetThreadHybmStream).stubs().will(returnValue(mockStream));
    InitMockEnv();

    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);
    hybm_copy_params params{};
    ock::mf::ExtOptions options{};
    ret = dataOp_->DataCopyAsync(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_DL_FUNCTION_FAILED, ret);
    ret = dataOp_->DataCopyAsync(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_DL_FUNCTION_FAILED, ret);
    ret = dataOp_->DataCopyAsync(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_ERROR, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_async_global_host_to_local_host)
{
    InitMockEnv();
    // 测试全局主机到本地主机的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopyAsync(params, HYBM_GLOBAL_HOST_TO_LOCAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_async_global_host_to_local_host_failed)
{
    MOCKER(&ock::mf::DlAclApi::AclrtMalloc).stubs().will(returnValue(-1)).then(returnValue(0));
    MOCKER(&ock::mf::DlAclApi::AclrtMemcpy).stubs().will(returnValue(-1));
    std::shared_ptr<ock::mf::HybmStream> mockStream1 = nullptr;
    std::shared_ptr<ock::mf::HybmStream> mockStream2 = std::make_shared<ock::mf::HybmStream>(0, 0, 0);
    MOCKER(&ock::mf::HybmStreamManager::GetThreadHybmStream).stubs()
        .will(returnValue(mockStream1)).then(returnValue(mockStream1)).then(returnValue(mockStream2));
    InitMockEnv();

    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);
    hybm_copy_params params{};
    ock::mf::ExtOptions options{};
    ret = dataOp_->DataCopyAsync(params, HYBM_GLOBAL_HOST_TO_LOCAL_HOST, options);
    ASSERT_EQ(BM_DL_FUNCTION_FAILED, ret);
    ret = dataOp_->DataCopyAsync(params, HYBM_GLOBAL_HOST_TO_LOCAL_HOST, options);
    ASSERT_EQ(BM_ERROR, ret);
    ret = dataOp_->DataCopyAsync(params, HYBM_GLOBAL_HOST_TO_LOCAL_HOST, options);
    ASSERT_EQ(BM_DL_FUNCTION_FAILED, ret);
}

TEST_F(HybmDataOpSdmaTest, data_copy_async_global_host_to_local_device)
{
    InitMockEnv();
    // 测试全局主机到本地设备的拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};

    ret = dataOp_->DataCopyAsync(params, HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, wait)
{
    InitMockEnv();
    // 测试等待操作
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    ret = dataOp_->Wait(0);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, cleanup)
{
    InitMockEnv();
    // 测试 CleanUp 方法
    dataOp_->CleanUp();
    // CleanUp 调用了 HybmStreamManager::DestroyAllThreadHybmStream，这里主要测试调用是否成功
    ASSERT_TRUE(true);
}

TEST_F(HybmDataOpSdmaTest, transform_va)
{
    InitMockEnv();
    // 测试 VA 转换
    // TransformVa 方法不依赖于初始化状态，直接调用即可
    void *src = nullptr;
    void *dst = nullptr;
    dataOp_->TransformVa(src, dst, HYBM_LOCAL_HOST_TO_GLOBAL_HOST);
    // TransformVa 调用了 HybmVaManager::GetInstance().TransformVa，这里主要测试调用是否成功
    ASSERT_TRUE(true);
}

TEST_F(HybmDataOpSdmaTest, batch_data_copy_local_host_to_global_device)
{
    InitMockEnv();
    // 测试批量数据拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_batch_copy_params params{};
    ock::mf::ExtOptions options{};

    // 准备批量拷贝参数
    void *sources[2] = {nullptr, nullptr};
    void *destinations[2] = {nullptr, nullptr};
    uint64_t dataSizes[2] = {1024ULL, 2048ULL};
    params.batchSize = 2UL;
    params.sources = sources;
    params.destinations = destinations;
    params.dataSizes = dataSizes;

    ret = dataOp_->BatchDataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, batch_data_copy_global_device_to_local_host)
{
    InitMockEnv();
    // 测试批量数据拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_batch_copy_params params{};
    ock::mf::ExtOptions options{};

    // 准备批量拷贝参数
    void *sources[2] = {nullptr, nullptr};
    void *destinations[2] = {nullptr, nullptr};
    uint64_t dataSizes[2] = {1024ULL, 2048ULL};
    params.batchSize = 2UL;
    params.sources = sources;
    params.destinations = destinations;
    params.dataSizes = dataSizes;

    ret = dataOp_->BatchDataCopy(params, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, batch_data_copy_local_device_to_global_host)
{
    InitMockEnv();
    // 测试批量数据拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_batch_copy_params params{};
    ock::mf::ExtOptions options{};

    // 准备批量拷贝参数
    void *sources[2] = {nullptr, nullptr};
    void *destinations[2] = {nullptr, nullptr};
    uint64_t dataSizes[2] = {1024ULL, 2048ULL};
    params.batchSize = 2UL;
    params.sources = sources;
    params.destinations = destinations;
    params.dataSizes = dataSizes;

    ret = dataOp_->BatchDataCopy(params, HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, batch_data_copy_global_host_to_local_device)
{
    InitMockEnv();
    // 测试批量数据拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_batch_copy_params params{};
    ock::mf::ExtOptions options{};

    // 准备批量拷贝参数
    void *sources[2] = {nullptr, nullptr};
    void *destinations[2] = {nullptr, nullptr};
    uint64_t dataSizes[2] = {1024ULL, 2048ULL};
    params.batchSize = 2UL;
    params.sources = sources;
    params.destinations = destinations;
    params.dataSizes = dataSizes;

    ret = dataOp_->BatchDataCopy(params, HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, batch_data_copy_local_device_to_global_device)
{
    InitMockEnv();
    // 测试批量数据拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_batch_copy_params params{};
    ock::mf::ExtOptions options{};

    // 准备批量拷贝参数
    void *sources[2] = {nullptr, nullptr};
    void *destinations[2] = {nullptr, nullptr};
    uint64_t dataSizes[2] = {1024ULL, 2048ULL};
    params.batchSize = 2UL;
    params.sources = sources;
    params.destinations = destinations;
    params.dataSizes = dataSizes;

    ret = dataOp_->BatchDataCopy(params, HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, batch_data_copy_global_device_to_local_device)
{
    InitMockEnv();
    // 测试批量数据拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_batch_copy_params params{};
    ock::mf::ExtOptions options{};

    // 准备批量拷贝参数
    void *sources[2] = {nullptr, nullptr};
    void *destinations[2] = {nullptr, nullptr};
    uint64_t dataSizes[2] = {1024ULL, 2048ULL};
    params.batchSize = 2UL;
    params.sources = sources;
    params.destinations = destinations;
    params.dataSizes = dataSizes;

    ret = dataOp_->BatchDataCopy(params, HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, batch_data_copy_local_host_to_global_host)
{
    InitMockEnv();
    // 测试批量数据拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_batch_copy_params params{};
    ock::mf::ExtOptions options{};

    // 准备批量拷贝参数
    void *sources[2] = {nullptr, nullptr};
    void *destinations[2] = {nullptr, nullptr};
    uint64_t dataSizes[2] = {1024ULL, 2048ULL};
    params.batchSize = 2UL;
    params.sources = sources;
    params.destinations = destinations;
    params.dataSizes = dataSizes;

    ret = dataOp_->BatchDataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, batch_data_copy_global_host_to_local_host)
{
    InitMockEnv();
    // 测试批量数据拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_batch_copy_params params{};
    ock::mf::ExtOptions options{};

    // 准备批量拷贝参数
    void *sources[2] = {nullptr, nullptr};
    void *destinations[2] = {nullptr, nullptr};
    uint64_t dataSizes[2] = {1024ULL, 2048ULL};
    params.batchSize = 2UL;
    params.sources = sources;
    params.destinations = destinations;
    params.dataSizes = dataSizes;

    ret = dataOp_->BatchDataCopy(params, HYBM_GLOBAL_HOST_TO_LOCAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, batch_data_copy_global_device_to_global_host)
{
    InitMockEnv();
    // 测试批量数据拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_batch_copy_params params{};
    ock::mf::ExtOptions options{};

    // 准备批量拷贝参数
    void *sources[2] = {nullptr, nullptr};
    void *destinations[2] = {nullptr, nullptr};
    uint64_t dataSizes[2] = {1024ULL, 2048ULL};
    params.batchSize = 2UL;
    params.sources = sources;
    params.destinations = destinations;
    params.dataSizes = dataSizes;

    ret = dataOp_->BatchDataCopy(params, HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, batch_data_copy_extend)
{
    InitMockEnv();
    // 测试批量数据拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_batch_copy_params params{};
    ock::mf::ExtOptions options{};
    options.flags |= COPY_EXTEND_FLAG;
    options.flags |= ASYNC_COPY_FLAG;

    // 准备批量拷贝参数
    void *sources[2] = {nullptr, nullptr};
    void *destinations[2] = {nullptr, nullptr};
    uint64_t dataSizes[2] = {1024ULL, 2048ULL};
    params.batchSize = 2UL;
    params.sources = sources;
    params.destinations = destinations;
    params.dataSizes = dataSizes;

    ret = dataOp_->BatchDataCopy(params, HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);
}

TEST_F(HybmDataOpSdmaTest, quant_copy)
{
    InitMockEnv();
    // 测试量化拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    // 构造有效的参数但设置 stream 为 nullptr 来触发错误
    hybm_quant_copy_params params{};
    params.batchSize = 1UL;

    // 分配有效内存
    void *source = (void *)malloc(1024ULL);
    void *dest = (void *)malloc(1024ULL);
    uint64_t dataSize = 1024ULL;
    void *scale = (void *)malloc(4ULL);  // 4 bytes for float
    void *offset = (void *)malloc(4ULL); // 4 bytes for float

    // 设置值
    *reinterpret_cast<float *>(scale) = 1.0f;
    *reinterpret_cast<float *>(offset) = 0.0f;

    // 准备数组指针
    void *sources[] = {source};
    void *destinations[] = {dest};
    uint64_t dataSizes[] = {dataSize};
    void *scales[] = {scale};
    void *offsets[] = {offset};

    params.sources = sources;
    params.destinations = destinations;
    params.dataSizes = dataSizes;
    params.scale = scales;
    params.offset = offsets;
    params.unitNum = 1UL;
    params.inputType = 0;
    params.stream = nullptr;
    params.flags = 0;

    ret = dataOp_->QuantCopy(params);
    // 所有 mock 函数都返回成功，所以应该返回 BM_OK
    ASSERT_EQ(BM_OK, ret);

    // 释放内存
    free(source);
    free(dest);
    free(scale);
    free(offset);
}
