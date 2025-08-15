#!/bin/bash

SCRIPT_PATH="$(readlink -f "$0")"
SCRIPT_DIR="$(dirname "${SCRIPT_PATH}")"
echo "1 ${SCRIPT_DIR}"

cd "${SCRIPT_DIR}/../3rdparty"
TEST_3RDPARTY_DIR="$(pwd)"
echo "2 ${TEST_3RDPARTY_DIR}"

git clone https://codehub-dg-y.huawei.com/software-engineering-research-community/fuzz/secodefuzz.git
if [ $? -ne 0 ]; then
  echo "Failed to clone secodefuzz repository."
  exit 1
fi

cd secodefuzz
SECODEFUZZ_ROOT_DIR="$(pwd)"
git checkout -b v2.4.8 v2.4.8
if [ $? -ne 0 ]; then
  echo "Failed to checkout secodefuzz version v2.4.8."
  exit 1
fi

bash build.sh
if [ $? -ne 0 ]; then
  echo "Failed to build secodefuzz."
  exit 1
fi

cd "${SECODEFUZZ_ROOT_DIR}"
mkdir lib
cp "examples/out-bin-x64/out/libSecodepits.so" "lib"
cp "examples/out-bin-x64/out/libxml2.so.2.6.26" "lib"
cp "examples/out-bin-x64/libSecodefuzz.a" "lib"
mkdir -p include/secodefuzz
cp "Secodefuzz/secodeFuzz.h" "include/secodefuzz"
echo "SecodeFuzz installed to ${SECODEFUZZ_ROOT_DIR} successfully."