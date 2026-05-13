#!/bin/bash

CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)

cd $CURRENT_DIR

CASE_LIST=${1:-917504}

WORLD_SIZE=${2:-4}

for subdir in */; do
    subdir_path="$CURRENT_DIR/$subdir"
    subdir_name="${subdir%/}"
    script_path="$subdir_path/test_zbal_$subdir_name.sh"
    if [ -f "$script_path" ]; then
        echo "========================================="
        echo "正在执行: $script_path"
        echo "========================================="

        (cd "$subdir_path" && bash test_zbal_$subdir_name.sh ${CASE_LIST} ${WORLD_SIZE})

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