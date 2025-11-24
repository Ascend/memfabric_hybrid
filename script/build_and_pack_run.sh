#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# This file is a part of the CANN Open Software.
# Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.

set -e
readonly ROOT_PATH=$(dirname $(readlink -f "$0"))
CURRENT_DIR=$(pwd)

BUILD_MODE="RELEASE"
BUILD_PYTHON="ON"
USE_CANN="ON"
USE_VMM="OFF"

show_help() {
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "  --build_mode <mode>     Set build mode (RELEASE/DEBUG/ASAN), default: RELEASE"
    echo "  --build_python <ON/OFF> Enable/disable Python build, default: ON"
    echo "  --use_cann <ON/OFF>     Enable/disable CANN dependency, default: ON"
    echo "  --use_vmm <ON/OFF>      Enable/disable VMM api, default: OFF"
    echo "  --help                  Show this help message"
    echo ""
    echo "Example:"
    echo "  $0 --build_mode DEBUG --build_python ON"
    echo ""
}

while [[ "$#" -gt 0 ]]; do
    case "$1" in
        --build_mode)
            BUILD_MODE="$2"
            shift 2
            ;;
        --build_python)
            BUILD_PYTHON="$2"
            shift 2
            ;;
        --use_cann)
            USE_CANN="$2"
            shift 2
            ;;
        --use_vmm)
            USE_VMM="$2"
            shift 2
            ;;
        --help)
            show_help
            exit 0
            ;;
        *)
            echo "Unknown argument: $1"
            echo ""
            show_help
            exit 1
            ;;
    esac
done

echo "BUILD_MODE: $BUILD_MODE"
echo "BUILD_PYTHON: $BUILD_PYTHON"
echo "USE_CANN: $USE_CANN"
echo "USE_VMM: $USE_VMM"

cd ${ROOT_PATH}
SPDLOG_SRC=${ROOT_PATH}/../3rdparty/log/spdlog/src
if [ ! -d "${SPDLOG_SRC}" ]; then
    git submodule init
    git submodule update
fi

bash build.sh "${BUILD_MODE}" OFF OFF "${BUILD_PYTHON}" ON "${USE_VMM}" "${USE_CANN}"

bash run_pkg_maker/make_run.sh "${BUILD_PYTHON}" "${USE_CANN}"

cd ${CURRENT_DIR}