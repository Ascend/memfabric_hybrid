/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <sys/time.h>
#include <unistd.h>
#include <random>
#include <cstring>

#include "acl/acl.h"

#include "smem.h"
#include "smem_shm.h"
#include "smem_bm.h"
#include "smem_trans.h"

#define CHECK_ACL(x)                                                                        \
    do {                                                                                    \
        aclError __ret = x;                                                                 \
        if (__ret != ACL_ERROR_NONE) {                                                      \
            std::cerr << __FILE__ << ":" << __LINE__ << " aclError:" << __ret << std::endl; \
        }                                                                                   \
    } while (0);

#define LOG_ERROR(msg) std::cout << __FILE__ << ":" << __LINE__ << "[ERR]" << msg << std::endl;

#define CHECK_GOTO_ERR(x, msg, lable)   \
do {                            \
    if ((x) != 0) {             \
        LOG_ERROR(msg);         \
        goto lable;              \
    }                           \
} while (0)

const uint64_t GVA_SIZE = 1024ULL * 1024 * 1024;
constexpr uint32_t MAX_UINT32 = 0xFFFFFFFF;

const static std::map<std::string, uint64_t> RATE_UNIT_MP = {
    {"GB", 1000ull * 1000ull * 1000ull},
    {"GiB", 1ull << 30},
    {"Gb", 1000ull * 1000ull * 1000ull / 8},
    {"MB", 1000ull * 1000ull},
    {"MiB", 1ull << 20},
    {"Mb", 1000ull * 1000ull / 8},
    {"KB", 1000ull},
    {"KiB", 1ull << 10},
    {"Kb", 1000ull / 8}};

static inline std::string calculateRate(uint64_t data_bytes,
                                        double duration)
{
    const uint64_t MEGABYTES_PER_BYTE = 1000000;
    std::string report_unit = "GB";
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2U)
        << 1.0 * data_bytes * MEGABYTES_PER_BYTE / duration / RATE_UNIT_MP.at(report_unit)
        << " " << report_unit << "/s";
    return oss.str();
}

static inline void init_warmup_data(char *&warmup_data, size_t length)
{
    const size_t STEP_SIZE = 4;
    uint64_t *p;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dis(0, MAX_UINT32);
    p = reinterpret_cast<uint64_t*>(warmup_data);
    for (uint64_t i = 0; i < length; i += STEP_SIZE) {
        p = reinterpret_cast<uint64_t*>(&warmup_data[i]);
        *p = static_cast<uint64_t>(dis(gen));
    }
}

