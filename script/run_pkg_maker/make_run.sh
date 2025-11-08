#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
set -e
readonly BASH_PATH=$(dirname $(readlink -f "$0"))
CURRENT_DIR=$(pwd)
PROJECT_DIR=${BASH_PATH}/../..

cd ${BASH_PATH}

# get commit id
rm -rf ../git_last_commit.txt
bash ../gen_last_git_commit.sh
GIT_COMMIT=$(cat ../git_last_commit.txt)
rm -rf ../git_last_commit.txt

# get arch
if [ $( uname -i | grep -c -i "x86_64" ) -ne 0 ]; then
    echo "it is system of x86_64"
    ARCH="x86_64"
elif [ $( uname -i | grep -c -i "aarch64" ) -ne 0 ]; then
    echo "it is system of aarch64"
    ARCH="aarch64"
else
    echo "it is not system of x86_64 or aarch64"
    exit 1
fi

#get os
OS_NAME=$(uname -s | awk '{print tolower($0)}')
ARCH_OS=${ARCH}-${OS_NAME}

PKG_DIR="mxc-memfabric_hybrid"
VERSION="1.0.0"
OUTPUT_DIR=${BASH_PATH}/../../output

rm -rf ${PKG_DIR}
mkdir -p ${PKG_DIR}/${ARCH_OS}
mkdir ${PKG_DIR}/${ARCH_OS}/bin
mkdir ${PKG_DIR}/${ARCH_OS}/include
mkdir ${PKG_DIR}/${ARCH_OS}/lib64
mkdir ${PKG_DIR}/${ARCH_OS}/wheel
mkdir ${PKG_DIR}/${ARCH_OS}/script
mkdir ${PKG_DIR}/config
mkdir -p ${PKG_DIR}/${ARCH_OS}/include/smem
mkdir -p ${PKG_DIR}/${ARCH_OS}/include/hybm
mkdir -p ${PKG_DIR}/${ARCH_OS}/include/driver
mkdir -p ${PKG_DIR}/${ARCH_OS}/include/memcache
mkdir -p ${PKG_DIR}/${ARCH_OS}/script/mock_server

# smem
cp -r ${OUTPUT_DIR}/smem/include/* ${PKG_DIR}/${ARCH_OS}/include/smem
cp ${OUTPUT_DIR}/smem/lib64/* ${PKG_DIR}/${ARCH_OS}/lib64
cp ${OUTPUT_DIR}/smem/wheel/* ${PKG_DIR}/${ARCH_OS}/wheel
# hybm
cp -r ${OUTPUT_DIR}/hybm/include/* ${PKG_DIR}/${ARCH_OS}/include/hybm
cp ${OUTPUT_DIR}/hybm/lib64/libmf_hybm_core.so ${PKG_DIR}/${ARCH_OS}/lib64/
# driver
cp -r ${OUTPUT_DIR}/driver/include/* ${PKG_DIR}/${ARCH_OS}/include/driver
cp ${OUTPUT_DIR}/driver/lib64/* ${PKG_DIR}/${ARCH_OS}/lib64/
# memcache
cp -r ${OUTPUT_DIR}/memcache/include/* ${PKG_DIR}/${ARCH_OS}/include/memcache
cp ${OUTPUT_DIR}/memcache/lib64/* ${PKG_DIR}/${ARCH_OS}/lib64/
cp ${OUTPUT_DIR}/memcache/bin/* ${PKG_DIR}/${ARCH_OS}/bin/
cp ${OUTPUT_DIR}/memcache/wheel/*.whl ${PKG_DIR}/${ARCH_OS}/wheel/
# mooncake_adapter
cp -r ${OUTPUT_DIR}/mooncake_adapter/wheel/*.whl ${PKG_DIR}/${ARCH_OS}/wheel/

cp ${PROJECT_DIR}/config/* ${PKG_DIR}/config
cp -r ${PROJECT_DIR}/test/certs ${PKG_DIR}/${ARCH_OS}/script
cp -r ${PROJECT_DIR}/test/k8s_deploy ${PKG_DIR}/${ARCH_OS}/script
cp ${PROJECT_DIR}/test/python/memcache/mock_server/server.py ${PKG_DIR}/${ARCH_OS}/script/mock_server
cp ${PROJECT_DIR}/test/python/memcache/mock_server/smem_bm/smem_bm_server.py ${PKG_DIR}/${ARCH_OS}/script/mock_server
cp -r ${PROJECT_DIR}/test/python/memcache/ha ${PKG_DIR}/${ARCH_OS}/script

mkdir -p ${PKG_DIR}/script
cp ${BASH_PATH}/install.sh ${PKG_DIR}/script/
cp ${BASH_PATH}/uninstall.sh ${PKG_DIR}/script/

# generate version.info
touch ${PKG_DIR}/version.info
cat>>${PKG_DIR}/version.info<<EOF
Version:${VERSION}
Platform:${ARCH}
Kernel:${OS_NAME}
CommitId:${GIT_COMMIT}
EOF

# make run
FILE_NAME=${PKG_DIR}-${VERSION}_${OS_NAME}_${ARCH}
tar -cvf ${FILE_NAME}.tar.gz ${PKG_DIR}/
cat run_header.sh ${FILE_NAME}.tar.gz > ${FILE_NAME}.run
mv ${FILE_NAME}.run ${OUTPUT_DIR}

rm -rf ${PKG_DIR}
rm -rf ${FILE_NAME}.tar.gz

cd ${CURRENT_DIR}
