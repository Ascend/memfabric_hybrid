#!/bin/bash
CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)

LONG=run-mode:,soc-version:,install-path:,build-type:,install-prefix:,
SHORT=r:,v:,i:,b:,p:,
OPTS=$(getopt -a --options $SHORT --longoptions $LONG -- "$@")
eval set -- "$OPTS"
SOC_VER="Ascend910B3"

while :; do
    case "$1" in
    -v | --soc-version)
        SOC_VER="$2"
        shift 2
        ;;
    --)
        shift
        break
        ;;
    *)
        echo "[ERROR] Unexpected input option: $1"
        break
        ;;
    esac
done

VERSION_LIST="Ascend910A Ascend910B Ascend310B1 Ascend310B2 Ascend310B3 Ascend310B4 Ascend310P1 Ascend310P3 Ascend910B1 Ascend910B2 Ascend910B3 Ascend910B4"
if [[ " $VERSION_LIST " != *" $SOC_VER "* ]]; then
    echo "ERROR: input SOC_VER should be in [$VERSION_LIST]"
    exit 1
fi

set -e
rm -rf build out
mkdir -p build
cmake -B build -DSOC_VERSION=${SOC_VER}
cmake --build build -j
cmake --install build