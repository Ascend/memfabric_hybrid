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
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include "acl/acl.h"
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

constexpr uint64_t GVA_SIZE = 2ULL * 1024 * 1024 * 1024;
constexpr const char *HCOM_URL = "tcp://0.0.0.0/0:10005";
constexpr int ARG_RANK_SIZE = 1;
constexpr int ARG_IP_PORT = 2;
constexpr int ARG_OP_TYPE = 3;
constexpr int ARG_IS_A3 = 4;
constexpr int ARG_COUNT_MIN = ARG_IS_A3 + 1;

smem_bm_data_op_type g_dataOpType = SMEMB_DATA_OP_DEVICE_RDMA;

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
    char buf[256];
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

static uint32_t DetectDeviceId(bool isA3)
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
        uint32_t coreSize = 2u;
        deviceId = isA3 ? deviceId * coreSize : deviceId;
        LOG_INFO("Detected device id: " << deviceId << (isA3 ? " (A3, x2 applied)" : ""));
        return deviceId;
    }

    LOG_WARN("No idle device detected, falling back to device 0");
    return 0;
}

static int InitSmemBenchmark(uint32_t rankSize, uint32_t deviceId, const std::string &ipPort)
{
    int ret = smem_init(0);
    CHECK_RET(ret, "smem_init failed, ret=" << ret);

    smem_set_log_level(0);

    smem_bm_config_t config;
    ret = smem_bm_config_init(&config);
    CHECK_RET(ret, "smem_bm_config_init failed, ret=" << ret);

    std::strncpy(config.hcomUrl, HCOM_URL, sizeof(config.hcomUrl) - 1);
    config.hcomUrl[sizeof(config.hcomUrl) - 1] = '\0';
    config.autoRanking = true;

    ret = smem_bm_init(ipPort.c_str(), rankSize, deviceId, &config);
    CHECK_RET(ret, "smem_bm_init failed, ret=" << ret << ", rankSize=" << rankSize << ", deviceId=" << deviceId);

    return 0;
}

static int RunBenchmark(uint32_t rankSize, const std::string &ipPort, bool isA3)
{
    const uint32_t deviceId = DetectDeviceId(isA3);
    if (InitSmemBenchmark(rankSize, deviceId, ipPort) != 0) {
        return -1;
    }
    LOG_INFO("Benchmark initialized: deviceId=" << deviceId << ", rankSize=" << rankSize);
    smem_bm_t bmHandle = smem_bm_create(0, 0, g_dataOpType, GVA_SIZE, GVA_SIZE, 0);
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
        LOG_ERROR("Usage: " << argv[0] << " <rankSize> <ipPort> <opType(0=SDMA,1=RDMA)> <isA3(0|1)>");
        return -1;
    }

    const uint32_t rankSize = static_cast<uint32_t>(std::atoi(argv[ARG_RANK_SIZE]));
    const std::string ipPort = argv[ARG_IP_PORT];
    const int opType = std::atoi(argv[ARG_OP_TYPE]);
    const bool isA3 = std::atoi(argv[ARG_IS_A3]) != 0;

    g_dataOpType = (opType == 0) ? SMEMB_DATA_OP_SDMA : SMEMB_DATA_OP_DEVICE_RDMA;

    LOG_INFO("Starting: rankSize=" << rankSize << ", ipPort=" << ipPort
                                   << ", opType=" << (opType == 0 ? "SDMA" : "DEVICE_RDMA") << ", isA3=" << isA3);

    CHECK_RET(RunBenchmark(rankSize, ipPort, isA3), "RunBenchmark failed");

    LOG_INFO("Process exited normally");
    return 0;
}