/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
#include "hybm_gvm_user.h"

#include <algorithm>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "hybm_gva_user_def.h"
#include "hybm_gvm_cmd.h"
#include "hybm_gvm_vir_page_manager.h"
#include "hybm_user_logger.h"

using namespace ock::mf;

static int g_hybm_fd = -1;
static uint32_t g_svspid = 0;

static void hybm_davinci_close(int fd, const char *davinci_sub_name)
{
    struct davinci_intf_close_arg arg = {{0}, 0};
    int ret;
    (void)std::copy_n(davinci_sub_name, DAVINIC_MODULE_NAME_MAX, arg.module_name);
    ret = ioctl(fd, DAVINCI_INTF_IOCTL_CLOSE, &arg);
    if (ret != 0) {
        BM_USER_LOG_ERROR("Davinci close fail. ret:" << ret << " errno:" << errno);
    }
}

static int32_t hybm_davinci_open(int fd, const char *davinci_sub_name)
{
    struct davinci_intf_open_arg arg = {{0}, 0};
    int ret;
    (void)std::copy_n(davinci_sub_name, DAVINIC_MODULE_NAME_MAX, arg.module_name);
    ret = ioctl(fd, DAVINCI_INTF_IOCTL_OPEN, &arg);
    if (ret != 0) {
        BM_USER_LOG_ERROR("DAVINCI_INTF_IOCTL_OPEN failed. ret:" << ret << " errno:" << errno);
        return -1;
    }

    return 0;
}

static int hybm_gvm_open_proc(const char *davinci_sub_name)
{
    int fd = -1;
    int ret = 0;

    fd = open(davinci_intf_get_dev_path(), O_RDWR | O_SYNC | O_CLOEXEC);
    if (fd < 0) {
        BM_USER_LOG_ERROR("Open device fail. fd:" << fd << " chardev:" << davinci_intf_get_dev_path());
        return fd;
    }

    ret = hybm_davinci_open(fd, davinci_sub_name);
    if (ret != 0) {
        (void)close(fd);
        BM_USER_LOG_ERROR("Davinci open memory device fail. ret:" << ret << " sub_name:" << davinci_sub_name);
        return -1;
    }
    return fd;
}

int32_t hybm_gvm_init(uint64_t device_id)
{
    int ret;
    struct hybm_gvm_ioctl_arg arg = {};
    if (g_hybm_fd >= 0) {
        BM_USER_LOG_INFO("hybm gvm module has already been initialized");
        return HYBM_GVM_FAILURE;
    }

    g_hybm_fd = hybm_gvm_open_proc(DAVINCI_GVM_SUB_MODULE_NAME);
    if (g_hybm_fd < 0) {
        BM_USER_LOG_ERROR("open davinci sub module " DAVINCI_GVM_SUB_MODULE_NAME " failed, fd:" << g_hybm_fd);
        return HYBM_GVM_FAILURE;
    }

    arg.data.proc_create_para.start = HYBM_GVA_START_ADDR;
    arg.data.proc_create_para.end = HYBM_GVA_START_ADDR + HYBM_GVA_RESERVE_SIZE;
    arg.data.proc_create_para.devid = device_id;
    ret = ioctl(g_hybm_fd, HYBM_GVM_CMD_PROC_CREATE, &arg);
    if (ret < 0) {
        BM_USER_LOG_ERROR("ioctl HYBM_GVM_CMD_PROC_CREATE failed, ret:" << ret);
        hybm_gvm_deinit();
        return HYBM_GVM_FAILURE;
    }

    ret = HybmGvmVirPageManager::Instance()->Initialize(HYBM_GVA_START_ADDR, HYBM_GVA_RESERVE_SIZE, g_hybm_fd);
    if (ret != 0) {
        BM_USER_LOG_ERROR("init virtual page manager failed, ret:" << ret);
        hybm_gvm_deinit();
        return HYBM_GVM_FAILURE;
    }
    g_svspid = arg.data.proc_create_para.svspid;
    BM_USER_LOG_INFO("hybm gvm module init success.");
    return HYBM_GVM_SUCCESS;
}

void hybm_gvm_deinit(void)
{
    if (g_hybm_fd < 0) {
        BM_USER_LOG_INFO("hybm gvm module has already been deinitialized");
        return;
    }
    hybm_davinci_close(g_hybm_fd, DAVINCI_GVM_SUB_MODULE_NAME);
    g_hybm_fd = -1;
}

int32_t hybm_gvm_get_device_info(uint32_t *ssid)
{
    if (g_hybm_fd < 0) {
        BM_USER_LOG_ERROR("hybm gvm module has not been initialized");
        return HYBM_GVM_FAILURE;
    }

    if (ssid == nullptr) {
        BM_USER_LOG_ERROR("input is nullptr");
        return HYBM_GVM_FAILURE;
    }

    *ssid = g_svspid;
    return HYBM_GVM_SUCCESS;
}

