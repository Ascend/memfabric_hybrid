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
readonly FINGERPRINT_FILE="$BUILD_PATH/.build_fingerprint"
readonly BUILD_FINGERPRINT="ASAN-UT-OPEN_ABI"

FAST_MODE=false
if [[ "$1" == "--fast" ]]; then
    FAST_MODE=true
    shift
fi
TEST_FILTER="*$1*"
cd ${PROJECT_FULL_PATH}
if $FAST_MODE; then
    NEED_FULL_BUILD=false
    if [ ! -d "${BUILD_PATH}" ] || [ ! -f "${FINGERPRINT_FILE}" ]; then
        echo "========= first build, full build ============"
        NEED_FULL_BUILD=true
    elif [ "$(cat ${FINGERPRINT_FILE} 2>/dev/null)" != "${BUILD_FINGERPRINT}" ]; then
        echo "========= build config changed, full rebuild ============"
        NEED_FULL_BUILD=true
    else
        echo "========= incremental build ============"
    fi
    if $NEED_FULL_BUILD; then
        rm -rf ${BUILD_PATH}
        rm -rf ${OUTPUT_PATH}
    fi
else
    rm -rf ${COVERAGE_PATH}
    rm -rf ${BUILD_PATH}
    rm -rf ${OUTPUT_PATH}
    rm -rf ${TEST_REPORT_PATH}
fi
mkdir -p ${BUILD_PATH}
mkdir -p ${TEST_REPORT_PATH}
mkdir -p ${OUTPUT_PATH}

set -e

unset HYBM_RDMA_SWAP_SPACE_SIZE

echo "========= UT env =========="
echo "HYBM_RDMA_SWAP_SPACE_SIZE=${HYBM_RDMA_SWAP_SPACE_SIZE:-<unset>}"

dos2unix "$MOCKCPP_PATH/include/mockcpp/JmpCode.h"
dos2unix "$MOCKCPP_PATH/include/mockcpp/mockcpp.h"
dos2unix "$MOCKCPP_PATH/src/JmpCode.cpp"
dos2unix "$MOCKCPP_PATH/src/JmpCodeArch.h"
dos2unix "$MOCKCPP_PATH/src/JmpCodeX64.h"
dos2unix "$MOCKCPP_PATH/src/JmpCodeX86.h"
dos2unix "$MOCKCPP_PATH/src/JmpOnlyApiHook.cpp"
dos2unix "$MOCKCPP_PATH/src/UnixCodeModifier.cpp"
dos2unix $TEST_3RD_PATCH_PATH/*.patch
if command -v ninja &> /dev/null; then
    echo "========= build by ninja ============"
    export GENERATOR="Ninja"
    export MAKE_CMD=ninja
else
    GENERATOR="Unix Makefiles"
    export MAKE_CMD=make
fi
if ! $FAST_MODE || [ ! -f "${BUILD_PATH}/build.ninja" -a ! -f "${BUILD_PATH}/Makefile" ]; then
    cmake -G "$GENERATOR" -DCMAKE_BUILD_TYPE=ASAN -DBUILD_UT=ON -DBUILD_OPEN_ABI=ON -S . -B ${BUILD_PATH}
    $FAST_MODE && echo "${BUILD_FINGERPRINT}" > "${FINGERPRINT_FILE}"
fi
${MAKE_CMD} install -j64 -C ${BUILD_PATH}
export LD_LIBRARY_PATH=$SMEM_LIB_PATH:$HYBM_LIB_PATH:$MOCK_CANN_PATH/driver/lib64
export ASCEND_HOME_PATH=$MOCK_CANN_PATH
export ASAN_OPTIONS="detect_stack_use_after_return=1:allow_user_poisoning=1"

cd "$OUTPUT_PATH/bin/ut" && ./test_memfabric --gtest_break_on_failure --gtest_output=xml:"$TEST_REPORT_PATH/test_detail.xml" --gtest_filter=${TEST_FILTER}

if ! $FAST_MODE; then
    mkdir -p "$COVERAGE_PATH"
    cd "$OUTPUT_PATH"
    EXCLUDE_DIRS=(
            "*/3rdparty/*"
            "*/src/smem/csrc/python_wrapper/*"
            "*/src/hybm/csrc/driver/*"
            "*/acc_links/csrc/common/*"
            "*/acc_links/csrc/security/*"
            "*/acc_links/csrc/under_api/openssl/*"
            "*/hybm/csrc/common/*"
            "*/hybm/csrc/ts_engine/*"
            "*/hybm/csrc/under_api/*"
            "*/util/csrc/ptracer/tracers/*"
    )
    lcov -d "$BUILD_PATH" --c --output-file "$COVERAGE_PATH"/coverage.info -rc lcov_branch_coverage=1 --rc lcov_excl_br_line="LCOV_EXCL_BR_LINE|SM_LOG*|SM_ASSERT*|BM_LOG*|BM_ASSERT*|SM_VALIDATE_*|ASSERT*|LOG_*"
    lcov -e "$COVERAGE_PATH"/coverage.info "*/src/*" -o "$COVERAGE_PATH"/coverage.info --rc lcov_branch_coverage=1
    lcov -r "$COVERAGE_PATH"/coverage.info "${EXCLUDE_DIRS[@]}" -o "$COVERAGE_PATH"/coverage.info --rc lcov_branch_coverage=1
    genhtml -o "$COVERAGE_PATH"/result "$COVERAGE_PATH"/coverage.info --show-details --legend --rc lcov_branch_coverage=1

    lines_rate=`lcov -r "$COVERAGE_PATH"/coverage.info -o "$COVERAGE_PATH"/coverage.info --rc lcov_branch_coverage=1 | grep lines | grep -Eo "[0-9\.]+%" | tr -d '%'`
    branches_rate=`lcov -r "$COVERAGE_PATH"/coverage.info -o "$COVERAGE_PATH"/coverage.info --rc lcov_branch_coverage=1 | grep branches | grep -Eo "[0-9\.]+%" | tr -d '%'`
    echo "lines    coverage rate: ${lines_rate}%"
    echo "branches coverage rate: ${branches_rate}%"

    if [[ $(awk "BEGIN {print (${lines_rate} < 70 || ${branches_rate} < 40) ? 1 : 0}") -eq 1 ]]; then
        echo "failed: lines coverage < 70% or branches coverage < 40%"
        exit -1
    else
        exit 0
    fi
fi
