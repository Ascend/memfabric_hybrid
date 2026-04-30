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

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <sstream>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include "smem.h"
#include "smem_bm.h"

#define LOG_INFO(msg)  std::cout << __FILE__ << ":" << __LINE__ << " [INFO] " << msg << std::endl
#define LOG_WARN(msg)  std::cout << __FILE__ << ":" << __LINE__ << " [WARN] " << msg << std::endl
#define LOG_ERROR(msg) std::cout << __FILE__ << ":" << __LINE__ << " [ERROR] " << msg << std::endl

#define CHECK_RET(x, msg)   \
    do {                    \
        if ((x) != 0) {     \
            LOG_ERROR(msg); \
            return -1;      \
        }                   \
    } while (0)

constexpr uint64_t BYTES_PER_KB = 1024ULL;
constexpr uint64_t BYTES_PER_MB = BYTES_PER_KB * BYTES_PER_KB;
constexpr uint64_t BYTES_PER_GB = BYTES_PER_KB * BYTES_PER_MB;
constexpr uint64_t DEVICE_DRAM_SIZE = 2ULL * BYTES_PER_GB;
constexpr uint64_t DEVICE_HBM_SIZE = 2ULL * BYTES_PER_GB;
constexpr uint64_t HOST_TCP_DRAM_SIZE = 128ULL * BYTES_PER_MB;
constexpr uint64_t HOST_TCP_HBM_SIZE = 0ULL;
constexpr uint16_t HOST_TCP_DEVICE_ID = 4U;
constexpr uint32_t A3_DEVICE_CORE_SIZE = 2U;
constexpr size_t SHELL_OUTPUT_BUFFER_SIZE = 256U;
constexpr uint32_t WORLD_SIZE = 16U;
constexpr uint16_t DEFAULT_HCOM_PORT = 10003U;
constexpr uint16_t MIN_PORT = 1U;
constexpr uint16_t MAX_PORT = 65535U;
constexpr const char *LOCAL_HCOM_URL_PREFIX = "tcp://127.0.0.1/0:";
constexpr const char *OP_TYPE_SDMA_NAME = "SDMA";
constexpr const char *OP_TYPE_DEVICE_RDMA_NAME = "DEVICE_RDMA";
constexpr const char *OP_TYPE_HOST_TCP_NAME = "HOST_TCP";
constexpr int OP_TYPE_SDMA = 0;
constexpr int OP_TYPE_DEVICE_RDMA = 1;
constexpr int OP_TYPE_HOST_TCP = 2;
constexpr int ARG_IP_PORT = 1;
constexpr int ARG_OP_TYPE = 2;
constexpr int ARG_IS_A3 = 3;
constexpr int ARG_HCOM_PORT = 4;
constexpr int ARG_COUNT_MIN = ARG_HCOM_PORT + 1;

smem_bm_data_op_type g_dataOpType = SMEMB_DATA_OP_DEVICE_RDMA;

struct ExampleOptions {
    smem_bm_data_op_type dataOpType = SMEMB_DATA_OP_DEVICE_RDMA;
    const char *opTypeName = OP_TYPE_DEVICE_RDMA_NAME;
    bool requiresNpu = true;
    uint64_t localDramSize = DEVICE_DRAM_SIZE;
    uint64_t localHbmSize = DEVICE_HBM_SIZE;
};

