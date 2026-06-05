#!/bin/bash
set -e

CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)

cd $CURRENT_DIR

WORLD_SIZE=${1:-4}

# Format: "op_name:case_list"
CASE_CONFIG=(
    "allgather:524288, 5242880"
    "allreduce:3, 5242880, 5242881"
    "alltoallv:16, 5242880"
    "broadcast:524289, 2097152, 16777216"
    "gather:524288, 5242880"
    "p2p:5242880"
    "reducescatter:5242880"
    "scatter:5242880"
)

for entry in "${CASE_CONFIG[@]}"; do
    op_name="${entry%%:*}"
    CASE_LIST="${entry#*:}"
    subdir_path="$CURRENT_DIR/$op_name"
    script_path="$subdir_path/test_zbal_$op_name.sh"
    if [ -f "$script_path" ]; then
        echo "========================================="
        echo "Executing: $script_path"
        echo "========================================="
        echo "CASE_LIST=$CASE_LIST"

        CASE_LIST=$(echo "$CASE_LIST" | tr -d ' ' | tr ',' ' ')
        (cd "$subdir_path" && bash test_zbal_$op_name.sh "${CASE_LIST}" ${WORLD_SIZE})

        if [ $? -eq 0 ]; then
            echo "========================================="
            echo "PASS: $script_path"
            echo "========================================="
        else
            echo "========================================="
            echo "FAIL: $script_path, exit code: $?"
            echo "========================================="
        fi
        echo ""
    fi
done

echo "========================================="
echo "All operators passed"
echo "========================================="