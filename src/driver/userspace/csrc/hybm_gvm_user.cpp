/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
#include "hybm_gvm_user.h"

#include <algorithm>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "hybm_gvm_user_def.h"
#include "hybm_gvm_cmd.h"
#include "hybm_gvm_vir_page_manager.h"
#include "hybm_user_logger.h"

using namespace ock::mf;

static int g_hybm_fd = -1;
static uint32_t g_svspid = 0;
static uint32_t g_sdid = 0;

/**
 * Copy C-style string, ensure dst to be `NULL` terminated, and no dirty data after the `NULL` terminator.
 * @param src A C-style string to copy from
 * @param src_size The capacity of the src string, including the `NULL` terminator.
 * @param dst A C-style string to copy into
 * @param dst_size The capacity of the dst buffer, including the `NULL` terminator.
 */
static inline void copy(const char *src, size_t src_size, char *dst, size_t dst_size)
{
    auto copy_size = std::min(strnlen(src, src_size - 1) + 1, sizeof(dst_size) - 1);
    std::copy_n(src, copy_size, dst);
    std::fill(dst + copy_size, dst + dst_size, '\0');
}

static void hybm_davinci_close(int fd, const char *davinci_sub_name)
{
    struct davinci_intf_close_arg arg = {{0}, 0};
    copy(davinci_sub_name, DAVINIC_MODULE_NAME_MAX, arg.module_name, sizeof(arg.module_name));
    auto ret = ioctl(fd, DAVINCI_INTF_IOCTL_CLOSE, &arg);
    if (ret != 0) {
        BM_USER_LOG_ERROR("Davinci close fail. ret:" << ret << " errno:" << errno);
    }
}

static int32_t hybm_davinci_open(int fd, const char *davinci_sub_name)
{
    struct davinci_intf_open_arg arg = {{0}, 0};
    copy(davinci_sub_name, DAVINIC_MODULE_NAME_MAX, arg.module_name, sizeof(arg.module_name));
    auto ret = ioctl(fd, DAVINCI_INTF_IOCTL_OPEN, &arg);
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

    arg.data.proc_create_para.start = HYBM_GVM_START_ADDR;
    arg.data.proc_create_para.end = HYBM_GVM_START_ADDR + HYBM_GVM_RESERVE_SIZE;
    arg.data.proc_create_para.devid = device_id;
    ret = ioctl(g_hybm_fd, HYBM_GVM_CMD_PROC_CREATE, &arg);
    if (ret < 0) {
        BM_USER_LOG_ERROR("ioctl HYBM_GVM_CMD_PROC_CREATE failed, ret:" << ret);
        hybm_gvm_deinit();
        return HYBM_GVM_FAILURE;
    }

    ret = HybmGvmVirPageManager::Instance()->Initialize(HYBM_GVM_START_ADDR, HYBM_GVM_RESERVE_SIZE, g_hybm_fd);
    if (ret != 0) {
        BM_USER_LOG_ERROR("init virtual page manager failed, ret:" << ret);
        hybm_gvm_deinit();
        return HYBM_GVM_FAILURE;
    }
    g_svspid = arg.data.proc_create_para.svspid;
    g_sdid = arg.data.proc_create_para.sdid;
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
    if (ssid == nullptr) {
        BM_USER_LOG_ERROR("input is nullptr");
        return HYBM_GVM_FAILURE;
    }
    if (g_hybm_fd < 0) {
        BM_USER_LOG_ERROR("hybm gvm module has not been initialized");
        return HYBM_GVM_FAILURE;
    }

    if ((g_sdid >> GVM_SDID_SERVER_OFFSET) == GVM_SDID_UNKNOWN_SERVER_ID) {
        BM_USER_LOG_INFO("found A2 server, return zero!");
        *ssid = 0;
        return HYBM_GVM_SUCCESS;
    }

    *ssid = g_svspid;
    return HYBM_GVM_SUCCESS;
}

