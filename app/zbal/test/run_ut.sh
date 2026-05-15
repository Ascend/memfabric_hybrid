#!/bin/bash

set -e

CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE:-$0}) && pwd)
ZBAL_DIR=$(cd ${CURRENT_DIR}/.. && pwd)
SGL_KERNEL_DIR=$(cd ${ZBAL_DIR}/../.. && pwd)

BUILD_DIR=${ZBAL_DIR}/test/build
OUTPUT_DIR=${ZBAL_DIR}/test
MOCK_LIB_DIR=${OUTPUT_DIR}/lib64/cann/lib64
COVERAGE_PATH="$OUTPUT_DIR/coverage"

rm -rf $BUILD_DIR
rm -rf $COVERAGE_PATH

# =============================================
# Environment setup
# =============================================
if [ -n "$ASCEND_HOME_PATH" ]; then
    _ASCEND_INSTALL_PATH=$ASCEND_HOME_PATH
else
    _ASCEND_INSTALL_PATH=/usr/local/Ascend/ascend-toolkit/latest
fi
export ASCEND_HOME_PATH=${ASCEND_HOME_PATH:-/usr/local/Ascend}
SOC_VERSION="Ascend910_9382"
export SOC_VERSION="${SOC_VERSION}"


# Build GoogleTest if not already built
GTEST_LIB=${ZBAL_DIR}/test/output/3rdparty/googletest/lib/libgtest.a
if [ ! -f "${GTEST_LIB}" ]; then
    echo "[INFO] Building GoogleTest..."
    GTEST_SRC=${ZBAL_DIR}/test/3rdparty/googletest
    GTEST_BUILD=${GTEST_SRC}/build
    GTEST_INSTALL=${ZBAL_DIR}/test/output/3rdparty/googletest
    mkdir -p ${GTEST_BUILD} ${GTEST_INSTALL}
    cmake -S ${GTEST_SRC} -B ${GTEST_BUILD} \
        -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
        -DCMAKE_BUILD_TYPE=DEBUG \
        -DCMAKE_INSTALL_PREFIX=${GTEST_INSTALL}/
    cmake --build ${GTEST_BUILD} --target install -j8
    echo "[INFO] GoogleTest built successfully."
fi

# CMake configure & build
echo "[INFO] Configuring UT build..."
mkdir -p ${BUILD_DIR}

cmake -S ${ZBAL_DIR} -B ${BUILD_DIR} \
    -DBUILD_ZBAL_MODULE_UT=ON \
    -DSOC_VERSION=$SOC_VERSION \
    -DCMAKE_BUILD_TYPE=DEBUG \
    -DDISABLE_ADAPTOR_COMPILE=ON \
    -DDISABLE_ALLOCATOR_COMPILE=ON \
    -DDISABLE_SHARE_COMPILE=ON \
    -DCMAKE_CXX_FLAGS="--coverage -fprofile-update=atomic" \
    -DCMAKE_C_FLAGS="--coverage -fprofile-update=atomic" \
    -DCMAKE_EXE_LINKER_FLAGS="--coverage" \
    -DCMAKE_SHARED_LINKER_FLAGS="--coverage"

echo "[INFO] Building UT targets..."
cmake --build ${BUILD_DIR} --target test_zbal acl_shared -j8

# Set MOCK LD_LIBRARY_PATH for runtime
export LD_LIBRARY_PATH=${MOCK_LIB_DIR}:${ASCEND_HOME_PATH}/runtime/lib64:${ASCEND_HOME_PATH}/lib64:${LD_LIBRARY_PATH}

# Run unit tests
echo "[INFO] Running unit tests..."
${ZBAL_DIR}/test/build/test/ut/testcase/test_zbal "$@"

echo "[INFO] generate coverage rate..."
mkdir -p "$COVERAGE_PATH"

EXCLUDE_DIRS=(
        "*/3rdparty/*"
        "*/doc/*"
        "*/python/*"
        "*/test/*"
)
echo "[INFO] generate coverage rate... BUILD_DIR: ${BUILD_DIR}  COVERAGE_PATH:${COVERAGE_PATH}"
lcov -c -d "$BUILD_DIR" \
    --output-file "$COVERAGE_PATH"/coverage.info \
    -rc lcov_branch_coverage=1 \
    --rc lcov_excl_br_line="LCOV_EXCL_BR_LINE|ZBAL_LOG*|ZBAL_ASSERT*|ZBAL_CHECK*|LOG_*|ZBAL_VALIDATE*" \
    --rc stop_on_error=0 \

lcov -e "$COVERAGE_PATH"/coverage.info "*/src/*" -o "$COVERAGE_PATH"/coverage.info --rc lcov_branch_coverage=1 --rc stop_on_error=0 || true
lcov -r "$COVERAGE_PATH"/coverage.info "${EXCLUDE_DIRS[@]}" -o "$COVERAGE_PATH"/coverage.info --rc lcov_branch_coverage=1 --rc stop_on_error=0 || true
genhtml -o "$COVERAGE_PATH"/result "$COVERAGE_PATH"/coverage.info --show-details --legend --rc lcov_branch_coverage=1 --rc stop_on_error=0 || true

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
