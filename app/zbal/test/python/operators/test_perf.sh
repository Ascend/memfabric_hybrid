#!/bin/bash

CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)

cd $CURRENT_DIR

# usage: bash test_perf.sh [0|1]
# 0 - run test
# 1 - perf result
OP=${1:0}

for subdir in */; do
    subdir_path="$CURRENT_DIR/$subdir"
    script_path="$subdir_path/test_perf.sh"
    if [ -f "$script_path" ]; then
        echo "========================================="
        echo "正在执行: $script_path"
        echo "========================================="

        (cd "$subdir_path" && bash test_perf.sh $OP)

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