int32_t hybm_gvm_reserve_memory(uint64_t *addr, uint64_t size, bool shared)
{
    if (g_hybm_fd < 0) {
        BM_USER_LOG_ERROR("hybm gvm module has not been initialized");
        return HYBM_GVM_FAILURE;
    }
    if (addr == nullptr || size == 0) {
        BM_USER_LOG_ERROR("Invalid param, addr is nullptr or size is empty, size:" << size);
        return HYBM_GVM_FAILURE;
    }
    auto ret = HybmGvmVirPageManager::Instance()->ReserveMemory(addr, size, shared);
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

int32_t hybm_gvm_mem_fetch(uint64_t addr, uint64_t size, uint32_t sdid)
{
    if ((g_sdid >> GVM_SDID_SERVER_OFFSET) == GVM_SDID_UNKNOWN_SERVER_ID) {
        BM_USER_LOG_INFO("found A2 server, skip fetch mem!");
        return HYBM_GVM_SUCCESS;
    }

    int ret;
    struct hybm_gvm_ioctl_arg arg = {};
    if (g_hybm_fd < 0) {
        BM_USER_LOG_ERROR("hybm gvm module has not been initialized");
        return HYBM_GVM_FAILURE;
    }
    if (addr == 0 || size == 0 || size > HYBM_GVM_RESERVE_SIZE || size % SIZE_1G) {
        BM_USER_LOG_ERROR("Invalid param, addr is nullptr or size is invalid, size:" << size);
        return HYBM_GVM_FAILURE;
    }

    if (sdid == 0) {
        sdid = g_sdid;
    }

    for (uint64_t i = 0; i < size; i += SIZE_1G) {
        BM_USER_ASSERT_RETURN(std::numeric_limits<uint64_t>::max() - addr >= i, HYBM_GVM_FAILURE);
        arg.data.mem_fetch_para.addr = addr + i;
        arg.data.mem_fetch_para.size = SIZE_1G;
        arg.data.mem_fetch_para.sdid = sdid;
        arg.data.mem_fetch_para.no_record = false;
        ret = ioctl(g_hybm_fd, HYBM_GVM_CMD_MEM_FETCH, &arg);
        if (ret < 0) {
            BM_USER_LOG_ERROR("ioctl HYBM_GVM_CMD_MEM_FETCH failed, ret:" << ret << " size:" << size);
            return HYBM_GVM_FAILURE;
        }
    }
    BM_USER_LOG_INFO("ioctl HYBM_GVM_CMD_MEM_FETCH success, size:" << size);
    return HYBM_GVM_SUCCESS;
}

int32_t hybm_gvm_mem_register(uint64_t addr, uint64_t size, uint64_t new_addr)
{
    if ((g_sdid >> GVM_SDID_SERVER_OFFSET) == GVM_SDID_UNKNOWN_SERVER_ID) {
        BM_USER_LOG_INFO("found A2 server, skip register mem!");
        HybmGvmVirPageManager::Instance()->UpdateRegisterMap(addr, size, new_addr);
        return HYBM_GVM_SUCCESS;
    }

    int ret;
    struct hybm_gvm_ioctl_arg arg = {};
    if (g_hybm_fd < 0) {
        BM_USER_LOG_ERROR("hybm gvm module has not been initialized");
        return HYBM_GVM_FAILURE;
    }
    if (addr == 0 || HybmGvmVirPageManager::Instance()->QueryInRegisterMap(addr, size)) {
        BM_USER_LOG_ERROR("Invalid param addr or size, size:" << size);
        return HYBM_GVM_FAILURE;
    }

    arg.data.mem_fetch_para.addr = addr;
    arg.data.mem_fetch_para.size = size;
    arg.data.mem_fetch_para.new_addr = new_addr;
    arg.data.mem_fetch_para.sdid = g_sdid;
    arg.data.mem_fetch_para.no_record = true;
    ret = ioctl(g_hybm_fd, HYBM_GVM_CMD_MEM_FETCH, &arg);
    if (ret < 0) {
        BM_USER_LOG_ERROR("ioctl HYBM_GVM_CMD_MEM_FETCH failed, ret:" << ret << " size:" << size);
        return HYBM_GVM_FAILURE;
    }

    if (!HybmGvmVirPageManager::Instance()->UpdateRegisterMap(addr, size, new_addr)) {
        BM_USER_LOG_INFO("insert register set failed, size:" << size);
        return HYBM_GVM_FAILURE;
    } else {
        BM_USER_LOG_INFO("insert register addr " << std::hex << addr << " size:" << size
                                                 << " new:" << std::hex << new_addr);
        return HYBM_GVM_SUCCESS;
    }
}

uint64_t hybm_gvm_mem_has_registered(uint64_t addr, uint64_t size)
{
    return HybmGvmVirPageManager::Instance()->QueryInRegisterMap(addr, size);
}

int32_t hybm_gvm_mem_alloc(uint64_t addr, uint64_t size)
{
    int ret;
    struct hybm_gvm_ioctl_arg arg = {};
    if (g_hybm_fd < 0) {
        BM_USER_LOG_ERROR("hybm gvm module has not been initialized");
        return HYBM_GVM_FAILURE;
    }
    if (addr == 0 || size == 0) {
        BM_USER_LOG_ERROR("Invalid param addr or size, size:" << size);
        return HYBM_GVM_FAILURE;
    }
    arg.data.mem_alloc_para.addr = addr;
    arg.data.mem_alloc_para.size = size;
    arg.data.mem_alloc_para.dma_flag = (addr < HYBM_GVM_SDMA_START_ADDR);
    ret = ioctl(g_hybm_fd, HYBM_GVM_CMD_MEM_ALLOC, &arg);
    if (ret < 0) {
        BM_USER_LOG_ERROR("ioctl HYBM_GVM_CMD_MEM_ALLOC failed, ret:" << ret << " size:" << size);
        return HYBM_GVM_FAILURE;
    }

    BM_USER_LOG_INFO("ioctl HYBM_GVM_CMD_MEM_ALLOC success, size:" << size);
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
        BM_USER_LOG_ERROR("Invalid param, addr is nullptr");
        return HYBM_GVM_FAILURE;
    }
    arg.data.mem_free_para.addr = addr;
    ret = ioctl(g_hybm_fd, HYBM_GVM_CMD_MEM_FREE, &arg);
    if (ret < 0) {
        BM_USER_LOG_ERROR("ioctl HYBM_GVM_CMD_MEM_FREE failed, ret:" << ret);
        return HYBM_GVM_FAILURE;
    }
    BM_USER_LOG_INFO("ioctl HYBM_GVM_CMD_MEM_FREE success");
    return HYBM_GVM_SUCCESS;
}

