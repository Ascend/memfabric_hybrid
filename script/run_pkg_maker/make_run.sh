#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
set -e
readonly BASH_PATH=$(dirname $(readlink -f "$0"))
CURRENT_DIR=$(pwd)

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

PKG_DIR="mxc-memfabric_hybrid"
VERSION="1.0.0"
OUTPUT_DIR=${BASH_PATH}/../../output

rm -rf ${PKG_DIR}
mkdir -p ${PKG_DIR}/${ARCH}_${OS_NAME}

cp -r ${OUTPUT_DIR}/smem ${PKG_DIR}/${ARCH}-${OS_NAME}
cp ${BASH_PATH}/../../3rdparty/hybmm/lib/libmf_hybm_core.so ${PKG_DIR}/${ARCH}-${OS_NAME}/lib64/

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