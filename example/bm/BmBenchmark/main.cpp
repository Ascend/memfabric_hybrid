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
#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <regex>
#include "band_width_calculator.h"

static std::string ExecShellCmd(const char *cmd)
{
    std::string result;
    // 使用 popen 执行命令并以只读方式打开管道
    FILE *pipe = popen(cmd, "r");

    // 检查命令是否成功执行
    if (!pipe) {
        return "ERROR: popen failed.";
    }

    char buffer[1024];
    // 从管道中逐行读取命令输出
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer; // 将读取到的内容追加到结果字符串
    }

    // 关闭管道并获取命令的退出状态
    int status = pclose(pipe);
    if (status == -1) {
        result += "\nERROR: pclose failed.";
    }

    return result;
}

void PrintHelp()
{
    std::cout <<
        "usage: bm_perf_benchmark [-h | --help]\n"
        "               <command> [<option>] [<args>]\n\n"
        "-h, --help                  Displays help information.\n"
        "-bw, bandwidth              Performs a bandwidth test.\n"
        "    -t,  --type             Indicates the test type (h2d,d2h,h2rd,h2rh,d2rh,d2rd,rh2d,rh2h,rd2h,rd2d,all).\n"
        "    -s,  --size             Specifies block size (default 1024).\n"
        "    -et, --execute-times    Specifies execution count (default 1000).\n"
        "    -bs, --batch_size       Specifies block count (default 1).\n"
        "    -d,  --device           Specifies starting device ID (default 0).\n"
        "    -ws, --world-size       Specifies the total number of NPUs in the cluster.\n"
        "    -lrs, --local-rank-size Specifies the number of NPUs on the local node.\n"
        "    -rs, --rank-start       Specifies the starting rankId for the local node (default 0).\n"
        "    -ip, --ip               Specifies the IP address and port (e.g., tcp://<ip>:<port>).\n"
    << std::endl;
}

[[noreturn]] void ExitWithError(const std::string &msg)
{
    std::cerr << "Error: " << msg << std::endl;
    std::exit(1);
}

CopyType ParseType(const std::string &arg)
{
    if (arg == "h2d") {
        return CopyType::HOST_TO_DEVICE;
    }
    if (arg == "d2h") {
        return CopyType::DEVICE_TO_HOST;
    }
    if (arg == "h2rd") {
        return CopyType::HOST_TO_REMOTE_DEVICE;
    }
    if (arg == "h2rh") {
        return CopyType::HOST_TO_REMOTE_HOST;
    }
    if (arg == "d2rh") {
        return CopyType::DEVICE_TO_REMOTE_HOST;
    }
    if (arg == "d2rd") {
        return CopyType::DEVICE_TO_REMOTE_DEVICE;
    }
    if (arg == "rh2d") {
        return CopyType::REMOTE_HOST_TO_DEVICE;
    }
    if (arg == "rh2h") {
        return CopyType::REMOTE_HOST_TO_HOST;
    }
    if (arg == "rd2h") {
        return CopyType::REMOTE_DEVICE_TO_HOST;
    }
    if (arg == "rd2d") {
        return CopyType::REMOTE_DEVICE_TO_DEVICE;
    }
    if (arg == "all") {
        return CopyType::ALL_DIRECTION;
    }
    ExitWithError("invalid type: " + arg + " (must be one of h2d/d2h/h2rd/h2rh/d2rh/d2rd/rh2d/rh2h/rd2h/rd2d/all)");
}

uint64_t ParseUint64(const std::string &arg, const std::string &field)
{
    try {
        size_t pos = 0;
        uint64_t val = std::stoull(arg, &pos, 0);
        if (pos != arg.size()) {
            ExitWithError("invalid numeric value for " + field + ": " + arg);
        }
        return val;
    } catch (...) {
        ExitWithError("invalid numeric value for " + field + ": " + arg);
    }
}

uint32_t ParseUint32(const std::string &arg, const std::string &field)
{
    uint64_t val = ParseUint64(arg, field);
    if (val > UINT32_MAX) {
        ExitWithError("value too large for " + field + ": " + arg);
    }
    return static_cast<uint32_t>(val);
}

void ParseIp(const std::string &arg)
{
    std::regex ipPattern(R"(^tcp://([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+):([0-9]{1,5})$)");
    if (!std::regex_match(arg, ipPattern)) {
        ExitWithError("invalid IP address and port format: " + arg + " (must be tcp://<ip>:<port>)");
    }
}

