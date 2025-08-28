#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

set -e
readonly ROOT_PATH=$(dirname $(readlink -f "$0"))
CURRENT_DIR=$(pwd)

BUILD_MODE=$1
if [ -z "$BUILD_MODE" ]; then
    BUILD_MODE="RELEASE"
fi

cd ${ROOT_PATH}
ACC_LINKS_SRC=${ROOT_PATH}/../3rdparty/net/acc_links/src
if [ ! -d "${ACC_LINKS_SRC}" ]; then
    git submodule init
    git submodule update
fi

bash build.sh ${BUILD_MODE} OFF OFF

bash run_pkg_maker/make_run.sh

cd ${CURRENT_DIR}