int32_t hybm_gvm_get_key(uint64_t addr, uint64_t *key)
{
    if ((g_sdid >> GVM_SDID_SERVER_OFFSET) == GVM_SDID_UNKNOWN_SERVER_ID) {
        BM_USER_LOG_INFO("found A2 server, skip get key!");
        return HYBM_GVM_SUCCESS;
    }

    int ret;
    struct hybm_gvm_ioctl_arg arg = {};
    if (g_hybm_fd < 0) {
        BM_USER_LOG_ERROR("hybm gvm module has not been initialized");
        return HYBM_GVM_FAILURE;
    }
    if (addr == 0 || key == nullptr) {
        BM_USER_LOG_ERROR("Invalid param addr or key is nullptr");
        return HYBM_GVM_FAILURE;
    }
    arg.data.get_key_para.addr = addr;
    ret = ioctl(g_hybm_fd, HYBM_GVM_CMD_GET_KEY, &arg);
    if (ret < 0) {
        BM_USER_LOG_ERROR("ioctl HYBM_GVM_CMD_GET_KEY failed, ret:" << ret);
        return HYBM_GVM_FAILURE;
    }
    *key = arg.data.get_key_para.key;
    BM_USER_LOG_INFO("ioctl HYBM_GVM_CMD_GET_KEY success, key:" << (*key));
    return HYBM_GVM_SUCCESS;
}

int32_t hybm_gvm_set_whitelist(uint64_t key, uint32_t sdid)
{
    if ((g_sdid >> GVM_SDID_SERVER_OFFSET) == GVM_SDID_UNKNOWN_SERVER_ID) {
        BM_USER_LOG_INFO("found A2 server, skip set wl!");
        return HYBM_GVM_SUCCESS;
    }

    int ret;
    struct hybm_gvm_ioctl_arg arg = {};
    if (g_hybm_fd < 0) {
        BM_USER_LOG_ERROR("hybm gvm module has not been initialized");
        return HYBM_GVM_FAILURE;
    }
    if (sdid == 0 || key == 0) {
        BM_USER_LOG_ERROR("Invalid param sdid:" << std::hex << sdid << " key:" << key);
        return HYBM_GVM_FAILURE;
    }
    arg.data.set_wl_para.key = key;
    arg.data.set_wl_para.sdid = sdid;
    ret = ioctl(g_hybm_fd, HYBM_GVM_CMD_SET_WL, &arg);
    if (ret < 0) {
        BM_USER_LOG_ERROR("ioctl HYBM_GVM_CMD_SET_WL failed, ret:" << ret << " sdid:" << std::hex << sdid
                                                                   << " key:" << key);
        return HYBM_GVM_FAILURE;
    }
    BM_USER_LOG_INFO("ioctl HYBM_GVM_CMD_SET_WL success, sdid:" << std::hex << sdid << " key:" << key);
    return HYBM_GVM_SUCCESS;
}

