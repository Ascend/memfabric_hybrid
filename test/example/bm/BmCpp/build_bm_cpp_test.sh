#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
BM_CPP_EXAMPLE_DIR=$(dirname $(readlink -f "$0"))
PROJ_DIR=$(BM_CPP_EXAMPLE_DIR)../../../../
bash ${PROJ_DIR}/script/build.sh DEBUG
mkdir ${PROJ_DIR}/output/tmp_mf/aarch64-linux/include/util -p
mkdir ${PROJ_DIR}/output/tmp_mf/aarch64-linux/include/smem -p
mkdir ${PROJ_DIR}/output/tmp_mf/aarch64-linux/include/hybm -p
mkdir ${PROJ_DIR}/output/tmp_mf/aarch64-linux/lib64 -p
cp -rf ${PROJ_DIR}/output/smem/include/* ${PROJ_DIR}/output/tmp_mf/aarch64-linux/include/smem
cp -rf ${PROJ_DIR}/output/hybm/include/* ${PROJ_DIR}/output/tmp_mf/aarch64-linux/include/hybm
cp -rf ${PROJ_DIR}/output/smem/lib64/* ${PROJ_DIR}/output/tmp_mf/aarch64-linux/lib64
cp -rf ${PROJ_DIR}/output/hybm/lib64/* ${PROJ_DIR}/output/tmp_mf/aarch64-linux/lib64
cp -rf ${PROJ_DIR}/output/driver/lib64/* ${PROJ_DIR}/output/tmp_mf/aarch64-linux/lib64

export MEMFABRIC_HYBRID_HOME_PATH=${PROJ_DIR}/output/tmp_mf

cd ${BM_CPP_EXAMPLE_DIR}
mkdir build
cmake . -B build
make -C build
mkdir ${PROJ_DIR}/output/bm_example -p
cp -rf ${PROJ_DIR}/output/tmp_mf/aarch64-linux/lib64/* ${PROJ_DIR}/output/bm_example
cp -rf ${BM_CPP_EXAMPLE_DIR}/bm_example ${PROJ_DIR}/output/bm_example

rm -rf ${BM_CPP_EXAMPLE_DIR}/bm_example
rm -rf ${BM_CPP_EXAMPLE_DIR}/build
rm -rf ${PROJ_DIR}/output/tmp_mf/