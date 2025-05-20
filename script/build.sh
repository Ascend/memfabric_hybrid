#!/bin/bash

BUILD_MODE=$1
BUILD_TESTS=$2
BUILD_OPEN_ABI=$3

if [ -z "$BUILD_MODE" ]; then
    BUILD_MODE="RELEASE"
fi

if [ -z "$BUILD_TESTS" ]; then
    BUILD_TESTS="OFF"
fi

if [ -z "$BUILD_OPEN_ABI" ]; then
    BUILD_OPEN_ABI="ON"
fi

set -e
readonly ROOT_PATH=$(dirname $(readlink -f "$0"))
CURRENT_DIR=$(pwd)

cd ${ROOT_PATH}/..
PROJ_DIR=$(pwd)
bash script/gen_last_git_commit.sh

rm -rf ./build ./output

mkdir build/
cmake -DCMAKE_BUILD_TYPE="${BUILD_MODE}" -DBUILD_TESTS="${BUILD_TESTS}" -DBUILD_OPEN_ABI="${BUILD_OPEN_ABI}" -S . -B build/
make install -j5 -C build/

mkdir -p "${PROJ_DIR}/src/smem/python/mf_smem/lib"
\cp -v "${PROJ_DIR}/output/smem/lib64/libmf_smem.so" "${PROJ_DIR}/src/smem/python/mf_smem/lib"

GIT_COMMIT=$(cat script/git_last_commit.txt)
{
  echo "smem version info:"
  echo "smem version: 1.0.0"
  echo "git: ${GIT_COMMIT}"
} > "${PROJ_DIR}/src/smem/python/mf_smem/VERSION"

readonly BACK_PATH_EVN=$PATH
python_path_list=("/opt/buildtools/python-3.8.5" "/opt/buildtools/python-3.9.11" "/opt/buildtools/python-3.10.2" "/opt/buildtools/python-3.11.4")
for python_path in ${python_path_list[@]}
do
    if [ -n "${multiple_python}" ]; then
        export PYTHON_HOME=${python_path}
        export CMAKE_PREFIX_PATH=$PYTHON_HOME
        export LD_LIBRARY_PATH=$PYTHON_HOME/lib
        export PATH=$PYTHON_HOME/bin:$BACK_PATH_EVN
    fi

    rm -rf "${PROJ_DIR}"/src/smem/python/mf_smem/_pymf_smem.cpython*.so
    rm -f "${PROJ_DIR}"/build/src/smem/csrc/python_wrapper/_pymf_smem.cpython*.so

    rm -rf build/
    mkdir build/
    cmake -DCMAKE_BUILD_TYPE="${BUILD_MODE}" -DBUILD_OPEN_ABI="${BUILD_OPEN_ABI}" -S . -B build/
    make -j5 -C build _pysmem
    \cp -v "${PROJ_DIR}"/build/src/smem/csrc/python_wrapper/_pymf_smem.cpython*.so "${PROJ_DIR}"/src/smem/python/mf_smem

    cd "${PROJ_DIR}/src/smem/python"
    rm -rf build mf_smem.egg-info
    python3 setup.py bdist_wheel
    cd "${PROJ_DIR}"

    if [ -z "${multiple_python}" ];then
        break
    fi
done

mkdir -p "${PROJ_DIR}/output/smem/wheel"
cp "${PROJ_DIR}"/src/smem/python/dist/*.whl "${PROJ_DIR}/output/smem/wheel"

cd ${CURRENT_DIR}