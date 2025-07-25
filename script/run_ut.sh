#!/bin/bash

readonly SCRIPT_FULL_PATH=$(dirname $(readlink -f "$0"))
readonly PROJECT_FULL_PATH=$(dirname "$SCRIPT_FULL_PATH")

readonly BUILD_PATH="$PROJECT_FULL_PATH/build"
readonly OUTPUT_PATH="$PROJECT_FULL_PATH/output"
readonly HYBM_LIB_PATH="$OUTPUT_PATH/hybm/lib"
readonly COVERAGE_PATH="$OUTPUT_PATH/coverage"
readonly MOCKCPP_PATH="$PROJECT_FULL_PATH/test/3rdparty/mockcpp"
readonly TEST_3RD_PATCH_PATH="$PROJECT_FULL_PATH/test/3rdparty/patch"
readonly MOCK_CANN_PATH="$HYBM_LIB_PATH/cann"

cd ${PROJECT_FULL_PATH}
rm -rf ${COVERAGE_PATH}
rm -rf ${BUILD_PATH}
mkdir -p ${BUILD_PATH}

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

cmake -DCMAKE_BUILD_TYPE=ASAN -DBUILD_TESTS=ON -DBUILD_OPEN_ABI=ON -DBUILD_PYTHON=OFF -S . -B ${BUILD_PATH}
make install -j5 -C ${BUILD_PATH}
export LD_LIBRARY_PATH=$HYBM_LIB_PATH:$MOCK_CANN_PATH/driver/lib64:$LD_LIBRARY_PATH
export ASCEND_HOME_PATH=$MOCK_CANN_PATH
export ASAN_OPTIONS="detect_stack_use_after_return=1:allow_user_poisoning=1"

cd "$OUTPUT_PATH/bin" && ./test_mf_hy --gtest_output=xml:gcover_report/test_detail.xml

mkdir -p "$COVERAGE_PATH"
cd "$OUTPUT_PATH"
lcov --d "$BUILD_PATH" --c --output-file "$COVERAGE_PATH"/coverage.info -rc lcov_branch_coverage=1 --rc lcov_excl_br_line="LCOV_EXCL_BR_LINE|SM_LOG*|SM_ASSERT*|BM_LOG*|BM_ASSERT*"
lcov -e "$COVERAGE_PATH"/coverage.info "*/src/smem/*" "*/src/hybm/*" -o "$COVERAGE_PATH"/coverage.info --rc lcov_branch_coverage=1
lcov -r "$COVERAGE_PATH"/coverage.info "*/src/hybm/driver/*" -o "$COVERAGE_PATH"/coverage.info --rc lcov_branch_coverage=1
genhtml -o "$COVERAGE_PATH"/result "$COVERAGE_PATH"/coverage.info --show-details --legend --rc lcov_branch_coverage=1