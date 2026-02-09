#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
# ******************************************************************************************
# Script for building NDR.
# Build options can be configured through command line arguments.
# Usage: bash scripts/build.sh [options]
# Options:
#   --build_test ON/OFF    Enable/disable unit tests compilation (default: OFF)
#   --help                 Show this help message
#
# Environment variables required:
# (1) ASCEND_CANN_DIR(required if build example) => set ascend cann dir.(no default)
# (2) ASCEND_DRIVER_DIR(required) => set ascend driver dir.(no default)
# version: 1.0.0
# ******************************************************************************************
set -e

# Function to show help
show_help() {
    echo "Usage: bash $0 [options]"
    echo "Options:"
    echo "  --build_test ON/OFF    Enable/disable unit tests compilation (default: OFF)"
    echo "  --help                 Show this help message"
    echo ""
    echo "Examples:"
    echo "  bash $0                           # Build without tests"
    echo "  bash $0 --build_test ON          # Build with tests"
    echo "  bash $0 --build_test OFF         # Build without tests"
    echo ""
    echo "Environment variables required:"
    echo "  ASCEND_CANN_DIR - set ascend cann dir (required if build example)"
    echo "  ASCEND_DRIVER_DIR - set ascend driver dir (required)"
}

# Initialize variables
NDR_BUILD_UT="OFF"
NDR_BUILD_EXAMPLE="OFF"
NDR_BUILD_TYPE="Release"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --build_test)
            if [[ $2 == "ON" || $2 == "OFF" ]]; then
                NDR_BUILD_UT="$2"
                shift 2
            else
                echo "Error: --build_test must be followed by ON or OFF"
                show_help
                exit 1
            fi
            ;;
        --help)
            show_help
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

export C_INCLUDE_PATH=$ASCEND_CANN_DIR/include:$ASCEND_DRIVER_DIR/include:$C_INCLUDE_PATH
export LD_LIBRARY_PATH=$ASCEND_CANN_DIR/runtime/lib64:$ASCEND_DRIVER_DIR/lib64/driver:$LD_LIBRARY_PATH
export LIBRARY_PATH=$ASCEND_CANN_DIR/runtime/lib64:$ASCEND_DRIVER_DIR/lib64/driver:$LIBRARY_PATH

readonly NDR_ROOT_DIR=$(cd $(dirname ${0}) && pwd)/../
readonly NDR_BUILD_DIR="${NDR_ROOT_DIR}/build"
readonly NDR_INSTALL_DIR="${NDR_ROOT_DIR}/output"
readonly NDR_LOG_TAG="[$(basename ${0})]"

echo "${NDR_LOG_TAG} NDR_ROOT_DIR: ${NDR_ROOT_DIR}"
echo "${NDR_LOG_TAG} NDR_BUILD_DIR: ${NDR_BUILD_DIR}"
echo "${NDR_LOG_TAG} NDR_INSTALL_DIR: ${NDR_INSTALL_DIR}"

echo "${NDR_LOG_TAG} NDR build UT: ${NDR_BUILD_UT}"

# prepare build directory
if [ -d "${NDR_BUILD_DIR}" ]; then
    rm -rf "${NDR_BUILD_DIR}"/*
else
    mkdir -p ${NDR_BUILD_DIR}
fi
echo "${NDR_LOG_TAG} NDR build dir: ${NDR_BUILD_DIR}"

# print NDR install directory
echo "${NDR_LOG_TAG} NDR install dir: ${NDR_INSTALL_DIR}"

# start build
cd ${NDR_BUILD_DIR}

cmake -DCMAKE_INSTALL_PREFIX=${NDR_INSTALL_DIR} \
      -DCMAKE_BUILD_TYPE=${NDR_BUILD_TYPE} \
      -DNDR_BUILD_EXAMPLE=${NDR_BUILD_EXAMPLE} \
      -DBUILD_TESTS=${NDR_BUILD_UT} \
      -DASCEND_CANN_DIR=${ASCEND_CANN_DIR} \
      -DASCEND_DRIVER_DIR=${ASCEND_DRIVER_DIR} ..

if [ "$?" != 0 ]; then
    echo "${NDR_LOG_TAG} NDR cmake failed"
    exit 1
fi

CPU_PROCESSOR_NUM=$(grep processor /proc/cpuinfo | wc -l)
echo "${NDR_LOG_TAG} parallel build job num is ${CPU_PROCESSOR_NUM}"

make clean
if [ "$?" != 0 ]; then
    echo "${NDR_LOG_TAG} make clean failed"
	exit 1
fi

make -j"${CPU_PROCESSOR_NUM}"
if [ "$?" != 0 ]; then
    echo "${NDR_LOG_TAG} make NDR failed"
  	exit 1
fi