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
BUILD_HCOM_WITH_RDMA=${9:-ON}
BUILD_HCOM_WITH_UB=${10:-OFF}
BUILD_ETCD_BACKEND=${11:-OFF}
BUILD_TOOL=${12:-cmake}
# 导出环境变量用于后续构建whl包
export MF_BUILD_HCOM=${8:-OFF}
export MF_BUILD_HCOM_WITH_RDMA=${9:-ON}
export MF_BUILD_HCOM_WITH_UB=${10:-OFF}
export BUILD_ETCD_BACKEND=${11:-OFF}
export BUILD_TOOL=${12:-cmake}


readonly SCRIPT_FULL_PATH=$(dirname $(readlink -f "$0"))
readonly PROJECT_FULL_PATH=$(dirname "$SCRIPT_FULL_PATH")
readonly MF_BUILD_JOBS="${MF_BUILD_JOBS:-32}"

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

copy_bazel_artifacts()
{
    echo "========= copy bazel artifacts to dist directory========="
    # copy from bazel-bin to output
    mkdir -p ${PROJ_DIR}/output/smem/lib64/
    cp -v "${PROJ_DIR}/bazel-bin/src/smem/csrc/libmf_smem.so" "${PROJ_DIR}/output/smem/lib64/"
    mkdir -p ${PROJ_DIR}/output/hybm/lib64/
    cp -v "${PROJ_DIR}/bazel-bin/src/hybm/csrc/libmf_hybm_core.so" "${PROJ_DIR}/output/hybm/lib64/"
    mkdir -p ${PROJ_DIR}/output/smem/include/host/
    cp -v "${PROJ_DIR}/src/smem/include/host/"*.h "${PROJ_DIR}/output/smem/include/host/"
    mkdir -p ${PROJ_DIR}/output/smem/include/device/
    cp -v "${PROJ_DIR}/src/smem/include/device/"*.h "${PROJ_DIR}/output/smem/include/device/"
    mkdir -p ${PROJ_DIR}/output/hybm/include/
    cp -v "${PROJ_DIR}/src/hybm/include/"*.h "${PROJ_DIR}/output/hybm/include/"

    # copy from bazel-bin to build
    if [ "${BUILD_PYTHON}" == "ON" ]; then
        mkdir -p "${PROJ_DIR}"/build/src/smem/csrc/python_wrapper/mk_transfer_adapter/
        cp -v "${PROJ_DIR}"/bazel-bin/src/smem/csrc/python_wrapper/mk_transfer_adapter/_pymf_transfer.so "${PROJ_DIR}"/build/src/smem/csrc/python_wrapper/mk_transfer_adapter/
        mkdir -p "${PROJ_DIR}"/build/src/smem/csrc/python_wrapper/memfabric_hybrid/
        cp -v "${PROJ_DIR}"/bazel-bin/src/smem/csrc/python_wrapper/memfabric_hybrid/_pymf_hybrid.so "${PROJ_DIR}"/build/src/smem/csrc/python_wrapper/memfabric_hybrid/
    fi

    if [ "${BUILD_HCOM}" == "ON" ]; then
        echo "========= copy hcom lib to output============"
        mkdir -p "${PROJ_DIR}"/output/3rdparty/hcom/lib/
        cp -v "${PROJ_DIR}"/bazel-bin/external/hcom/src/hcom/libhcom.so "${PROJ_DIR}"/output/3rdparty/hcom/lib/

        if [ -f "${PROJ_DIR}/bazel-bin/external/hcom/src/libboundscheck.so" ]; then
            if [ ! -d "$LIBBOUNDSCHECK_INSTALL_PATH" ]; then
                mkdir -p "$LIBBOUNDSCHECK_INSTALL_PATH"
            fi
            cp -v ${PROJ_DIR}/bazel-bin/external/hcom/src/libboundscheck.so "$LIBBOUNDSCHECK_INSTALL_PATH"
        fi
    fi

    if [ "${BUILD_ETCD_BACKEND}" == "ON" ]; then
        echo "========= copy etcd client lib to output============"
        mkdir -p ${PROJ_DIR}/output/etcd/lib64/
        cp -v "${PROJ_DIR}"/bazel-bin/src/util/etcd_client/etcd_store_backend/libetcd_client_v3.so ${PROJ_DIR}/output/etcd/lib64/
    fi
}

