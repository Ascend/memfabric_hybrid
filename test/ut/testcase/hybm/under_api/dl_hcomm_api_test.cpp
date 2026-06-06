/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 */

#include <gtest/gtest.h>

#define private public
#include "dl_hcomm_api.h"
#undef private

using namespace ock::mf;

namespace {
int32_t MockHcommEndpointCreate(const EndpointDesc *, EndpointHandle *)
{
    return BM_OK;
}

int32_t MockHcommEndpointDestroy(EndpointHandle)
{
    return BM_OK;
}

struct DlHcommApiStateGuard {
    bool oldLoaded{DlHcommApi::gLoaded};
    void *oldHandle{DlHcommApi::hcommHandle};
    const char *oldLibName{DlHcommApi::hcommLibName};

    ~DlHcommApiStateGuard()
    {
        DlHcommApi::gLoaded = oldLoaded;
        DlHcommApi::hcommHandle = oldHandle;
        DlHcommApi::hcommLibName = oldLibName;
        DlHcommApi::gHcommEndpointCreate = nullptr;
        DlHcommApi::gHcommEndpointDestroy = nullptr;
    }
};
} // namespace

TEST(DlHcommApiTest, LoadLibraryReturnsOkWhenAlreadyLoaded)
{
    DlHcommApiStateGuard guard;
    DlHcommApi::gLoaded = true;
    DlHcommApi::hcommHandle = nullptr;

    EXPECT_EQ(DlHcommApi::LoadLibrary(), BM_OK);
}

TEST(DlHcommApiTest, LoadLibraryReturnsFailureWhenLibraryMissing)
{
    DlHcommApiStateGuard guard;
    DlHcommApi::gLoaded = false;
    DlHcommApi::hcommHandle = nullptr;
    DlHcommApi::hcommLibName = "libhcomm_not_exist_for_ut.so";

    EXPECT_EQ(DlHcommApi::LoadLibrary(), BM_DL_FUNCTION_FAILED);
    EXPECT_FALSE(DlHcommApi::gLoaded);
}

TEST(DlHcommApiTest, CleanupLibraryReturnsDirectlyWhenNotLoaded)
{
    DlHcommApiStateGuard guard;
    DlHcommApi::gLoaded = false;
    DlHcommApi::gHcommEndpointCreate = MockHcommEndpointCreate;

    DlHcommApi::CleanupLibrary();
    EXPECT_EQ(DlHcommApi::gHcommEndpointCreate, MockHcommEndpointCreate);
}

TEST(DlHcommApiTest, CleanupLibraryResetsLoadedStateAndFunctionPointers)
{
    DlHcommApiStateGuard guard;
    DlHcommApi::gLoaded = true;
    DlHcommApi::hcommHandle = nullptr;
    DlHcommApi::gHcommEndpointCreate = MockHcommEndpointCreate;
    DlHcommApi::gHcommEndpointDestroy = MockHcommEndpointDestroy;

    DlHcommApi::CleanupLibrary();
    EXPECT_FALSE(DlHcommApi::gLoaded);
    EXPECT_EQ(DlHcommApi::hcommHandle, nullptr);
    EXPECT_EQ(DlHcommApi::gHcommEndpointCreate, nullptr);
    EXPECT_EQ(DlHcommApi::gHcommEndpointDestroy, nullptr);
}
