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

BUILD_MODE=${1:-RELEASE}
BUILD_TESTS=${2:-OFF}
BUILD_OPEN_ABI=${3:-ON}
BUILD_PYTHON=${4:-ON}
ENABLE_PTRACER=${5:-ON}
USE_CANN=${6:-ON}
BUILD_COMPILER=${7:-gcc}
BUILD_PACKAGE=${8:-ON}

readonly SCRIPT_FULL_PATH=$(dirname $(readlink -f "$0"))
readonly PROJECT_FULL_PATH=$(dirname "$SCRIPT_FULL_PATH")

if [ "${BUILD_TESTS}" == "ON" ]; then
  readonly MOCKCPP_PATH="$PROJECT_FULL_PATH/test/3rdparty/mockcpp"
  readonly TEST_3RD_PATCH_PATH="$PROJECT_FULL_PATH/test/3rdparty/patch"
  dos2unix "$MOCKCPP_PATH/include/mockcpp/JmpCode.h"
  dos2unix "$MOCKCPP_PATH/include/mockcpp/mockcpp.h"
  dos2unix "$MOCKCPP_PATH/src/JmpCode.cpp"
  dos2unix "$MOCKCPP_PATH/src/JmpCodeArch.h"
  dos2unix "$MOCKCPP_PATH/src/JmpCodeX64.h"
  dos2unix "$MOCKCPP_PATH/src/JmpCodeX86.h"
  dos2unix "$MOCKCPP_PATH/src/JmpOnlyApiHook.cpp"
  dos2unix "$MOCKCPP_PATH/src/UnixCodeModifier.cpp"
  dos2unix $TEST_3RD_PATCH_PATH/*.patch
fi

set -e
readonly ROOT_PATH=$(dirname $(readlink -f "$0"))
CURRENT_DIR=$(pwd)

cd ${ROOT_PATH}/..
PROJ_DIR=$(pwd)

rm -rf ./build ./output

mkdir build/
cmake -DCMAKE_BUILD_TYPE="${BUILD_MODE}" -DBUILD_COMPILER="${BUILD_COMPILER}" -DBUILD_TESTS="${BUILD_TESTS}" -DBUILD_OPEN_ABI="${BUILD_OPEN_ABI}" -DBUILD_PYTHON="${BUILD_PYTHON}" -DENABLE_PTRACER="${ENABLE_PTRACER}" -DUSE_CANN="${USE_CANN}" -S . -B build/
make install -j32 -C build/

if [ "${BUILD_PYTHON}" != "ON" ]; then
    echo "========= skip build python ============"
        cd ${CURRENT_DIR}
        exit 0
fi

# mf_adapter
mkdir -p ${PROJ_DIR}/src/smem/python/mk_transfer_adapter/mf_adapter/lib
cp -v "${PROJ_DIR}/output/smem/lib64/libmf_smem.so" "${PROJ_DIR}/src/smem/python/mk_transfer_adapter/mf_adapter/lib"
cp -v "${PROJ_DIR}/output/hybm/lib64/libmf_hybm_core.so" "${PROJ_DIR}/src/smem/python/mk_transfer_adapter/mf_adapter/lib"

# memfabric_hybrid
mkdir -p ${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/lib
\cp -v "${PROJ_DIR}/output/smem/lib64/libmf_smem.so" "${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/lib"
\cp -v "${PROJ_DIR}/output/hybm/lib64/libmf_hybm_core.so" "${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/lib"

GIT_COMMIT=`git rev-parse HEAD` || true
{
  echo "mf version info:"
  echo "mf version: 1.0.0"
  echo "git: ${GIT_COMMIT}"
} > VERSION

cp VERSION "${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/"
cp VERSION "${PROJ_DIR}/src/smem/python/mk_transfer_adapter/mf_adapter/"
rm -f VERSION

readonly BACK_PATH_EVN=$PATH

# 如果 PYTHON_HOME 不存在，则设置默认值
if [ -z "$PYTHON_HOME" ]; then
    # 定义要检查的目录路径
    CHECK_DIR="/usr/local/python3.11"
    # 判断目录是否存在
    if [ -d "$CHECK_DIR" ]; then
        export PYTHON_HOME="$CHECK_DIR"
    else
        export PYTHON_HOME="/usr/local/"
    fi
    echo "Not set PYTHON_HOME，and use $PYTHON_HOME"
fi

export LD_LIBRARY_PATH=$PYTHON_HOME/lib:$LD_LIBRARY_PATH
export PATH=$PYTHON_HOME/bin:$BACK_PATH_EVN
export CMAKE_PREFIX_PATH=$PYTHON_HOME

python_path_list=("/opt/buildtools/python-3.8.5" "/opt/buildtools/python-3.9.11" "/opt/buildtools/python-3.10.2" "/opt/buildtools/python-3.11.4")
for python_path in "${python_path_list[@]}"
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

    # mf_adapter
    rm -rf "${PROJ_DIR}"/src/smem/python/mk_transfer_adapter/mf_adapter/_pymf_transfer.cpython*.so
    \cp -v "${PROJ_DIR}"/build/src/smem/csrc/python_wrapper/mk_transfer_adapter/_pymf_transfer.cpython*.so "${PROJ_DIR}"/src/smem/python/mk_transfer_adapter/mf_adapter/
    mkdir -p ${PROJ_DIR}/src/smem/python/mk_transfer_adapter/mf_adapter/lib
    cp -v "${PROJ_DIR}/output/smem/lib64/libmf_smem.so" "${PROJ_DIR}/src/smem/python/mk_transfer_adapter/mf_adapter/lib"
    cp -v "${PROJ_DIR}/output/hybm/lib64/libmf_hybm_core.so" "${PROJ_DIR}/src/smem/python/mk_transfer_adapter/mf_adapter/lib"
    cd "${PROJ_DIR}/src/smem/python/mk_transfer_adapter"
    rm -rf build mf_adapter.egg-info
    python3 setup.py bdist_wheel
    cd "${PROJ_DIR}"

    # memfabric_hybrid
    rm -rf "${PROJ_DIR}"/src/smem/python/memfabric_hybrid/memfabric_hybrid/_pymf_hybrid.cpython*.so
    \cp -v "${PROJ_DIR}"/build/src/smem/csrc/python_wrapper/memfabric_hybrid/_pymf_hybrid.cpython*.so "${PROJ_DIR}"/src/smem/python/memfabric_hybrid/memfabric_hybrid/
    rm -rf "${PROJ_DIR}"/src/python/memfabric_hybrid/_pymf_transfer.cpython*.so
    \cp -v "${PROJ_DIR}"/build/src/smem/csrc/python_wrapper/mk_transfer_adapter/_pymf_transfer.cpython*.so "${PROJ_DIR}"/src/smem/python/memfabric_hybrid/memfabric_hybrid/
    cd "${PROJ_DIR}/src/smem/python/memfabric_hybrid"
    rm -rf build memfabric_hybrid.egg-info
    python3 setup.py bdist_wheel
    cd "${PROJ_DIR}"

    if [ -z "${multiple_python}" ];then
        break
    fi
done

# copy mf_transfer wheel package
mkdir -p "${PROJ_DIR}/output/mk_transfer_adapter/wheel"
cp "${PROJ_DIR}"/src/smem/python/mk_transfer_adapter/dist/*.whl "${PROJ_DIR}/output/mk_transfer_adapter/wheel"
rm -rf "${PROJ_DIR}"/src/smem/python/mk_transfer_adapter/dist

# memfabric_hybrid
mkdir -p "${PROJ_DIR}/output/memfabric_hybrid/wheel"
cp "${PROJ_DIR}"/src/smem/python/memfabric_hybrid/dist/*.whl "${PROJ_DIR}/output/memfabric_hybrid/wheel"
rm -rf "${PROJ_DIR}"/src/smem/python/memfabric_hybrid/dist

if [ "${BUILD_PACKAGE}" == "ON" ]; then
    # copy memfabric_hybrid wheel package && mf_transfer wheel package && mf_run package
    bash "${PROJ_DIR}"/script/run_pkg_maker/make_run.sh RELEASE ON
    rm -rf "${PROJ_DIR}"/package
    mkdir -p "${PROJ_DIR}"/package
    cp "${PROJ_DIR}"/output/mk_transfer_adapter/wheel/*.whl "${PROJ_DIR}/package"
    cp "${PROJ_DIR}"/output/memfabric_hybrid/wheel/*.whl "${PROJ_DIR}/package"
    cp "${PROJ_DIR}"/output/*.run "${PROJ_DIR}"/package
fi

cd ${CURRENT_DIR}