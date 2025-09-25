#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# This file is a part of the CANN Open Software.
# Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.

function check_env() {
    if ! command -v python3 >/dev/null 2>&1; then
        echo "Error: python3 not found in PATH"
        return 1
    fi
    if ! python3 -c "import pybind11" >/dev/null 2>&1; then
        echo "Error: pybind11 is not installed in current python environment"
        echo "Try: pip install pybind11"
        return 1
    fi
}

BUILD_MODE=$1
BUILD_TESTS=$2
BUILD_OPEN_ABI=$3
BUILD_PYTHON=$4
BUILD_COMPILER=$5
BUILD_PACKAGE=$6

if [ -z "$BUILD_MODE" ]; then
    BUILD_MODE="RELEASE"
fi

if [ -z "$BUILD_TESTS" ]; then
    BUILD_TESTS="OFF"
fi

if [ -z "$BUILD_OPEN_ABI" ]; then
    BUILD_OPEN_ABI="ON"
fi

if [ -z "$BUILD_PYTHON" ]; then
    if check_env; then
        BUILD_PYTHON="ON"
    else
        BUILD_PYTHON="OFF"
    fi
fi

if [ -z "$BUILD_COMPILER" ]; then
    BUILD_COMPILER="gcc"
fi

if [ -z "$BUILD_PACKAGE" ]; then
    BUILD_PACKAGE="ON"
fi

readonly ROOT_PATH=$(dirname $(readlink -f "$0"))

if [ "${BUILD_TESTS}" == "ON" ]; then
    echo "BUILD_TESTS, NO BUILD PYTHON"
    BUILD_PYTHON="OFF"
    BUILD_PACKAGE="OFF"

    cd test/3rdparty/
    [[ ! -d "googletest" ]] && git clone --branch v1.14.0 --depth 1 https://github.com/google/googletest.git
    [[ ! -d "mockcpp" ]] && git clone --branch v2.7 --depth 1 https://github.com/sinojelly/mockcpp.git
    cd -

    PROJECT_FULL_PATH=$(dirname "$ROOT_PATH")
    MOCKCPP_PATH="$PROJECT_FULL_PATH/test/3rdparty/mockcpp"
    dos2unix "$MOCKCPP_PATH/include/mockcpp/JmpCode.h"
    dos2unix "$MOCKCPP_PATH/include/mockcpp/mockcpp.h"
    dos2unix "$MOCKCPP_PATH/src/JmpCode.cpp"
    dos2unix "$MOCKCPP_PATH/src/JmpCodeArch.h"
    dos2unix "$MOCKCPP_PATH/src/JmpCodeX64.h"
    dos2unix "$MOCKCPP_PATH/src/JmpCodeX86.h"
    dos2unix "$MOCKCPP_PATH/src/JmpOnlyApiHook.cpp"
    dos2unix "$MOCKCPP_PATH/src/UnixCodeModifier.cpp"
fi

set -e
CURRENT_DIR=$(pwd)

cd ${ROOT_PATH}/..
PROJ_DIR=$(pwd)

rm -rf ./build ./output

mkdir build/
cmake -DCMAKE_BUILD_TYPE="${BUILD_MODE}" -DBUILD_TESTS="${BUILD_TESTS}" -DBUILD_OPEN_ABI="${BUILD_OPEN_ABI}" -DBUILD_PYTHON="${BUILD_PYTHON}" -DBUILD_COMPILER="${BUILD_COMPILER}" -S . -B build/
make install -j5 -C build/

