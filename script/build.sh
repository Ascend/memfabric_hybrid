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

set -e
readonly ROOT_PATH=$(dirname $(readlink -f "$0"))
CURRENT_DIR=$(pwd)

cd ${ROOT_PATH}/..
PROJ_DIR=$(pwd)
bash script/gen_last_git_commit.sh

rm -rf ./build ./output

mkdir build/
bash script/compile_acc_links.sh "${BUILD_MODE}" "${BUILD_TESTS}" "${BUILD_OPEN_ABI}"
cmake -DCMAKE_BUILD_TYPE="${BUILD_MODE}" -DBUILD_TESTS="${BUILD_TESTS}" -DBUILD_OPEN_ABI="${BUILD_OPEN_ABI}" -DBUILD_PYTHON="${BUILD_PYTHON}" -S . -B build/
make install -j5 -C build/

mkdir -p "${PROJ_DIR}/src/smem/python/mf_smem/lib"
\cp -v "${PROJ_DIR}/output/smem/lib64/libmf_smem.so" "${PROJ_DIR}/src/smem/python/mf_smem/lib"

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
    fi

    rm -rf "${PROJ_DIR}"/src/smem/python/mf_smem/_pymf_smem.cpython*.so
    rm -f "${PROJ_DIR}"/build/src/smem/csrc/python_wrapper/_pymf_smem.cpython*.so

    rm -rf build/
    mkdir -p build/
    cmake -DCMAKE_BUILD_TYPE="${BUILD_MODE}" -DBUILD_OPEN_ABI="${BUILD_OPEN_ABI}" -S . -B build/
    make -j5 -C build _pysmem
    \cp -v "${PROJ_DIR}"/build/src/smem/csrc/python_wrapper/_pymf_smem.cpython*.so "${PROJ_DIR}"/src/smem/python/mf_smem

    cd "${PROJ_DIR}/src/smem/python"
    rm -rf build mf_smem.egg-info
    python3 setup.py bdist_wheel
    cd "${PROJ_DIR}"
    mkdir -p build/
    cmake -DCMAKE_BUILD_TYPE="${BUILD_MODE}" -DBUILD_OPEN_ABI="${BUILD_OPEN_ABI}" -S . -B build/

    rm -rf "${PROJ_DIR}"/src/mooncake_adapter/python/mf_adapter/_pymf_transfer.cpython*.so
    rm -f "${PROJ_DIR}"/build/src/mooncake_adapter/csrc/_pymf_transfer.cpython*.so

    make -j5 -C build _pytransfer
    \cp -v "${PROJ_DIR}"/build/src/mooncake_adapter/csrc/_pymf_transfer.cpython*.so "${PROJ_DIR}"/src/mooncake_adapter/python/mf_adapter

    mkdir -p "${PROJ_DIR}/src/mooncake_adapter/python/mf_adapter/lib"
    cp -v "${PROJ_DIR}/output/smem/lib64/libmf_smem.so" "${PROJ_DIR}/src/mooncake_adapter/python/mf_adapter/lib"
    cp -v "${PROJ_DIR}/output/hybm/lib/libmf_hybm_core.so" "${PROJ_DIR}/src/mooncake_adapter/python/mf_adapter/lib"

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

# copy mooncake_adapter wheel package
mkdir -p "${PROJ_DIR}/output/mooncake_adapter/wheel"
cp "${PROJ_DIR}"/src/mooncake_adapter/python/dist/*.whl "${PROJ_DIR}/output/mooncake_adapter/wheel"

cd ${CURRENT_DIR}