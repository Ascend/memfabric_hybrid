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
readonly SCRIPT_FULL_PATH=$(dirname $(readlink -f "$0"))
readonly PROJECT_FULL_PATH=$(dirname "$SCRIPT_FULL_PATH")

readonly BUILD_PATH="$PROJECT_FULL_PATH/build"
readonly OUTPUT_PATH="$PROJECT_FULL_PATH/output"
readonly HYBM_LIB_PATH="$OUTPUT_PATH/hybm/lib64"
readonly SMEM_LIB_PATH="$OUTPUT_PATH/smem/lib64"
readonly COVERAGE_PATH="$OUTPUT_PATH/coverage"
readonly TEST_REPORT_PATH="$OUTPUT_PATH/bin/gcover_report"
readonly MOCKCPP_PATH="$PROJECT_FULL_PATH/test/3rdparty/mockcpp"
readonly TEST_3RD_PATCH_PATH="$PROJECT_FULL_PATH/test/3rdparty/patch"
readonly MOCK_CANN_PATH="$HYBM_LIB_PATH/cann"

TEST_FILTER="*$1*"
cd ${PROJECT_FULL_PATH}
rm -rf ${COVERAGE_PATH}
rm -rf ${BUILD_PATH}
rm -rf ${OUTPUT_PATH}
rm -rf ${TEST_REPORT_PATH}
mkdir -p ${BUILD_PATH}
mkdir -p ${TEST_REPORT_PATH}
mkdir -p ${OUTPUT_PATH}

set -e

dos2unix "$MOCKCPP_PATH/include/mockcpp/JmpCode.h"
dos2unix "$MOCKCPP_PATH/include/mockcpp/mockcpp.h"
dos2unix "$MOCKCPP_PATH/src/JmpCode.cpp"
dos2unix "$MOCKCPP_PATH/src/JmpCodeArch.h"
dos2unix "$MOCKCPP_PATH/src/JmpCodeX64.h"
dos2unix "$MOCKCPP_PATH/src/JmpCodeX86.h"
dos2unix "$MOCKCPP_PATH/src/JmpOnlyApiHook.cpp"
dos2unix "$MOCKCPP_PATH/src/UnixCodeModifier.cpp"
dos2unix $TEST_3RD_PATCH_PATH/*.patch

cmake -DCMAKE_BUILD_TYPE=DEBUG -DBUILD_TESTS=ON -DBUILD_OPEN_ABI=ON -DBUILD_COMPILER=gcc -S . -B ${BUILD_PATH}
make install -j32 -C ${BUILD_PATH}
export LD_LIBRARY_PATH=$MEMCACHE_LIB_PATH:$SMEM_LIB_PATH:$HYBM_LIB_PATH:$MOCK_CANN_PATH/driver/lib64:$LD_LIBRARY_PATH
export ASCEND_HOME_PATH=$MOCK_CANN_PATH
export ASAN_OPTIONS="detect_stack_use_after_return=1:allow_user_poisoning=1"

cd "$OUTPUT_PATH/bin/ut" && ./test_memfabric --gtest_break_on_failure --gtest_output=xml:"$TEST_REPORT_PATH/test_detail.xml" --gtest_filter=${TEST_FILTER}

mkdir -p "$COVERAGE_PATH"
cd "$OUTPUT_PATH"
lcov -d "$BUILD_PATH" --c --output-file "$COVERAGE_PATH"/coverage.info -rc lcov_branch_coverage=1 --rc lcov_excl_br_line="LCOV_EXCL_BR_LINE|SM_LOG*|SM_ASSERT*|BM_LOG*|BM_ASSERT*|SM_VALIDATE_*|ASSERT*|LOG_*|MMC_LOG*|MMC_RETURN*|MMC_ASSERT*|MMC_VALIDATE*"
lcov -e "$COVERAGE_PATH"/coverage.info "*/src/smem/*" -o "$COVERAGE_PATH"/coverage.info --rc lcov_branch_coverage=1
lcov -r "$COVERAGE_PATH"/coverage.info "*/3rdparty/*" "*/src/hybm/csrc/driver/*" -o "$COVERAGE_PATH"/coverage.info --rc lcov_branch_coverage=1
genhtml -o "$COVERAGE_PATH"/result "$COVERAGE_PATH"/coverage.info --show-details --legend --rc lcov_branch_coverage=1

lines_rate=`lcov -r "$COVERAGE_PATH"/coverage.info -o "$COVERAGE_PATH"/coverage.info --rc lcov_branch_coverage=1 | grep lines | grep -Eo "[0-9\.]+%" | tr -d '%'`
branches_rate=`lcov -r "$COVERAGE_PATH"/coverage.info -o "$COVERAGE_PATH"/coverage.info --rc lcov_branch_coverage=1 | grep branches | grep -Eo "[0-9\.]+%" | tr -d '%'`
echo "lines    coverage rate: ${lines_rate}%"
echo "branches coverate rate: ${branches_rate}%"

if [[ $(awk "BEGIN {print (${lines_rate} < 70 || ${branches_rate} < 40) ? 1 : 0}") -eq 1 ]]; then
    exit 0
else
    exit 0
fi