int32_t hybm_gvm_reserve_memory(uint64_t *addr, uint64_t size)
{
    if (g_hybm_fd < 0) {
        BM_USER_LOG_ERROR("hybm gvm module has not been initialized");
        return HYBM_GVM_FAILURE;
    }
    if (addr == nullptr || size == 0) {
        BM_USER_LOG_ERROR("Invalid param addr:" << std::hex << addr << " size:" << size);
        return HYBM_GVM_FAILURE;
    }
    auto ret = HybmGvmVirPageManager::Instance()->ReserveMemory(addr, size);
    if (ret != 0) {
        BM_USER_LOG_ERROR("Failed to reserve memory, size:" << std::hex << size);
        return HYBM_GVM_FAILURE;
    }
    return HYBM_GVM_SUCCESS;
}

int32_t hybm_gvm_unreserve_memory()
{
    return HYBM_GVM_SUCCESS;
}

int32_t hybm_gvm_mem_fetch(uint64_t addr, uint64_t size)
{
    int ret;
    struct hybm_gvm_ioctl_arg arg = {};
    if (g_hybm_fd < 0) {
        BM_USER_LOG_ERROR("hybm gvm module has not been initialized");
        return HYBM_GVM_FAILURE;
    }
    if (addr == 0 || size == 0) {
        BM_USER_LOG_ERROR("Invalid param addr:" << std::hex << addr << " size:" << size);
        return HYBM_GVM_FAILURE;
    }
    arg.data.mem_fetch_para.addr = addr;
    arg.data.mem_fetch_para.size = size;
    ret = ioctl(g_hybm_fd, HYBM_GVM_CMD_MEM_FETCH, &arg);
    if (ret < 0) {
        BM_USER_LOG_ERROR("ioctl HYBM_GVM_CMD_MEM_FETCH failed, ret:" << ret << " addr:" << std::hex << addr
                                                                      << " size:" << size);
        return HYBM_GVM_FAILURE;
    }
    BM_USER_LOG_INFO("ioctl HYBM_GVM_CMD_MEM_FETCH success, addr:" << std::hex << addr << " size:" << size);
    return HYBM_GVM_SUCCESS;
}

int32_t hybm_gvm_mem_alloc(uint64_t *addr, uint64_t size)
{
    int ret;
    struct hybm_gvm_ioctl_arg arg = {};
    if (g_hybm_fd < 0) {
        BM_USER_LOG_ERROR("hybm gvm module has not been initialized");
        return HYBM_GVM_FAILURE;
    }
    if (addr == nullptr || size == 0) {
        BM_USER_LOG_ERROR("Invalid param addr:" << addr << " size:" << size);
        return HYBM_GVM_FAILURE;
    }
    arg.data.mem_alloc_para.addr = *addr;
    arg.data.mem_alloc_para.size = size;
    ret = ioctl(g_hybm_fd, HYBM_GVM_CMD_MEM_ALLOC, &arg);
    if (ret < 0) {
        BM_USER_LOG_ERROR("ioctl HYBM_GVM_CMD_MEM_ALLOC failed, ret:" << ret << " addr:" << std::hex << (*addr)
                                                                      << " size:" << size);
        return HYBM_GVM_FAILURE;
    }

    if (*addr != 0ULL && *addr != arg.data.mem_alloc_para.ret_addr) {
        BM_USER_LOG_ERROR("ioctl HYBM_GVM_CMD_MEM_ALLOC failed, input:" << std::hex << (*addr)
                                                                        << " ret:" << arg.data.mem_alloc_para.ret_addr);
    }
    *addr = arg.data.mem_alloc_para.ret_addr;
    BM_USER_LOG_INFO("ioctl HYBM_GVM_CMD_MEM_ALLOC success, addr:" << std::hex << (*addr) << " size:" << size);
    return HYBM_GVM_SUCCESS;
}

int32_t hybm_gvm_mem_free(uint64_t addr)
{
    int ret;
    struct hybm_gvm_ioctl_arg arg = {};
    if (g_hybm_fd < 0) {
        BM_USER_LOG_ERROR("hybm gvm module has not been initialized");
        return HYBM_GVM_FAILURE;
    }
    if (addr == 0) {
        BM_USER_LOG_ERROR("Invalid param addr:" << std::hex << addr);
        return HYBM_GVM_FAILURE;
    }
    arg.data.mem_free_para.addr = addr;
    ret = ioctl(g_hybm_fd, HYBM_GVM_CMD_MEM_FREE, &arg);
    if (ret < 0) {
        BM_USER_LOG_ERROR("ioctl HYBM_GVM_CMD_MEM_FREE failed, ret:" << ret << " addr:" << std::hex << addr);
        return HYBM_GVM_FAILURE;
    }
    BM_USER_LOG_INFO("ioctl HYBM_GVM_CMD_MEM_FREE success, addr:" << std::hex << addr);
    return HYBM_GVM_SUCCESS;
}