int32_t bm_perf_test(smem_bm_t bm_handle, int rankId)
{
    char *warmup_data = nullptr;
    int32_t ret;
    const uint32_t KB_SIZE = 1024;

    if (rankId == 0) {
        uint32_t block_iteration = 10;
        uint32_t base_block_size = 32 << 10;    // 32k
        uint32_t times = 10;
        uint32_t batch_size = 32;

        /* init warmup data */
        warmup_data = (char *)malloc(GVA_SIZE * sizeof(char));
        if (!warmup_data) {
            std::cout << "warmup data malloc failed" << std::endl;
            goto out;
        }

        init_warmup_data(warmup_data, GVA_SIZE);

        void *local_addr = smem_bm_ptr(bm_handle, 0);
        void *remote_addr = smem_bm_ptr(bm_handle, 1);
        // warmup
        std::cout << "Warmup Start" << std::endl;
        smem_copy_params copy_params_1 = {warmup_data, local_addr, GVA_SIZE};
        ret = smem_bm_copy(bm_handle, &copy_params_1, SMEMB_COPY_H2G, 0);
        CHECK_GOTO_ERR(ret, "copy host to gva failed, ret:" << ret << " rank:" << rankId, out);
        smem_copy_params copy_params_2 = {local_addr, remote_addr, GVA_SIZE / 16};
        ret = smem_bm_copy(bm_handle, &copy_params_2, SMEMB_COPY_L2G, 0);
        CHECK_GOTO_ERR(ret, "copy host to gva failed, ret:" << ret << " rank:" << rankId, out);
        std::cout << "Warmup End" << std::endl;

        std::cout << "Test Start" << std::endl;
        for (uint32_t i = 0; i < block_iteration; i++) {
            uint32_t block_size = base_block_size * (1 << i);
            struct timeval start_tv;
            struct timeval stop_tv;
            gettimeofday(&start_tv, nullptr);
            /* latency test */
            smem_copy_params copy_params = {local_addr, remote_addr, block_size};
            for (uint32_t j = 0; j < times; j++) {
                ret = smem_bm_copy(bm_handle, &copy_params, SMEMB_COPY_L2G, 0);
                CHECK_GOTO_ERR(ret, "copy host to gva failed, ret:" << ret << " rank:" << rankId, out);
            }

            gettimeofday(&stop_tv, nullptr);
            double duration1 = (stop_tv.tv_sec - start_tv.tv_sec) * 1000000.0 +
                                (stop_tv.tv_usec - start_tv.tv_usec);
            duration1 /= times;
            
            /* bw test */
            std::vector<void *> laddrv;
            std::vector<void *> raddrv;
            std::vector<uint64_t> lengthv;
            laddrv.reserve(batch_size);
            raddrv.reserve(batch_size);
            lengthv.reserve(batch_size);
            for (uint32_t j = 0; j < batch_size; j++) {
                void *laddr = (uint8_t *)local_addr + j * block_size;
                void *raddr = (uint8_t *)remote_addr + j * block_size;
                laddrv.push_back(laddr);
                raddrv.push_back(raddr);
                lengthv.push_back(block_size);
            }
            gettimeofday(&start_tv, nullptr);
            smem_batch_copy_params batch_params = {const_cast<const void**>(laddrv.data()), raddrv.data(),
                                                   (uint64_t *)lengthv.data(), static_cast<uint32_t>(lengthv.size())};
            for (uint32_t j = 0; j < times; j++) {
                smem_bm_copy_batch(bm_handle, &batch_params, SMEMB_COPY_L2G, 0);
            }

            gettimeofday(&stop_tv, nullptr);
            double duration2 = (stop_tv.tv_sec - start_tv.tv_sec) * 1000000.0 +
                                (stop_tv.tv_usec - start_tv.tv_usec);
            duration2 /= times;
            std::cout << "Test completed: latency " << duration1
                    << "us, block size " << block_size / KB_SIZE << "KB, total size "
                    << batch_size * block_size / KB_SIZE << "KB , throughput "
                    << calculateRate(batch_size * block_size, duration2) << std::endl;
        }
        std::cout << "Test End" << std::endl;
    }

out:
    if (warmup_data) {
        free(warmup_data);
    }

    return 0;
}

