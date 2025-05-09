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
rm -rf ./build ./output

mkdir build/
cmake -DCMAKE_BUILD_TYPE="${BUILD_MODE}" -DBUILD_TESTS="${BUILD_TESTS}" -DBUILD_OPEN_ABI="${BUILD_OPEN_ABI}" -S . -B build/
make install -j5 -C build/

cd ${CURRENT_DIR}