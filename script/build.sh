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

export BUILD_MODE=${1:-RELEASE}
BUILD_UT=${2:-OFF}
BUILD_OPEN_ABI=${3:-OFF}
BUILD_PYTHON=${4:-ON}
ENABLE_PTRACER=${5:-ON}
export XPU_TYPE=${6:-NPU} # 导出环境变量用于后续构建whl包
BUILD_TEST=${7:-OFF}
BUILD_HCOM=${8:-OFF}

readonly SCRIPT_FULL_PATH=$(dirname $(readlink -f "$0"))
readonly PROJECT_FULL_PATH=$(dirname "$SCRIPT_FULL_PATH")

if [ "${BUILD_UT}" == "ON" ]; then
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
if command -v ninja &> /dev/null; then
    echo "========= build by ninja ============"
    export GENERATOR="Ninja"
    export MAKE_CMD=ninja
else
    GENERATOR="Unix Makefiles"
    export MAKE_CMD=make
fi
mkdir build/
cmake \
    -G "$GENERATOR"  \
    -DCMAKE_BUILD_TYPE="${BUILD_MODE}" \
    -DBUILD_UT="${BUILD_UT}" \
    -DBUILD_OPEN_ABI="${BUILD_OPEN_ABI}" \
    -DBUILD_PYTHON="${BUILD_PYTHON}" \
    -DENABLE_PTRACER="${ENABLE_PTRACER}" \
    -DXPU_TYPE="${XPU_TYPE}" \
    -DBUILD_TEST="${BUILD_TEST}" \
    -DBUILD_HCOM="${BUILD_HCOM}" \
    -S . \
    -B build/
${MAKE_CMD} install -j32 -C build/

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
# --- 动态库：lib/ ---
\cp -v "${PROJ_DIR}/output/smem/lib64/libmf_smem.so" "${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/lib/"
\cp -v "${PROJ_DIR}/output/hybm/lib64/libmf_hybm_core.so" "${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/lib/"
# --- 头文件：smem/include/host/ ---
mkdir -p ${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/include/smem/host
cp -v "${PROJ_DIR}/output/smem/include/host/"*.h "${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/include/smem/host/"
# --- 头文件：smem/include/device/ ---
mkdir -p ${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/include/smem/device
cp -v "${PROJ_DIR}/output/smem/include/device/"*.h "${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/include/smem/device/"
# --- 头文件：hybm/include/ ---
mkdir -p ${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/include/hybm
cp -v "${PROJ_DIR}/output/hybm/include/"*.h "${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/include/hybm/"

# hcom
if [ "${BUILD_HCOM}" == "ON" ]; then
    echo "========= copy hcom lib ============"
        \cp -v "${PROJ_DIR}"/output/3rdparty/hcom/lib/libhcom.so "${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/lib"
        \cp -v "${PROJ_DIR}"/output/3rdparty/hcom/dist/hcom_3rdparty/libboundscheck/lib/libboundscheck.so \
               "${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/lib"
fi

VERSION="$(cat VERSION | tr -d '[:space:]')"
export MEMFABRIC_VERSION="${VERSION}"
echo "VERSION IS ${VERSION}"
GIT_COMMIT=`git rev-parse HEAD` || true
{
  echo "mf version info:"
  echo "mf version: ${MEMFABRIC_VERSION}"
  echo "git: ${GIT_COMMIT}"
} > "${PROJ_DIR}/output/VERSION"

cp "${PROJ_DIR}/output/VERSION" "${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/"
cp "${PROJ_DIR}/output/VERSION" "${PROJ_DIR}/src/smem/python/mk_transfer_adapter/mf_adapter/"
rm -f "${PROJ_DIR}/output/VERSION"

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
        cmake -G "$GENERATOR" -DCMAKE_BUILD_TYPE="${BUILD_MODE}" -DBUILD_OPEN_ABI="${BUILD_OPEN_ABI}" -S . -B build/
        ${MAKE_CMD} -j5 -C build
    fi

    # mf_adapter
    rm -rf "${PROJ_DIR}"/src/smem/python/mk_transfer_adapter/mf_adapter/_pymf_transfer.cpython*.so
    \cp -v "${PROJ_DIR}"/build/src/smem/csrc/python_wrapper/mk_transfer_adapter/_pymf_transfer.cpython*.so "${PROJ_DIR}"/src/smem/python/mk_transfer_adapter/mf_adapter/
    mkdir -p ${PROJ_DIR}/src/smem/python/mk_transfer_adapter/mf_adapter/lib
    cp -v "${PROJ_DIR}/output/smem/lib64/libmf_smem.so" "${PROJ_DIR}/src/smem/python/mk_transfer_adapter/mf_adapter/lib"
    cp -v "${PROJ_DIR}/output/hybm/lib64/libmf_hybm_core.so" "${PROJ_DIR}/src/smem/python/mk_transfer_adapter/mf_adapter/lib"
    cd "${PROJ_DIR}/src/smem/python/mk_transfer_adapter"
    rm -rf build mf_adapter.egg-info
    export LD_LIBRARY_PATH="${PROJ_DIR}/src/smem/python/mk_transfer_adapter/mf_adapter/lib":$LD_LIBRARY_PATH # fix `auditwheel repair` failed
    python3 setup.py bdist_wheel
    cd "${PROJ_DIR}"

    # memfabric_hybrid
    rm -rf "${PROJ_DIR}"/src/smem/python/memfabric_hybrid/memfabric_hybrid/_pymf_hybrid.cpython*.so
    \cp -v "${PROJ_DIR}"/build/src/smem/csrc/python_wrapper/memfabric_hybrid/_pymf_hybrid.cpython*.so "${PROJ_DIR}"/src/smem/python/memfabric_hybrid/memfabric_hybrid/
    rm -rf "${PROJ_DIR}"/src/python/memfabric_hybrid/_pymf_transfer.cpython*.so
    \cp -v "${PROJ_DIR}"/build/src/smem/csrc/python_wrapper/mk_transfer_adapter/_pymf_transfer.cpython*.so "${PROJ_DIR}"/src/smem/python/memfabric_hybrid/memfabric_hybrid/
    cd "${PROJ_DIR}/src/smem/python/memfabric_hybrid"
    rm -rf build memfabric_hybrid.egg-info
    export LD_LIBRARY_PATH="${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/lib":$LD_LIBRARY_PATH # fix `auditwheel repair` failed
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

cd ${CURRENT_DIR}