#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# This file is a part of the CANN Open Software.
# Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
readonly SCRIPT_FULL_PATH=$(dirname $(readlink -f "$0"))
readonly PROJECT_FULL_PATH=$(dirname "$SCRIPT_FULL_PATH")

readonly BUILD_PATH="$PROJECT_FULL_PATH/build"
readonly OUTPUT_PATH="$PROJECT_FULL_PATH/output"
readonly HYBM_LIB_PATH="$OUTPUT_PATH/hybm/lib64"
readonly COVERAGE_PATH="$OUTPUT_PATH/coverage"
readonly TEST_3RD_PATH="$PROJECT_FULL_PATH/test/3rdparty"
readonly MOCKCPP_PATH="$PROJECT_FULL_PATH/test/3rdparty/mockcpp"
readonly TEST_3RD_PATCH_PATH="$PROJECT_FULL_PATH/test/3rdparty/patch"
readonly MOCK_CANN_PATH="$HYBM_LIB_PATH/cann"
readonly SECODE_FUZZZ_PATH="$PROJECT_FULL_PATH/test/3rdparty/secodefuzz"

export ENABLE_FUZZ="ON"

# 关键文件列表
REQUIRED_FILES=(
    "${SECODE_FUZZZ_PATH}/lib/libSecodefuzz.a"
    "${SECODE_FUZZZ_PATH}/include/secodefuzz/secodeFuzz.h"
)

# 检查文件是否完整
check_files_exist() {
    for file in "${REQUIRED_FILES[@]}"; do
        if [ ! -f "$file" ]; then
            echo "Required file missing: $file"
            return 1
        fi
    done
    return 0
}

# 如果 secodefuzz 目录不存在，或者文件不完整，则拉取和编译
if [ ! -d "${SECODE_FUZZZ_PATH}" ] || ! check_files_exist; then
    echo "secodefuzz directory or required files missing. Cloning and building secodefuzz..."

    # 如果目录存在但文件不完整，删除目录以重新拉取和编译
    if [ -d "${SECODE_FUZZZ_PATH}" ]; then
        echo "Removing incomplete secodefuzz directory..."
        rm -rf "${SECODE_FUZZZ_PATH}"
    fi

    # 拉取 secodefuzz 代码
    cd "${TEST_3RD_PATH}"
    git clone secodefuzz.git # use fuzz git repo as neccessary
    if [ $? -ne 0 ]; then
        echo "Failed to clone secodefuzz repository."
        exit 1
    fi

    # 切换到指定版本
    cd secodefuzz
    git checkout -b v2.4.8 v2.4.8
    if [ $? -ne 0 ]; then
        echo "Failed to checkout secodefuzz version v2.4.8."
        exit 1
    fi

    # 编译 secodefuzz
    bash build.sh
    if [ $? -ne 0 ]; then
        echo "Failed to build secodefuzz."
        exit 1
    fi

    # 创建安装目录并复制文件
    mkdir -p "${SECODE_FUZZZ_PATH}/lib"
    cp ./examples/out-bin-x64/out/* "${SECODE_FUZZZ_PATH}/lib"
    cp ./examples/out-bin-x64/libSecodefuzz.a "${SECODE_FUZZZ_PATH}/lib"
    mkdir -p "${SECODE_FUZZZ_PATH}/include/secodefuzz"
    cp ./Secodefuzz/secodeFuzz.h "${SECODE_FUZZZ_PATH}/include/secodefuzz"

    echo "secodefuzz installed to ${SECODE_FUZZZ_PATH} successfully."
else
    echo "secodefuzz directory and required files already exist at ${SECODE_FUZZZ_PATH}. Skipping clone and build."
fi


set -e
cd ${PROJECT_FULL_PATH}
mkdir -p ${BUILD_PATH}
cmake -DCMAKE_BUILD_TYPE=ASAN -DBUILD_TESTS=ON -DBUILD_OPEN_ABI=ON -DBUILD_COMPILER=gcc -S . -B ${BUILD_PATH}
make install -j5 -C ${BUILD_PATH}
export LD_LIBRARY_PATH=$HYBM_LIB_PATH:$MOCK_CANN_PATH/driver/lib64:$LD_LIBRARY_PATH
export ASCEND_HOME_PATH=$MOCK_CANN_PATH
export ASAN_OPTIONS="detect_stack_use_after_return=1:allow_user_poisoning=1"

cd "$OUTPUT_PATH/bin" && ./test_mf_fuzz --gtest_output=xml:gcover_report/test_detail.xml

mkdir -p "$COVERAGE_PATH"
cd "$OUTPUT_PATH"
lcov --d "$BUILD_PATH" --c --output-file "$COVERAGE_PATH"/coverage.info -rc lcov_branch_coverage=1
lcov -e "$COVERAGE_PATH"/coverage.info "*/src/*" -o "$COVERAGE_PATH"/coverage.info --rc lcov_branch_coverage=1
lcov -r "$COVERAGE_PATH"/coverage.info "*/3rdparty/*" "*/src/hybm/driver/*" -o "$COVERAGE_PATH"/coverage.info --rc lcov_branch_coverage=1

genhtml -o "$COVERAGE_PATH"/result "$COVERAGE_PATH"/coverage.info --show-details --legend --rc lcov_branch_coverage=1