int32_t trans_perf_test(smem_trans_t trans_handle, smem_shm_t shm_handle, int rankId)
{
    char *warmup_data = nullptr;
    int32_t ret;
    void *dev_addr = nullptr;
    void *gather_addr[2];
    void *dst_dev_addr = nullptr;
    const uint32_t KB_SIZE = 1024;

    // malloc device mem
    aclError aclret = aclrtMalloc(&dev_addr, GVA_SIZE, ACL_MEM_MALLOC_HUGE_FIRST);
    CHECK_GOTO_ERR(aclret != ACL_ERROR_NONE, "failed to allocate device memory, ret:" << aclret, out);
    std::cout << "[" << rankId << "]" << " malloc dev mem " << dev_addr << std::endl;

    /* gather peer addr */
    ret = smem_shm_control_allgather(shm_handle, (char *)&dev_addr,
                                     sizeof(void *), (char *)gather_addr, sizeof(void *) * 2U);
    CHECK_GOTO_ERR(ret, "failed to allgather dev memory, ret:" << ret, out);

    if (rankId == 0) {
        uint32_t block_iteration = 10;
        uint32_t base_block_size = 32 << 10;    // 32k
        uint32_t times = 10;
        uint32_t batch_size = 32;
        std::string dstSessionId = "127.0.0.1:10001";
        dst_dev_addr = gather_addr[1];
        CHECK_GOTO_ERR(!dst_dev_addr, "dev memory error", out);
        std::cout << "[" << rankId << "]" << " get dst dev addr " << dst_dev_addr << std::endl;

        /* init warmup data */
        warmup_data = (char *)malloc(GVA_SIZE * sizeof(char));
        CHECK_GOTO_ERR(!warmup_data, "warmup data malloc failed", out);

        std::cout << "Warmup Start" << std::endl;
        init_warmup_data(warmup_data, GVA_SIZE);
        aclrtMemcpy(dev_addr, GVA_SIZE, warmup_data, GVA_SIZE, ACL_MEMCPY_HOST_TO_DEVICE);
        ret = smem_trans_write(trans_handle, dev_addr, dstSessionId.c_str(), dst_dev_addr, base_block_size);
        CHECK_GOTO_ERR(ret, "trans copy failed, ret:" << ret << " rank:" << rankId, out);
        std::cout << "Warmup End" << std::endl;

        /* server dont need to register mem */

        ret = smem_shm_control_barrier(shm_handle);
        CHECK_GOTO_ERR(ret, "barrier failed, ret:" << ret << " rank:" << rankId, out);

        std::cout << "Test Start" << std::endl;
        for (uint32_t i = 0; i < block_iteration; i++) {
            uint32_t block_size = base_block_size * (1 << i);
            struct timeval start_tv;
            struct timeval stop_tv;
            gettimeofday(&start_tv, nullptr);
            /* latency test */
            for (uint32_t j = 0; j < times; j++) {
                ret = smem_trans_write(trans_handle, dev_addr, dstSessionId.c_str(), dst_dev_addr, block_size);
                CHECK_GOTO_ERR(ret, "trans copy failed, ret:" << ret << " rank:" << rankId, out);
            }

            gettimeofday(&stop_tv, nullptr);
            double duration1 = (stop_tv.tv_sec - start_tv.tv_sec) * 1000000.0 +
                                (stop_tv.tv_usec - start_tv.tv_usec);
            duration1 /= times;

            /* bw test */
            std::vector<void *> laddrv;
            std::vector<void *> raddrv;
            std::vector<uint64_t> lengthv;
            laddrv.reserve(batch_size);
            raddrv.reserve(batch_size);
            lengthv.reserve(batch_size);
            for (uint32_t j = 0; j < batch_size; j++) {
                void *laddr = (uint8_t *)dev_addr + j * block_size;
                void *raddr = (uint8_t *)dst_dev_addr + j * block_size;
                laddrv.push_back(laddr);
                raddrv.push_back(raddr);
                lengthv.push_back(block_size);
            }
            gettimeofday(&start_tv, nullptr);

            for (uint32_t j = 0; j < times; j++) {
                ret = smem_trans_batch_write(trans_handle, const_cast<const void**>(laddrv.data()),
                    dstSessionId.c_str(), raddrv.data(), (size_t *)lengthv.data(), lengthv.size());
                CHECK_GOTO_ERR(ret, "trans copy failed, ret:" << ret << " rank:" << rankId, out);
            }

            gettimeofday(&stop_tv, nullptr);
            double duration2 = (stop_tv.tv_sec - start_tv.tv_sec) * 1000000.0 +
                                (stop_tv.tv_usec - start_tv.tv_usec);
            duration2 /= times;

            std::cout << "Test completed: latency " << duration1
                    << "us, block size " << (block_size / KB_SIZE) << "KB, total size "
                    << batch_size * block_size / KB_SIZE << "KB , throughput "
                    << calculateRate(batch_size * block_size, duration2) << std::endl;
        }
        std::cout << "Test End" << std::endl;
    } else {
        ret = smem_trans_register_mem(trans_handle, dev_addr, GVA_SIZE, 0);
        CHECK_GOTO_ERR(ret, "failed to register device memory, ret:" << ret, out);

        ret = smem_shm_control_barrier(shm_handle);
        CHECK_GOTO_ERR(ret, "barrier failed, ret:" << ret << " rank:" << rankId, out);
    }

    ret = smem_shm_control_barrier(shm_handle);
    CHECK_GOTO_ERR(ret, "barrier failed, ret:" << ret << " rank:" << rankId, out);

out:
    if (warmup_data) {
        free(warmup_data);
    }
    if (dev_addr) {
        aclrtFree(dev_addr);
    }
    return 0;
}

