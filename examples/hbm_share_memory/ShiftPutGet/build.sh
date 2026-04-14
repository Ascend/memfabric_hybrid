#!/bin/bash
CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)

SHORT=r:,v:,i:,b:,p:,
LONG=soc-version:,
OPTS=$(getopt -a --options $SHORT --longoptions $LONG -- "$@")
eval set -- "$OPTS"
INPUT_ENV_VERSION="A3"

while :; do
    case "$1" in
    -v | --soc-version)
        INPUT_ENV_VERSION="$2"
        shift 2
        ;;
    --)
        shift
        break
        ;;
    *)
        echo "[ERROR] Unexpected in option: $1"
        break
        ;;
    esac
done

VERSION_LIST="A2 A3 A5"
if [[ " $VERSION_LIST " != *" $INPUT_ENV_VERSION "* ]]; then
    echo "ERROR: INPUT_ENV_VERSION should be in [$VERSION_LIST]"
    exit 1
fi

set -e
rm -rf build out
mkdir -p build out
cmake . -B build -DENV_VERSION=${INPUT_ENV_VERSION}
cmake --build build -j
cmake --install build