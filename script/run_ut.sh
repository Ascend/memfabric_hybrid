#!/bin/bash

readonly SCRIPT_FULL_PATH=$(dirname $(readlink -f "$0"))
readonly PROJECT_FULL_PATH=$(dirname "$SCRIPT_FULL_PATH")

readonly BUILD_PATH="$PROJECT_FULL_PATH/build"
readonly OUTPUT_PATH="$PROJECT_FULL_PATH/output"
readonly HYBM_LIB_PATH="$OUTPUT_PATH/hybm/lib"
readonly SMEM_LIB_PATH="$OUTPUT_PATH/smem/lib64"
readonly MEMCACHE_LIB_PATH="$OUTPUT_PATH/memcache/lib64"
readonly COVERAGE_PATH="$OUTPUT_PATH/coverage"
readonly TEST_REPORT_PATH="$OUTPUT_PATH/bin/gcover_report"
readonly MOCKCPP_PATH="$PROJECT_FULL_PATH/test/3rdparty/mockcpp"
readonly TEST_3RD_PATCH_PATH="$PROJECT_FULL_PATH/test/3rdparty/patch"
readonly MOCK_CANN_PATH="$HYBM_LIB_PATH/cann"

cd ${PROJECT_FULL_PATH}
rm -rf ${COVERAGE_PATH}
rm -rf ${BUILD_PATH}
rm -rf ${OUTPUT_PATH}
rm -rf ${TEST_REPORT_PATH}
mkdir -p ${BUILD_PATH}
mkdir -p ${TEST_REPORT_PATH}

dos2unix "$MOCKCPP_PATH/include/mockcpp/JmpCode.h"
dos2unix "$MOCKCPP_PATH/include/mockcpp/mockcpp.h"
dos2unix "$MOCKCPP_PATH/src/JmpCode.cpp"
dos2unix "$MOCKCPP_PATH/src/JmpCodeArch.h"
dos2unix "$MOCKCPP_PATH/src/JmpCodeX64.h"
dos2unix "$MOCKCPP_PATH/src/JmpCodeX86.h"
dos2unix "$MOCKCPP_PATH/src/JmpOnlyApiHook.cpp"
dos2unix "$MOCKCPP_PATH/src/UnixCodeModifier.cpp"
dos2unix $TEST_3RD_PATCH_PATH/*.patch

cmake -DCMAKE_BUILD_TYPE=DEBUG -DBUILD_TESTS=ON -DBUILD_OPEN_ABI=ON -S . -B ${BUILD_PATH}
make install -j5 -C ${BUILD_PATH}
export LD_LIBRARY_PATH=$MEMCACHE_LIB_PATH:$SMEM_LIB_PATH:$HYBM_LIB_PATH:$MOCK_CANN_PATH/driver/lib64:$LD_LIBRARY_PATH
export ASCEND_HOME_PATH=$MOCK_CANN_PATH

set -e

cd "$OUTPUT_PATH/bin/ut" && ./test_mmc_test --gtest_output=xml:"$TEST_REPORT_PATH/test_detail.xml"

mkdir -p "$COVERAGE_PATH"
cd "$OUTPUT_PATH"
lcov --d "$BUILD_PATH" --c --output-file "$COVERAGE_PATH"/coverage.info -rc lcov_branch_coverage=1 --rc lcov_excl_br_line="LCOV_EXCL_BR_LINE|MMC_LOG*|MMC_RETURN*|MMC_ASSERT*|MMC_VALIDATE*"
lcov -e "$COVERAGE_PATH"/coverage.info "*/src/memcache/*" -o "$COVERAGE_PATH"/coverage.info --rc lcov_branch_coverage=1
genhtml -o "$COVERAGE_PATH"/result "$COVERAGE_PATH"/coverage.info --show-details --legend --rc lcov_branch_coverage=1