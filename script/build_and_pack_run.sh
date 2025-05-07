#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

set -e
readonly ROOT_PATH=$(dirname $(readlink -f "$0"))
CURRENT_DIR=$(pwd)

# check hybm lib
HYBM_LIB=${ROOT_PATH}/../3rdparty/hybmm/lib/libmf_hybm_core.so
if [ ! -f "${HYBM_LIB}" ]; then
    echo "hybmm lib is not exist! (${HYBM_LIB})"
    exit 1
fi

cd ${ROOT_PATH}
ACC_LINKS_SRC=${ROOT_PATH}/../3rdparty/net/acc_links/src
if [ ! -d "${ACC_LINKS_SRC}" ]; then
    git submodule init
    git submodule update
fi

bash build.sh RELEASE OFF OFF

bash run_pkg_maker/make_run.sh

cd ${CURRENT_DIR}