check_contains_path()
{
    local check_path="$1"
    local path_var="${LD_LIBRARY_PATH:-}"

    # empty LD_LIBRARY_PATH
    if [ -z "$path_var" ]; then
        return 0 # not contain
    fi

    # check every path parted by ':'
    IFS=':' read -ra paths <<< "$path_var"
    for path in "${paths[@]}"; do
        # remove '/'
        path="${path%/}"
        check_path="${check_path%/}"

        if [ "$path" = "$check_path" ]; then
            echo "========= contain $check_path============"
            return 1  # contain
        fi
    done
    echo "========= not contain $check_path============"
    return 0  # not contain
}

cd ${ROOT_PATH}/..
PROJ_DIR=$(pwd)
LIBBOUNDSCHECK_INSTALL_PATH="${PROJ_DIR}/output/3rdparty/hcom/dist/hcom_3rdparty/libboundscheck/lib/"

if [ "${BUILD_PYTHON}" == "ON" ]; then
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
        echo "Not set PYTHON_HOME, and use $PYTHON_HOME"
    fi

    export LD_LIBRARY_PATH=$PYTHON_HOME/lib:$LD_LIBRARY_PATH
    export PATH=$PYTHON_HOME/bin:$BACK_PATH_EVN
    export CMAKE_PREFIX_PATH=$PYTHON_HOME
    export LD_LIBRARY_PATH="${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/lib":$LD_LIBRARY_PATH # fix `auditwheel repair` failed
fi

if [ "${BUILD_TOOL}" == "cmake" ]; then
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
        -DBUILD_WITH_RDMA="${BUILD_HCOM_WITH_RDMA}" \
        -DBUILD_WITH_UB="${BUILD_HCOM_WITH_UB}" \
        -DBUILD_ETCD_BACKEND="${BUILD_ETCD_BACKEND}" \
        -S . \
        -B build/
    ${MAKE_CMD} install -j"${MF_BUILD_JOBS}" -C build/
else
    BAZEL_ARGS=()

    if [ "${BUILD_MODE}" == "DEBUG" ]; then
        BAZEL_ARGS+=("--compilation_mode=dbg")
    else
        BAZEL_ARGS+=("--compilation_mode=opt")
    fi

    if [ "${BUILD_UT}" == "ON" ]; then
        BAZEL_ARGS+=("--copt=-DUT_ENABLED")
    fi

    if [ "${BUILD_OPEN_ABI}" == "OFF" ]; then
        BAZEL_ARGS+=("--copt=-D_GLIBCXX_USE_CXX11_ABI=0")
    fi

    if [ "${ENABLE_PTRACER}" == "ON" ]; then
        BAZEL_ARGS+=("--copt=-DENABLE_PTRACER")
    fi

    if [ "${XPU_TYPE}" == "NPU" ]; then
        BAZEL_ARGS+=("--copt=-DASCEND_NPU")
    elif [ "${XPU_TYPE}" == "GPU" ]; then
        BAZEL_ARGS+=("--copt=-DNVIDIA_GPU")
    else
        BAZEL_ARGS+=("--copt=-DNO_XPU")
    fi

    if [ "${BUILD_HCOM}" == "ON" ]; then
        BAZEL_ARGS+=("--define=build_with_hcom=1")

        if [ "${BUILD_HCOM_WITH_RDMA}" == "OFF" ]; then
            BAZEL_ARGS+=("--define=hcom_enable_rdma=0")
        fi
        if [ "${BUILD_HCOM_WITH_UB}" == "ON" ]; then
            BAZEL_ARGS+=("--define=hcom_enable_ub=1")
        fi
    fi

    if [ "${BUILD_ETCD_BACKEND}" == "OFF" ]; then
        BAZEL_ARGS+=("--build_tag_filters=-enable_etcd_client")
    fi

    BAZEL_ARGS+=("--explain=explain.log")
    BAZEL_ARGS+=("--verbose_explanations")

    rm -rf ./output
    rm -rf ./build

    echo "bazel clean --async"
    bazel clean --async

    echo "bazel build arguments: ${BAZEL_ARGS[@]}"
    bazel build //src/... "${BAZEL_ARGS[@]}"
    copy_bazel_artifacts
fi

