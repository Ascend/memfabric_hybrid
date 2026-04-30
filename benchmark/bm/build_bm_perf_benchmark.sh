#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
BM_CPP_EXAMPLE_DIR=$(dirname $(readlink -f "$0"))
PROJ_DIR=${BM_CPP_EXAMPLE_DIR}/../../../

cd ${BM_CPP_EXAMPLE_DIR}
mkdir build
cmake . -B build
make -C build
mkdir ${PROJ_DIR}/output/bm_perf_benchmark -p
cp -rf ${BM_CPP_EXAMPLE_DIR}/bm_perf_benchmark ${PROJ_DIR}/output/bm_perf_benchmark/

rm -rf ${BM_CPP_EXAMPLE_DIR}/bm_perf_benchmark
rm -rf ${BM_CPP_EXAMPLE_DIR}/build