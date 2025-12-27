#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

BUILD_TEST=${1:-BUILD_TEST}
XPU_TYPE=${2:-NPU}
echo "BUILD_TEST is ${BUILD_TEST}"
echo "XPU_TYPE is ${XPU_TYPE}"
set -e
readonly BASH_PATH=$(dirname $(readlink -f "$0"))
CURRENT_DIR=$(pwd)
PROJECT_DIR=${BASH_PATH}/../..

cd ${BASH_PATH}

# get commit id
GIT_COMMIT=`git rev-parse HEAD` || true

# get arch
if [ $( uname -m | grep -c -i "x86_64" ) -ne 0 ]; then
    echo "it is system of x86_64"
    ARCH="x86_64"
elif [ $( uname -m | grep -c -i "aarch64" ) -ne 0 ]; then
    echo "it is system of aarch64"
    ARCH="aarch64"
else
    echo "it is not system of x86_64 or aarch64"
    exit 1
fi

#get os
OS_NAME=$(uname -s | awk '{print tolower($0)}')
ARCH_OS=${ARCH}-${OS_NAME}

PKG_DIR="memfabric_hybrid"
VERSION="${VERSION:-1.0.1}"
OUTPUT_DIR=${BASH_PATH}/../../output

rm -rf ${PKG_DIR}
mkdir -p ${PKG_DIR}/"${ARCH_OS}"
mkdir ${PKG_DIR}/"${ARCH_OS}"/include
mkdir ${PKG_DIR}/"${ARCH_OS}"/lib64
mkdir ${PKG_DIR}/"${ARCH_OS}"/wheel
mkdir -p ${PKG_DIR}/"${ARCH_OS}"/include/smem
mkdir -p ${PKG_DIR}/"${ARCH_OS}"/include/hybm

# smem
cp -r "${OUTPUT_DIR}"/smem/include/ ${PKG_DIR}/"${ARCH_OS}"/include/smem/
cp "${OUTPUT_DIR}"/smem/lib64/* ${PKG_DIR}/"${ARCH_OS}"/lib64
# hybm
cp -r "${OUTPUT_DIR}"/hybm/include/* ${PKG_DIR}/"${ARCH_OS}"/include/hybm
cp "${OUTPUT_DIR}"/hybm/lib64/libmf_hybm_core.so ${PKG_DIR}/"${ARCH_OS}"/lib64/
# memfabric_hybrid
cp -r "${OUTPUT_DIR}"/memfabric_hybrid/wheel/*.whl ${PKG_DIR}/"${ARCH_OS}"/wheel/

if [ "$BUILD_TEST" = "ON" ]; then
    mkdir -p ${PKG_DIR}/"${ARCH_OS}"/script/mock_server
    cp "${PROJECT_DIR}"/test/python/mock_server/server.py ${PKG_DIR}/"${ARCH_OS}"/script/mock_server
fi

mkdir -p ${PKG_DIR}/script
if [[ "$XPU_TYPE" == "NPU" ]]; then
   echo "in make_run.sh, XPU_TYPE is NPU"
   cp "${BASH_PATH}"/install.sh ${PKG_DIR}/script/
elif [[ "$XPU_TYPE" == "GPU" ]]; then
   echo "in make_run.sh, XPU_TYPE is GPU"
   cp "${BASH_PATH}"/no_cann/install.sh ${PKG_DIR}/script/
else
   echo "in make_run.sh, XPU_TYPE is NONE"
   cp "${BASH_PATH}"/no_cann/install.sh ${PKG_DIR}/script/
fi

cp "${BASH_PATH}"/uninstall.sh ${PKG_DIR}/script/

# generate version.info
touch ${PKG_DIR}/version.info
cat>>${PKG_DIR}/version.info<<EOF
Version:${VERSION}
Platform:${ARCH}
Kernel:${OS_NAME}
CommitId:${GIT_COMMIT}
EOF

# make run
case "$XPU_TYPE" in
    NPU)
        XPU_SUFFIX=""
        ;;
    NONE)
        XPU_SUFFIX="_cpu"
        ;;
    GPU)
        XPU_SUFFIX="_gpu"
        ;;
    "")
        echo "Error: Environment variable XPU_TYPE is not set. Must be exactly one of: NPU, NONE, GPU." >&2
        exit 1
        ;;
    *)
        echo "Error: Invalid value for XPU_TYPE: '$XPU_TYPE'. Must be exactly one of: NPU, NONE, GPU." >&2
        exit 1
        ;;
esac

FILE_NAME=${PKG_DIR}-${VERSION}_${OS_NAME}_${ARCH}${XPU_SUFFIX}
tar -cvf "${FILE_NAME}".tar.gz ${PKG_DIR}/
cat run_header.sh "${FILE_NAME}".tar.gz > "${FILE_NAME}".run
mv "${FILE_NAME}".run "${OUTPUT_DIR}"
rm -rf ${PKG_DIR}
rm -rf "${FILE_NAME}".tar.gz

cd "${CURRENT_DIR}"