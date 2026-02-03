#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
# ******************************************************************************************
# Script for building NDR.
# Build options can be configured through environment variables.
# (1) NDR_BUILD_TYPE(optional, default is release) => set build type.(release/debug)
# (2) NDR_BUILD_EXAMPLE(optional, default is off) => build example and perf.(on/off)
# (3) ASCEND_CANN_DIR(required if build example) => set ascend cann dir.(no default)
# (4) ASCEND_DRIVER_DIR(required) => set ascend driver dir.(no default)
# version: 1.0.0
# ******************************************************************************************
set -e

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

# default build type is release
if [ "${NDR_BUILD_TYPE,,}" == "debug" ]; then
    NDR_BUILD_TYPE="Debug"
else
    NDR_BUILD_TYPE="Release"
fi
echo "${NDR_LOG_TAG} NDR build type: ${NDR_BUILD_TYPE}"

# default build example is off
if [ "${NDR_BUILD_EXAMPLE,,}" == "on" ]; then
    NDR_BUILD_EXAMPLE="ON"
else
    NDR_BUILD_EXAMPLE="OFF"
fi
echo "${NDR_LOG_TAG} NDR build example: ${NDR_BUILD_EXAMPLE}"

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