int32_t hybm_gvm_mem_open(uint64_t addr, uint64_t key)
{
    if ((g_sdid >> GVM_SDID_SERVER_OFFSET) == GVM_SDID_UNKNOWN_SERVER_ID) {
        BM_USER_LOG_INFO("found A2 server, skip open!");
        return HYBM_GVM_SUCCESS;
    }

    int ret;
    struct hybm_gvm_ioctl_arg arg = {};
    if (g_hybm_fd < 0) {
        BM_USER_LOG_ERROR("hybm gvm module has not been initialized");
        return HYBM_GVM_FAILURE;
    }
    if (addr == 0 || key == 0) {
        BM_USER_LOG_ERROR("Invalid param addr or key is empty, key:" << key);
        return HYBM_GVM_FAILURE;
    }
    arg.data.mem_open_para.addr = addr;
    arg.data.mem_open_para.key = key;
    ret = ioctl(g_hybm_fd, HYBM_GVM_CMD_MEM_OPEN, &arg);
    if (ret < 0) {
        BM_USER_LOG_ERROR("ioctl HYBM_GVM_CMD_MEM_OPEN failed, ret:" << ret << " key:" << key);
        return HYBM_GVM_FAILURE;
    }
    BM_USER_LOG_INFO("ioctl HYBM_GVM_CMD_MEM_OPEN success, key:" << key);
    return HYBM_GVM_SUCCESS;
}

int32_t hybm_gvm_mem_close(uint64_t addr)
{
    if ((g_sdid >> GVM_SDID_SERVER_OFFSET) == GVM_SDID_UNKNOWN_SERVER_ID) {
        BM_USER_LOG_INFO("found A2 server, skip close!");
        return HYBM_GVM_SUCCESS;
    }

    int ret;
    struct hybm_gvm_ioctl_arg arg = {};
    if (g_hybm_fd < 0) {
        BM_USER_LOG_ERROR("hybm gvm module has not been initialized");
        return HYBM_GVM_FAILURE;
    }
    if (addr == 0) {
        BM_USER_LOG_ERROR("Invalid param addr is nullptr");
        return HYBM_GVM_FAILURE;
    }
    arg.data.mem_close_para.addr = addr;
    ret = ioctl(g_hybm_fd, HYBM_GVM_CMD_MEM_CLOSE, &arg);
    if (ret < 0) {
        BM_USER_LOG_ERROR("ioctl HYBM_GVM_CMD_MEM_CLOSE failed, ret:" << ret);
        return HYBM_GVM_FAILURE;
    }
    BM_USER_LOG_INFO("ioctl HYBM_GVM_CMD_MEM_CLOSE success");
    return HYBM_GVM_SUCCESS;
}

uint64_t hybm_gvm_user_alloc(uint64_t size)
{
    uint64_t len = (size + SIZE_1G - 1U) / SIZE_1G * SIZE_1G;
    uint64_t va = 0U;
    int32_t ret = hybm_gvm_reserve_memory(&va, len, false);
    if (ret != 0) {
        BM_USER_LOG_ERROR("alloc va failed, ret:" << ret);
        return HYBM_GVM_FAILURE;
    }

    ret = hybm_gvm_mem_alloc(va, len);
    if (ret != 0) {
        BM_USER_LOG_ERROR("va map failed, ret:" << ret);
        return HYBM_GVM_FAILURE;
    }

    return va;
}

void hybm_gvm_user_free(uint64_t addr)
{
    int32_t ret = hybm_gvm_mem_free(addr);
    if (ret != 0) {
        BM_USER_LOG_ERROR("free mem failed, ret:" << ret);
    }
}