# hcom
if [ "${BUILD_HCOM}" == "ON" ]; then

    echo "========= copy hcom lib ============"

    # Check if the source code compilation output directory of libboundscheck exists
    if [ ! -d "$LIBBOUNDSCHECK_INSTALL_PATH" ]; then
        mkdir -p "$LIBBOUNDSCHECK_INSTALL_PATH"
    fi

    # Check if libboundscheck.so exists in the compilation output directory; if not, copy it from the system library directories.
    if [ ! -f "$LIBBOUNDSCHECK_INSTALL_PATH/libboundscheck.so" ]; then
        if [ -f "/usr/lib64/libboundscheck.so" ]; then
            cp -v /usr/lib64/libboundscheck.so "$LIBBOUNDSCHECK_INSTALL_PATH"
        elif [ -f "/usr/lib/libboundscheck.so" ]; then
            cp -v /usr/lib/libboundscheck.so "$LIBBOUNDSCHECK_INSTALL_PATH"
        else
            echo "Error: libboundscheck.so not found"
        fi
    fi
fi

if [ "${BUILD_PYTHON}" != "ON" ]; then
    echo "========= skip build python ============"
        cd ${CURRENT_DIR}
        exit 0
fi

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

if [ "${BUILD_HCOM}" == "ON" ]; then
    echo "========= copy hcom lib to wheel pkg ============"
    cp -v "${PROJ_DIR}"/output/3rdparty/hcom/lib/libhcom.so "${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/lib"
    cp -v "${LIBBOUNDSCHECK_INSTALL_PATH}"/libboundscheck.so \
          "${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/lib"
fi

if [ "${BUILD_ETCD_BACKEND}" == "ON" ]; then
    echo "========= copy etcd client lib to wheel pkg============"
    cp -v ${PROJ_DIR}/output/etcd/lib64/libetcd_client_v3.so "${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/lib"
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
rm -f "${PROJ_DIR}/output/VERSION"

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
        if [ "${BUILD_TOOL}" == "cmake" ]; then
            cmake -G "$GENERATOR" -DCMAKE_BUILD_TYPE="${BUILD_MODE}" -DBUILD_OPEN_ABI="${BUILD_OPEN_ABI}" -S . -B build/
            ${MAKE_CMD} -j"${MF_BUILD_JOBS}" -C build
        else
            bazel clean --async
            bazel build //src/... "${BAZEL_ARGS[@]}"
            copy_bazel_artifacts
        fi
    fi

    # memfabric_hybrid
    rm -rf "${PROJ_DIR}"/src/smem/python/memfabric_hybrid/memfabric_hybrid/_pymf_hybrid*.so
    \cp -v "${PROJ_DIR}"/build/src/smem/csrc/python_wrapper/memfabric_hybrid/_pymf_hybrid*.so "${PROJ_DIR}"/src/smem/python/memfabric_hybrid/memfabric_hybrid/
    rm -rf "${PROJ_DIR}"/src/smem/python/memfabric_hybrid/memfabric_hybrid/_pymf_transfer*.so
    \cp -v "${PROJ_DIR}"/build/src/smem/csrc/python_wrapper/mk_transfer_adapter/_pymf_transfer*.so "${PROJ_DIR}"/src/smem/python/memfabric_hybrid/memfabric_hybrid/
    cd "${PROJ_DIR}/src/smem/python/memfabric_hybrid"
    rm -rf build memfabric_hybrid.egg-info
    if check_contains_path "${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/lib"; then
        echo "========= not contain add to LD_LIBRARY_PATH ============"
        export LD_LIBRARY_PATH="${PROJ_DIR}/src/smem/python/memfabric_hybrid/memfabric_hybrid/lib":$LD_LIBRARY_PATH # fix `auditwheel repair` failed
    fi

    python3 setup.py bdist_wheel
    cd "${PROJ_DIR}"

    if [ -z "${multiple_python}" ];then
        break
    fi
done

# memfabric_hybrid
mkdir -p "${PROJ_DIR}/output/memfabric_hybrid/wheel"
cp "${PROJ_DIR}"/src/smem/python/memfabric_hybrid/dist/*.whl "${PROJ_DIR}/output/memfabric_hybrid/wheel"
rm -rf "${PROJ_DIR}"/src/smem/python/memfabric_hybrid/dist
rm -rf "${PROJ_DIR}"/src/smem/python/memfabric_hybrid/memfabric_hybrid/include
rm -rf "${PROJ_DIR}"/src/smem/python/memfabric_hybrid/memfabric_hybrid/lib

cd ${CURRENT_DIR}
