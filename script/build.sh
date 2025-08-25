#!/bin/bash

BUILD_MODE=${1:-RELEASE}
BUILD_TESTS=${2:-OFF}
BUILD_OPEN_ABI=${3:-ON}
BUILD_PYTHON=${4:-ON}
BUILD_ASAN=${5:-OFF}
ENABLE_HTRACE=${6:-ON}

if [ "$BUILD_ASAN" = "ON" ]; then
  if [ "$BUILD_MODE" = "RELEASE" ]; then
    echo "Error: BUILD_ASAN is set to ON, but BUILD_ASAN is set to RELEASE."
    echo "AddressSanitizer (ASan) should NOT be used in RELEASE builds."
    echo "Reason: ASan significantly impacts performance and memory usage, and its runtime is not intended for production."
    echo "Please either:"
    echo "  1. Set BUILD_MODE to DEBUG, or another non-release mode, OR"
    echo "  2. Set BUILD_ASAN to OFF"
    exit 1
  fi
  echo "INFO: ASan is enabled. Building in '$BUILD_MODE' mode."
fi

readonly SCRIPT_FULL_PATH=$(dirname $(readlink -f "$0"))
readonly PROJECT_FULL_PATH=$(dirname "$SCRIPT_FULL_PATH")
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

set -e
readonly ROOT_PATH=$(dirname $(readlink -f "$0"))
CURRENT_DIR=$(pwd)

cd ${ROOT_PATH}/..
PROJ_DIR=$(pwd)
bash script/gen_last_git_commit.sh

rm -rf ./build ./output/memcache ./output/hybm ./output/smem
rm -f ./output/mxc-memfabric_hybrid-1.0.0_linux_aarch64.run

mkdir build/
cmake -DCMAKE_BUILD_TYPE="${BUILD_MODE}" -DBUILD_TESTS="${BUILD_TESTS}" -DBUILD_OPEN_ABI="${BUILD_OPEN_ABI}" -DBUILD_PYTHON="${BUILD_PYTHON}" -DBUILD_ASAN="${BUILD_ASAN}" -DENABLE_HTRACE="${ENABLE_HTRACE}" -S . -B build/
make install -j32 -C build/

mkdir -p "${PROJ_DIR}/src/smem/python/mf_smem/lib"
mkdir -p "${PROJ_DIR}/src/memcache/python/pymmc/lib"
\cp -v "${PROJ_DIR}/output/smem/lib64/libmf_smem.so" "${PROJ_DIR}/src/smem/python/mf_smem/lib"
\cp -v "${PROJ_DIR}/output/hybm/lib/libmf_hybm_core.so" "${PROJ_DIR}/src/memcache/python/pymmc/lib"
\cp -v "${PROJ_DIR}/output/smem/lib64/libmf_smem.so" "${PROJ_DIR}/src/memcache/python/pymmc/lib"
\cp -v "${PROJ_DIR}/output/memcache/lib64/libmf_memcache.so" "${PROJ_DIR}/src/memcache/python/pymmc/lib"
\cp -v "${PROJ_DIR}/output/driver/lib64/libhybm_gvm.so" "${PROJ_DIR}/src/memcache/python/pymmc/lib"

if [ "${BUILD_PYTHON}" != "ON" ]; then
    echo "========= skip build python ============"
        cd ${CURRENT_DIR}
        exit 0
fi


GIT_COMMIT=$(cat script/git_last_commit.txt)
{
  echo "smem version info:"
  echo "smem version: 1.0.0"
  echo "git: ${GIT_COMMIT}"
} > VERSION

cp VERSION "${PROJ_DIR}/src/smem/python/mf_smem/"
cp VERSION "${PROJ_DIR}/src/memcache/python/pymmc/"
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

python_path_list=("/opt/buildtools/python-3.11.4")
for python_path in "${python_path_list[@]}"
do
    if [ -n "${multiple_python}" ]; then
        export PYTHON_HOME=${python_path}
        export CMAKE_PREFIX_PATH=$PYTHON_HOME
        export LD_LIBRARY_PATH=$PYTHON_HOME/lib
        export PATH=$PYTHON_HOME/bin:$BACK_PATH_EVN
    fi

    rm -f "${PROJ_DIR}"/src/smem/python/mf_smem/_pymf_smem.cpython*.so
    rm -f "${PROJ_DIR}"/src/memcache/python/pymmc/pymmc.cpython*.so

    rm -rf build/
    mkdir build/
    cmake -DCMAKE_BUILD_TYPE="${BUILD_MODE}" -DBUILD_OPEN_ABI="${BUILD_OPEN_ABI}" -DBUILD_ASAN="${BUILD_ASAN}" -S . -B build/
    make -j5 -C build _pysmem _pymmc

    \cp -v "${PROJ_DIR}"/build/src/smem/csrc/python_wrapper/_pymf_smem.cpython*.so "${PROJ_DIR}"/src/smem/python/mf_smem
    \cp -v "${PROJ_DIR}"/build/src/memcache/csrc/python_wrapper/_pymmc.cpython*.so "${PROJ_DIR}"/src/memcache/python/pymmc

    cd "${PROJ_DIR}/src/smem/python"
    rm -rf build mf_smem.egg-info
    python3 setup.py bdist_wheel
    cd "${PROJ_DIR}/src/memcache/python"
    rm -rf build pymmc.egg-info
    python3 setup.py bdist_wheel

    if [ -z "${multiple_python}" ];then
        break
    fi
done

mkdir -p "${PROJ_DIR}/output/smem/wheel"
cp "${PROJ_DIR}"/src/smem/python/dist/*.whl "${PROJ_DIR}/output/smem/wheel"
mkdir -p "${PROJ_DIR}/output/memcache/wheel"
cp "${PROJ_DIR}"/src/memcache/python/dist/*.whl "${PROJ_DIR}/output/memcache/wheel"

cd ${CURRENT_DIR}