void CheckCmdBw(BwTestParam &opts)
{
    if (opts.worldRankSize == 0 || opts.localRankSize == 0) {
        ExitWithError("world-size and local-rank-size must be greater than 0");
    }
    if (opts.worldRankSize < opts.localRankSize) {
        ExitWithError("world-size must be greater than or equal to local-rank-size");
    }
    if (opts.worldRankSize != (opts.worldRankSize & (~(opts.worldRankSize - 1)))) {
        ExitWithError("world-size must be the power of 2");
    }
    if (opts.worldRankSize > RANK_SIZE_MAX) {
        ExitWithError("world-size must be less than or equal to " + std::to_string(RANK_SIZE_MAX));
    }
    if (opts.executeTimes == 0) {
        ExitWithError("execute-times must be greater than 0");
    }
    if (opts.copySize == 0) {
        ExitWithError("size must be greater than 0");
    }
}

void ParseCmdBw(int argc, char *argv[], BwTestParam &opts)
{
    bool hasType = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-t" || arg == "--type") {
            if (i + 1 >= argc) {
                ExitWithError("missing value for " + arg);
            }
            opts.type = ParseType(argv[++i]);
            hasType = true;
        } else if (arg == "-s" || arg == "--size") {
            if (i + 1 >= argc) {
                ExitWithError("missing value for " + arg);
            }
            opts.copySize = ParseUint64(argv[++i], "size");
        } else if (arg == "-et" || arg == "--execute-times") {
            if (i + 1 >= argc) {
                ExitWithError("missing value for " + arg);
            }
            opts.executeTimes = ParseUint64(argv[++i], "execute-times");
        } else if (arg == "-bs" || arg == "--batch_size") {
            if (i + 1 >= argc) {
                ExitWithError("missing value for " + arg);
            }
            opts.batchSize = ParseUint64(argv[++i], "batch-size");
        } else if (arg == "-d" || arg == "--device") {
            if (i + 1 >= argc) {
                ExitWithError("missing value for " + arg);
            }
            opts.deviceId = ParseUint32(argv[++i], "device");
        } else if (arg == "-ws" || arg == "--world-size") {
            if (i + 1 >= argc) {
                ExitWithError("missing value for " + arg);
            }
            opts.worldRankSize = ParseUint32(argv[++i], "world-size");
        } else if (arg == "-lrs" || arg == "--local-rank-size") {
            if (i + 1 >= argc) {
                ExitWithError("missing value for " + arg);
            }
            opts.localRankSize = ParseUint32(argv[++i], "local-rank-size");
        } else if (arg == "-rs" || arg == "--rank-start") {
            if (i + 1 >= argc) {
                ExitWithError("missing value for " + arg);
            }
            opts.localRankStart = ParseUint32(argv[++i], "rank-start");
        } else if (arg == "-ip" || arg == "--ip") {
            if (i + 1 >= argc) {
                ExitWithError("missing value for " + arg);
            }
            opts.ipPort = argv[++i];
            ParseIp(opts.ipPort);
        } else {
            ExitWithError("unknown option: " + arg);
        }
    }

    if (!hasType) {
        ExitWithError("missing required option: -t / --type");
    }

    CheckCmdBw(opts);
}

void RunBwTest(BwTestParam &opts)
{
    BandWidthCalculator bwCalculator(opts);
    auto ret = bwCalculator.MultiProcessExecute();
    if (ret != 0) {
        LOG_ERROR("bandwidth test failed.");
    }
}

int main(int32_t argc, char* argv[])
{
    if (argc < 2) {
        PrintHelp();
        return 0;
    }

    // 检查硬件环境
    std::string productName = ExecShellCmd("dmidecode | grep 'Product Name'");
    smem_bm_data_op_type opType = SMEMB_DATA_OP_BUTT;
    if (productName.find("A3") != std::string::npos) {
        opType = SMEMB_DATA_OP_SDMA;
        LOG_INFO("current machine is A3.\n");
    } else if (productName.find("A2") != std::string::npos) {
        opType = SMEMB_DATA_OP_DEVICE_RDMA;
        LOG_INFO("current machine is A2.\n");
    } else {
        LOG_ERROR("unexcept machine, productName: " << productName);
        return -1;
    }
    std::string cmd = argv[1];
    if (cmd == "-h" || cmd == "--help") {
        PrintHelp();
        return 0;
    } else if (cmd == "-bw" || cmd == "--bandwidth") {
        BwTestParam opts;
        ParseCmdBw(argc, argv, opts);
        opts.opType = opType;
        RunBwTest(opts);
    } else {
        ExitWithError("unknown command: " + cmd);
    }

    LOG_INFO("main process exited.");
    return 0;
}