int32_t hybm_gvm_get_key(uint64_t addr, uint64_t *key)
{
    int ret;
    struct hybm_gvm_ioctl_arg arg = {};
    if (g_hybm_fd < 0) {
        BM_USER_LOG_ERROR("hybm gvm module has not been initialized");
        return HYBM_GVM_FAILURE;
    }
    if (addr == 0 || key == nullptr) {
        BM_USER_LOG_ERROR("Invalid param addr:" << std::hex << addr << " key:" << key);
        return HYBM_GVM_FAILURE;
    }
    arg.data.get_key_para.addr = addr;
    ret = ioctl(g_hybm_fd, HYBM_GVM_CMD_GET_KEY, &arg);
    if (ret < 0) {
        BM_USER_LOG_ERROR("ioctl HYBM_GVM_CMD_GET_KEY failed, ret:" << ret << " addr:" << std::hex << addr);
        return HYBM_GVM_FAILURE;
    }
    *key = arg.data.get_key_para.key;
    BM_USER_LOG_INFO("ioctl HYBM_GVM_CMD_GET_KEY success, addr:" << std::hex << addr << " key:" << (*key));
    return HYBM_GVM_SUCCESS;
}

int32_t hybm_gvm_set_whitelist(uint64_t key, uint32_t sdid)
{
    int ret;
    struct hybm_gvm_ioctl_arg arg = {};
    if (g_hybm_fd < 0) {
        BM_USER_LOG_ERROR("hybm gvm module has not been initialized");
        return HYBM_GVM_FAILURE;
    }
    if (sdid == 0 || key == 0) {
        BM_USER_LOG_ERROR("Invalid param sdid:" << sdid << " key:" << key);
        return HYBM_GVM_FAILURE;
    }
    arg.data.set_wl_para.key = key;
    arg.data.set_wl_para.sdid = sdid;
    ret = ioctl(g_hybm_fd, HYBM_GVM_CMD_SET_WL, &arg);
    if (ret < 0) {
        BM_USER_LOG_ERROR("ioctl HYBM_GVM_CMD_SET_WL failed, ret:" << ret << " sdid:" << sdid << " key:" << std::hex
                                                                   << key);
        return HYBM_GVM_FAILURE;
    }
    BM_USER_LOG_INFO("ioctl HYBM_GVM_CMD_SET_WL success, sdid:" << sdid << " key:" << std::hex << key);
    return HYBM_GVM_SUCCESS;
}

int32_t hybm_gvm_mem_open(uint64_t addr, uint64_t key)
{
    int ret;
    struct hybm_gvm_ioctl_arg arg = {};
    if (g_hybm_fd < 0) {
        BM_USER_LOG_ERROR("hybm gvm module has not been initialized");
        return HYBM_GVM_FAILURE;
    }
    if (addr == 0 || key == 0) {
        BM_USER_LOG_ERROR("Invalid param addr:" << addr << " key:" << key);
        return HYBM_GVM_FAILURE;
    }
    arg.data.mem_open_para.addr = addr;
    arg.data.mem_open_para.key = key;
    ret = ioctl(g_hybm_fd, HYBM_GVM_CMD_MEM_OPEN, &arg);
    if (ret < 0) {
        BM_USER_LOG_ERROR("ioctl HYBM_GVM_CMD_MEM_OPEN failed, ret:" << ret << " addr:" << std::hex << addr
                                                                     << " key:" << key);
        return HYBM_GVM_FAILURE;
    }
    BM_USER_LOG_INFO("ioctl HYBM_GVM_CMD_MEM_OPEN success, addr:" << std::hex << addr << " key:" << key);
    return HYBM_GVM_SUCCESS;
}

int32_t hybm_gvm_mem_close(uint64_t addr)
{
    int ret;
    struct hybm_gvm_ioctl_arg arg = {};
    if (g_hybm_fd < 0) {
        BM_USER_LOG_ERROR("hybm gvm module has not been initialized");
        return HYBM_GVM_FAILURE;
    }
    if (addr == 0) {
        BM_USER_LOG_ERROR("Invalid param addr:" << std::hex << addr);
        return HYBM_GVM_FAILURE;
    }
    arg.data.mem_close_para.addr = addr;
    ret = ioctl(g_hybm_fd, HYBM_GVM_CMD_MEM_CLOSE, &arg);
    if (ret < 0) {
        BM_USER_LOG_ERROR("ioctl HYBM_GVM_CMD_MEM_CLOSE failed, ret:" << ret << " addr:" << std::hex << addr);
        return HYBM_GVM_FAILURE;
    }
    BM_USER_LOG_INFO("ioctl HYBM_GVM_CMD_MEM_CLOSE success, addr:" << std::hex << addr);
    return HYBM_GVM_SUCCESS;
}
