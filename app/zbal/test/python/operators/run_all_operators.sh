#!/bin/bash
set -e

CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)

cd $CURRENT_DIR

DEFAULT_CASE_LIST=${1:-917504}
WORLD_SIZE=${2:-4}

declare -A CASE_CONFIG=(
    ["allgather"]="524288, 5242880"
    ["allreduce"]="3, 5242880, 5242881"
    ["alltoallv"]="16, 5242880"
    ["broadcast"]="524289, 2097152, 16777216"
    ["gather"]="524288, 5242880"
    ["p2p"]="5242880"
    ["reducescatter"]="5242880"
    ["scatter"]="5242880"
)

for subdir in */; do
    subdir_path="$CURRENT_DIR/$subdir"
    subdir_name="${subdir%/}"
    script_path="$subdir_path/test_zbal_$subdir_name.sh"
    if [ -f "$script_path" ]; then
        echo "========================================="
        echo "正在执行: $script_path"
        echo "========================================="

        if [ -n "${CASE_CONFIG[$subdir_name]+x}" ]; then
            CASE_LIST="${CASE_CONFIG[$subdir_name]}"
            echo "读取到CASE_LIST配置：CASE_LIST=$CASE_LIST"
        else
            CASE_LIST="$DEFAULT_CASE_LIST"
            echo "未找到CASE_LIST配置，使用全局默认：CASE_LIST=$DEFAULT_CASE_LIST"
        fi

        CASE_LIST=$(echo "$CASE_LIST" | tr -d ' ' | tr ',' ' ')
        (cd "$subdir_path" && bash test_zbal_$subdir_name.sh "${CASE_LIST}" ${WORLD_SIZE})

        if [ $? -eq 0 ]; then
            echo "========================================="
            echo "✓ $script_path 执行成功"
            echo "========================================="
        else
            echo "========================================="
            echo "✗ $script_path 执行失败，退出码: $?"
            echo "========================================="
        fi
        echo ""
    fi
done