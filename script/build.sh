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
\cp -v "${PROJ_DIR}"/output/smem/lib64/_pymf_smem.cpython*.so "${PROJ_DIR}/src/smem/python/mf_smem"
\cp -v "${PROJ_DIR}/output/smem/lib64/libmf_smem.so" "${PROJ_DIR}/src/smem/python/mf_smem/lib"

GIT_COMMIT=$(cat script/git_last_commit.txt)
{
  echo "smem version info:"
  echo "smem version: 1.0.0"
  echo "git: ${GIT_COMMIT}"
} > "${PROJ_DIR}/src/smem/python/mf_smem/VERSION"

cd "${PROJ_DIR}/src/smem/python"
rm -rf build/
python3 setup.py bdist_wheel

mkdir -p "${PROJ_DIR}/output/smem/wheel"
cp "${PROJ_DIR}"/src/smem/python/dist/*.whl "${PROJ_DIR}/output/smem/wheel"

cd ${CURRENT_DIR}