static bool ParseExampleOptions(int opType, ExampleOptions &exampleOptions)
{
    switch (opType) {
        case OP_TYPE_SDMA:
            exampleOptions.dataOpType = SMEMB_DATA_OP_SDMA;
            exampleOptions.opTypeName = OP_TYPE_SDMA_NAME;
            exampleOptions.requiresNpu = true;
            exampleOptions.localDramSize = DEVICE_DRAM_SIZE;
            exampleOptions.localHbmSize = DEVICE_HBM_SIZE;
            return true;
        case OP_TYPE_DEVICE_RDMA:
            exampleOptions.dataOpType = SMEMB_DATA_OP_DEVICE_RDMA;
            exampleOptions.opTypeName = OP_TYPE_DEVICE_RDMA_NAME;
            exampleOptions.requiresNpu = true;
            exampleOptions.localDramSize = DEVICE_DRAM_SIZE;
            exampleOptions.localHbmSize = DEVICE_HBM_SIZE;
            return true;
        case OP_TYPE_HOST_TCP:
            exampleOptions.dataOpType = SMEMB_DATA_OP_HOST_TCP;
            exampleOptions.opTypeName = OP_TYPE_HOST_TCP_NAME;
            exampleOptions.requiresNpu = false;
            exampleOptions.localDramSize = HOST_TCP_DRAM_SIZE;
            exampleOptions.localHbmSize = HOST_TCP_HBM_SIZE;
            return true;
        default:
            return false;
    }
}

[[nodiscard]] static std::vector<std::string> ReadShellOutput(const std::string &cmd)
{
    if (cmd.empty()) {
        throw std::invalid_argument("Shell command must not be empty");
    }

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("Failed to open pipe for command: " + cmd);
    }

    std::vector<std::string> lines;
    char buf[SHELL_OUTPUT_BUFFER_SIZE];
    while (fgets(buf, sizeof(buf), pipe.get()) != nullptr) {
        buf[sizeof(buf) - 1] = '\0';
        std::string line(buf);
        if (!line.empty()) {
            lines.push_back(std::move(line));
        }
    }

    if (pclose(pipe.release()) != 0) {
        LOG_WARN("Command exited with non-zero status, output may be empty");
        return {"0"};
    }

    return lines;
}

static uint16_t DetectNpuDeviceId(bool isA3)
{
    auto lines = ReadShellOutput("npu-smi info | grep 'No running processes'");

    for (int i = static_cast<int>(lines.size()); i > 0; --i) {
        auto it = std::find_if(lines[i - 1].begin(), lines[i - 1].end(), [](char c) { return std::isdigit(c); });
        if (it == lines[i - 1].end()) {
            continue;
        }
        uint32_t deviceId = std::stoul(std::string(1, *it));
        if (deviceId == 0) {
            continue;
        }
        deviceId = isA3 ? deviceId * A3_DEVICE_CORE_SIZE : deviceId;
        LOG_INFO("Detected device id: " << deviceId << (isA3 ? " (A3, x2 applied)" : ""));
        return static_cast<uint16_t>(deviceId);
    }

    LOG_WARN("No idle device detected, falling back to device 0");
    return 0;
}

static uint16_t ResolveDeviceId(const ExampleOptions &exampleOptions, bool isA3)
{
    if (exampleOptions.requiresNpu) {
        return DetectNpuDeviceId(isA3);
    }

    LOG_INFO("HOST_TCP mode selected, use synthetic device id " << HOST_TCP_DEVICE_ID << ", ignore isA3=" << isA3);
    return HOST_TCP_DEVICE_ID;
}

static bool ParsePort(const char *arg, uint16_t &port)
{
    if (arg == nullptr || arg[0] == '\0') {
        return false;
    }

    char *end = nullptr;
    const unsigned long parsed = std::strtoul(arg, &end, 10);
    if (end == nullptr || *end != '\0' || parsed < MIN_PORT || parsed > MAX_PORT) {
        return false;
    }

    port = static_cast<uint16_t>(parsed);
    return true;
}

static std::string BuildLocalHcomUrl(uint16_t hcomPort)
{
    std::ostringstream stream;
    stream << LOCAL_HCOM_URL_PREFIX << hcomPort;
    return stream.str();
}

static int InitSmemBenchmark(uint32_t worldSize, uint16_t deviceId, const std::string &ipPort,
                             const std::string &hcomUrl)
{
    int ret = smem_init(0);
    CHECK_RET(ret, "smem_init failed, ret=" << ret);

    smem_set_log_level(0);

    smem_bm_config_t config;
    ret = smem_bm_config_init(&config);
    CHECK_RET(ret, "smem_bm_config_init failed, ret=" << ret);

    std::strncpy(config.hcomUrl, hcomUrl.c_str(), sizeof(config.hcomUrl) - 1);
    config.hcomUrl[sizeof(config.hcomUrl) - 1] = '\0';
    config.autoRanking = true;

    ret = smem_bm_init(ipPort.c_str(), worldSize, deviceId, &config);
    CHECK_RET(ret, "smem_bm_init failed, ret=" << ret << ", worldSize=" << worldSize << ", deviceId=" << deviceId);

    return 0;
}