int32_t bm_test(int rankId, int rankSize, int deviceId, int useSdma, std::string &ipPort)
{
    void *bm_gva = nullptr;
    void *shm_gva = nullptr;
    smem_bm_config_t config;
    smem_shm_config_t config2;
    smem_bm_t bm_handle;
    smem_shm_t shm_handle;

    (void)smem_bm_config_init(&config);
    config.rankId = rankId;
    if (rankId == 0) {
        std::string s1 = "tcp://192.168.0.1/16:12005";
        std::copy_n(s1.c_str(), s1.length(), config.hcomUrl);
    } else {
        std::string s1 = "tcp://192.168.0.1/16:12006";
        std::copy_n(s1.c_str(), s1.length(), config.hcomUrl);
    }
    auto ret = smem_bm_init(ipPort.c_str(), rankSize, deviceId, &config);
    CHECK_GOTO_ERR(ret, "smem bm init failed, ret:" << ret << " rank:" << rankId, err2);

    (void)smem_shm_config_init(&config2);
    config2.startConfigStore = false;

    ret = smem_shm_init(ipPort.c_str(), rankSize, rankId, deviceId, &config2);
    CHECK_GOTO_ERR(ret, "smem shmem init failed, ret:" << ret << " rank:" << rankId, err3);

    if (useSdma) {
        bm_handle = smem_bm_create(0, 0, SMEMB_DATA_OP_SDMA, 0, GVA_SIZE, 0);
    } else {
        bm_handle = smem_bm_create(0, 0, SMEMB_DATA_OP_DEVICE_RDMA, GVA_SIZE, 0, 0);
    }
    CHECK_GOTO_ERR((bm_handle == nullptr), "smem_bm_create failed, rank:" << rankId, err4);
    std::cout << "[" << rankId << "]" << " smem bm create done sdma_flag " << useSdma << std::endl;

    ret = smem_bm_join(bm_handle, 0, &bm_gva);
    CHECK_GOTO_ERR(ret, "smem_bm_join failed, ret:" << ret << " rank:" << rankId, err5);

    shm_handle = smem_shm_create(0, rankSize, rankId, GVA_SIZE, SMEMS_DATA_OP_MTE, 0, &shm_gva);
    CHECK_GOTO_ERR((shm_handle == nullptr), "smem_shm_create failed, rank:" << rankId, err5);
    std::cout << "[" << rankId << "]" << " smem shmem create done" << std::endl;

    ret = smem_shm_control_barrier(shm_handle);
    CHECK_GOTO_ERR(ret, "barrier failed, ret:" << ret << " rank:" << rankId, err6);

    /* Test */
    bm_perf_test(bm_handle, rankId);

    ret = smem_shm_control_barrier(shm_handle);
    CHECK_GOTO_ERR(ret, "barrier failed, ret:" << ret << " rank:" << rankId, err6);
err6:
    smem_shm_destroy(shm_handle, 0);
err5:
    smem_bm_destroy(bm_handle);
err4:
    smem_shm_uninit(0);
err3:
    smem_bm_uninit(0);
err2:
    return 0;
}