if [ "${BUILD_PYTHON}" != "ON" ]; then
  echo "========= skip build python ============"
    if [ "${BUILD_PACKAGE}" == "ON" ]; then
        #copy mf_run package
        bash "${PROJ_DIR}"/script/run_pkg_maker/make_run.sh RELEASE OFF
        rm -rf "${PROJ_DIR}"/package
        mkdir -p "${PROJ_DIR}"/package
        cp "${PROJ_DIR}"/output/*.run "${PROJ_DIR}"/package
    fi
    cd ${CURRENT_DIR}
    exit 0
fi

mkdir -p "${PROJ_DIR}/src/smem/python/mf_smem/lib"
\cp -v "${PROJ_DIR}/output/smem/lib64/libmf_smem.so" "${PROJ_DIR}/src/smem/python/mf_smem/lib"

GIT_COMMIT=`git rev-parse HEAD`
{
  echo "smem version info:"
  echo "smem version: 1.0.0"
  echo "git: ${GIT_COMMIT}"
} > "${PROJ_DIR}/src/smem/python/mf_smem/VERSION"
{
  echo "mf_adapter version info:"
  echo "mf_adapter version: 1.0.0"
  echo "git: ${GIT_COMMIT}"
} > "${PROJ_DIR}/src/mooncake_adapter/python/mf_adapter/VERSION"

readonly BACK_PATH_EVN=$PATH
python_path_list=("/opt/buildtools/python-3.8.5" "/opt/buildtools/python-3.9.11" "/opt/buildtools/python-3.10.2" "/opt/buildtools/python-3.11.4")
for python_path in ${python_path_list[@]}
do
    if [ -n "${multiple_python}" ]; then
        export PYTHON_HOME=${python_path}
        export CMAKE_PREFIX_PATH=$PYTHON_HOME
        export LD_LIBRARY_PATH=$PYTHON_HOME/lib
        export PATH=$PYTHON_HOME/bin:$BACK_PATH_EVN

        rm -rf build/
        mkdir -p build/
        cmake -DCMAKE_BUILD_TYPE="${BUILD_MODE}" -DBUILD_OPEN_ABI="${BUILD_OPEN_ABI}" -S . -B build/
        make -j5 -C build
    fi

    rm -rf "${PROJ_DIR}"/src/smem/python/mf_smem/_pymf_smem.cpython*.so
    \cp -v "${PROJ_DIR}"/build/src/smem/csrc/python_wrapper/_pymf_smem.cpython*.so "${PROJ_DIR}"/src/smem/python/mf_smem

    cd "${PROJ_DIR}/src/smem/python"
    rm -rf build mf_smem.egg-info
    python3 setup.py bdist_wheel
    cd "${PROJ_DIR}"

    rm -rf "${PROJ_DIR}"/src/mooncake_adapter/python/mf_adapter/_pymf_transfer.cpython*.so
    \cp -v "${PROJ_DIR}"/build/src/mooncake_adapter/csrc/_pymf_transfer.cpython*.so "${PROJ_DIR}"/src/mooncake_adapter/python/mf_adapter
    mkdir -p ${PROJ_DIR}/src/mooncake_adapter/python/mf_adapter/lib
    cp -v "${PROJ_DIR}/output/smem/lib64/libmf_smem.so" "${PROJ_DIR}/src/mooncake_adapter/python/mf_adapter/lib"
    cp -v "${PROJ_DIR}/output/hybm/lib64/libmf_hybm_core.so" "${PROJ_DIR}/src/mooncake_adapter/python/mf_adapter/lib"
    cd "${PROJ_DIR}/src/mooncake_adapter/python"
    rm -rf build mf_adapter.egg-info
    python3 setup.py bdist_wheel
    cd "${PROJ_DIR}"

    if [ -z "${multiple_python}" ];then
        break
    fi
done

# copy smem wheel package
mkdir -p "${PROJ_DIR}/output/smem/wheel"
cp "${PROJ_DIR}"/src/smem/python/dist/*.whl "${PROJ_DIR}/output/smem/wheel"
rm -rf "${PROJ_DIR}"/src/smem/python/dist

# copy mooncake_adapter wheel package
mkdir -p "${PROJ_DIR}/output/mooncake_adapter/wheel"
cp "${PROJ_DIR}"/src/mooncake_adapter/python/dist/*.whl "${PROJ_DIR}/output/mooncake_adapter/wheel"
rm -rf "${PROJ_DIR}"/src/mooncake_adapter/python/dist

if [ "${BUILD_PACKAGE}" == "ON" ]; then
    #copy smem wheel package && mooncake_adapter wheel package && mf_run package
    bash "${PROJ_DIR}"/script/run_pkg_maker/make_run.sh RELEASE ON
    rm -rf "${PROJ_DIR}"/package
    mkdir -p "${PROJ_DIR}"/package
    cp "${PROJ_DIR}"/output/mooncake_adapter/wheel/*.whl "${PROJ_DIR}/package"
    cp "${PROJ_DIR}"/output/smem/wheel/*.whl "${PROJ_DIR}/package"
    cp "${PROJ_DIR}"/output/*.run "${PROJ_DIR}"/package
fi

cd ${CURRENT_DIR}