static int RunBenchmark(uint32_t worldSize, const std::string &ipPort, bool isA3, uint16_t hcomPort,
                        const ExampleOptions &exampleOptions)
{
    const std::string hcomUrl = BuildLocalHcomUrl(hcomPort);
    const uint16_t deviceId = ResolveDeviceId(exampleOptions, isA3);
    if (InitSmemBenchmark(worldSize, deviceId, ipPort, hcomUrl) != 0) {
        return -1;
    }
    LOG_INFO("Benchmark initialized: deviceId=" << deviceId << ", worldSize=" << worldSize
                                                << ", opType=" << exampleOptions.opTypeName << ", hcomUrl=" << hcomUrl
                                                << ", localDramSize=" << exampleOptions.localDramSize
                                                << ", localHbmSize=" << exampleOptions.localHbmSize);
    smem_bm_t bmHandle =
        smem_bm_create(0, 0, g_dataOpType, exampleOptions.localDramSize, exampleOptions.localHbmSize, 0);
    if (bmHandle == nullptr) {
        LOG_ERROR("smem_bm_create failed");
        smem_bm_uninit(0);
        return -1;
    }

    int ret = smem_bm_join(bmHandle, 0);
    if (ret != 0) {
        LOG_ERROR("smem_bm_join failed, ret=" << ret);
        smem_bm_destroy(bmHandle);
        smem_bm_uninit(0);
        return -1;
    }

    LOG_INFO("All ranks joined, ready (type 'exit'/'e'/'q' to stop)");

    std::string userInput;
    while (std::cin >> userInput) {
        if (userInput == "exit" || userInput == "e" || userInput == "q") {
            break;
        }
    }

    smem_bm_destroy(bmHandle);
    smem_bm_uninit(0);
    LOG_INFO("Benchmark stopped, resources released");
    return 0;
}

int main(int32_t argc, char *argv[])
{
    if (argc < ARG_COUNT_MIN) {
        LOG_ERROR("Usage: " << argv[0] << " <ipPort> <opType(0=SDMA,1=DEVICE_RDMA,2=HOST_TCP)> <isA3(0|1)> <hcomPort>");
        return -1;
    }

    const uint32_t worldSize = WORLD_SIZE;
    const std::string ipPort = argv[ARG_IP_PORT];
    const int opType = std::atoi(argv[ARG_OP_TYPE]);
    const bool isA3 = std::atoi(argv[ARG_IS_A3]) != 0;
    uint16_t hcomPort = DEFAULT_HCOM_PORT;
    ExampleOptions exampleOptions;

    if (!ParsePort(argv[ARG_HCOM_PORT], hcomPort)) {
        LOG_ERROR("Invalid hcomPort " << argv[ARG_HCOM_PORT] << ", valid range is " << MIN_PORT << "-" << MAX_PORT);
        return -1;
    }

    if (!ParseExampleOptions(opType, exampleOptions)) {
        LOG_ERROR("Invalid opType " << opType << ", valid values are 0(SDMA), 1(DEVICE_RDMA), 2(HOST_TCP)");
        return -1;
    }

    g_dataOpType = exampleOptions.dataOpType;

    LOG_INFO("Starting: worldSize=" << worldSize << ", ipPort=" << ipPort << ", hcomPort=" << hcomPort
                                    << ", opType=" << exampleOptions.opTypeName << ", isA3=" << isA3);

    CHECK_RET(RunBenchmark(worldSize, ipPort, isA3, hcomPort, exampleOptions), "RunBenchmark failed");

    LOG_INFO("Process exited normally");
    return 0;
}
