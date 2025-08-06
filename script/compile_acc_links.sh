#!/bin/bash

BUILD_MODE=$1
BUILD_TESTS=$2
BUILD_OPEN_ABI=$3

if [ -z "$BUILD_MODE" ]; then
    BUILD_MODE="RELEASE"
fi

if [ -z "$BUILD_TESTS" ]; then
    BUILD_TESTS="OFF"
fi

if [ -z "$BUILD_OPEN_ABI" ]; then
    BUILD_OPEN_ABI="ON"
fi

set -e
readonly ROOT_PATH=$(dirname $(readlink -f "$0"))
MF_ROOT_PATH=$(cd "${ROOT_PATH}/.."; pwd)

echo "MF_ROOT_PATH is ${MF_ROOT_PATH}"
ACCLINK_SRC_PATH="${MF_ROOT_PATH}/src/net/acc_links"
ACCLINK_BUILD_PATH="${MF_ROOT_PATH}/build/acc_links"
ACCLINK_INSTALL_PATH="${MF_ROOT_PATH}/output/3rdparty/acc_links"

echo "ACCLINK_INSTALL_PATH is ${ACCLINK_INSTALL_PATH}"
[ -d "${ACCLINK_BUILD_PATH}" ] && rm -rf "${ACCLINK_BUILD_PATH}"
[ -d "${ACCLINK_INSTALL_PATH}" ] && rm -rf "${ACCLINK_INSTALL_PATH}"

mkdir -p "${ACCLINK_BUILD_PATH}"
mkdir -p "${ACCLINK_INSTALL_PATH}"
cmake -DCMAKE_BUILD_TYPE="${BUILD_MODE}" -DBUILD_TESTS="${BUILD_TESTS}" -DBUILD_OPEN_ABI="${BUILD_OPEN_ABI}" -DCMAKE_INSTALL_PREFIX="${ACCLINK_INSTALL_PATH}" -DBUILD_ACC_LINK=ON -S . -B ${ACCLINK_BUILD_PATH}
make clean -C "${ACCLINK_BUILD_PATH}"
make install -j8 -C "${ACCLINK_BUILD_PATH}"