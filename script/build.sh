#!/bin/bash

BUILD_MODE=$1
BUILD_TESTS=$2
BUILD_OPEN_ABI=$3
BUILD_PYTHON=$4

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
    BUILD_PYTHON="ON"
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

rm -rf ./build ./output

mkdir build/
cmake -DCMAKE_BUILD_TYPE="${BUILD_MODE}" -DBUILD_TESTS="${BUILD_TESTS}" -DBUILD_OPEN_ABI="${BUILD_OPEN_ABI}" -DBUILD_PYTHON="${BUILD_PYTHON}" -S . -B build/
make install -j5 -C build/

mkdir -p "${PROJ_DIR}/src/smem/python/mf_smem/lib"
mkdir -p "${PROJ_DIR}/src/memcache/python/pymmc/lib"
\cp -v "${PROJ_DIR}/output/smem/lib64/libmf_smem.so" "${PROJ_DIR}/src/smem/python/mf_smem/lib"
\cp -v "${PROJ_DIR}/output/hybm/lib/libmf_hybm_core.so" "${PROJ_DIR}/src/memcache/python/pymmc/lib"
\cp -v "${PROJ_DIR}/output/smem/lib64/libmf_smem.so" "${PROJ_DIR}/src/memcache/python/pymmc/lib"
\cp -v "${PROJ_DIR}/output/memcache/lib64/libmf_memcache.so" "${PROJ_DIR}/src/memcache/python/pymmc/lib"

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
python_path_list=("/opt/buildtools/python-3.8.5" "/opt/buildtools/python-3.9.11" "/opt/buildtools/python-3.10.2" "/opt/buildtools/python-3.11.4")
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
    cmake -DCMAKE_BUILD_TYPE="${BUILD_MODE}" -DBUILD_OPEN_ABI="${BUILD_OPEN_ABI}" -S . -B build/
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