int32_t trans_test(int rankId, int rankSize, int deviceId, int useSdma, std::string &ipPort)
{
    void *shm_gva = nullptr;
    smem_trans_config_t config;
    smem_shm_config_t config2;
    smem_trans_t trans_handle;
    smem_shm_t shm_handle;
    std::string sessionId;
    int32_t ret;

    if (rankId == 0) {
        ret = smem_create_config_store(ipPort.c_str());
        CHECK_GOTO_ERR(ret, "smem create config store failed, ret:" << " rank:" << rankId, err1);
    }

    smem_trans_config_init(&config);
    if (rankId == 0) {
        /* Prefill */
        config.role = SMEM_TRANS_SENDER;
        sessionId = "127.0.0.1:10000";
    } else {
        /* Decode */
        config.role = SMEM_TRANS_RECEIVER;
        sessionId = "127.0.0.1:10001";
    }
    config.deviceId = deviceId;

    trans_handle = smem_trans_create(ipPort.c_str(), sessionId.c_str(), &config);
    CHECK_GOTO_ERR(!trans_handle, "smem trans create failed, ret:" << " rank:" << rankId, err1);
    std::cout << "[" << rankId << "]" << " smem trans create done" << std::endl;

    (void)smem_shm_config_init(&config2);
    config2.startConfigStore = false;

    ret = smem_shm_init(ipPort.c_str(), rankSize, rankId, deviceId, &config2);
    CHECK_GOTO_ERR(ret, "smem shmem init failed, ret:" << ret << " rank:" << rankId, err2);

    shm_handle = smem_shm_create(0, rankSize, rankId, GVA_SIZE, SMEMS_DATA_OP_MTE, 0, &shm_gva);
    CHECK_GOTO_ERR((shm_handle == nullptr), "smem_shm_create failed, rank:" << rankId, err3);
    std::cout << "[" << rankId << "]" << " smem shmem create done" << std::endl;

    ret = smem_shm_control_barrier(shm_handle);
    CHECK_GOTO_ERR(ret, "barrier failed, ret:" << ret << " rank:" << rankId, err4);

    trans_perf_test(trans_handle, shm_handle, rankId);

err4:
    smem_shm_destroy(shm_handle, 0);
err3:
    smem_shm_uninit(0);
err2:
    smem_trans_destroy(trans_handle, 0);
    smem_trans_uninit(0);
err1:
    return 0;
}

/*
 * smem_perf {rankSize} {rankId} {deviceID} {useSdma} {testBm} tcp://{Ip}:{port}
 * smem_perf 2 0 2 1 1 tcp://127.0.0.1:12050
 */
int32_t main(int32_t argc, char* argv[])
{
    int rankSize = atoi(argv[1]);
    int rankId = atoi(argv[2]);
    int deviceId = atoi(argv[3]);
    int useSdma = atoi(argv[4]);
    int testBm = atoi(argv[5]);
    std::string ipPort = argv[6];
    std::cout << "[TEST] input rank_size: " << rankSize << " rank_id:" << rankId
              << " device_id: " << deviceId << " use_sdma: " << useSdma << " test_bm: " << testBm
              << " store_ip: " << ipPort << std::endl;

    const size_t RANK_ID_SIZE = 2;
    if (rankSize != RANK_ID_SIZE) {
        std::cout << "[TEST] input rank_size: " << rankSize << " is not 2" << std::endl;
        return -1;
    }

    /* init aclrt */
    CHECK_ACL(aclInit(nullptr));
    CHECK_ACL(aclrtSetDevice(deviceId));

    const uint32_t LOG_LEVEL_WARNING = 2;
    smem_set_log_level(LOG_LEVEL_WARNING);
    smem_set_conf_store_tls(0, "", 0);
    auto ret = smem_init(0);
    CHECK_GOTO_ERR(ret, "smem init failed, ret:" << ret << " rank:" << rankId, err1);

    if (testBm) {
        (void)bm_test(rankId, rankSize, deviceId, useSdma, ipPort);
    } else {
        (void)trans_test(rankId, rankSize, deviceId, useSdma, ipPort);
    }

    smem_uninit();
err1:
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
    return 0;
}