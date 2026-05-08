#!/bin/bash

set -e

CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)

echo $CURRENT_DIR

export HCCL_IF_IP=''
WORLD_SIZE=${2:-16}
TEST_TYPE=float
CASE_NUM=0  # if CASE_NUM is 0 will use CASE_LIST instead
CASE_LIST=${1:-7340032}

RANK_PER_NODE=16
IPs=()
ip_size=${#IPs[@]}

function get_node_idx()
{
    local_ips=`hostname -I | tr ' ' '\n'`
    for ip in $local_ips; do
        for i in "${!IPs[@]}"; do
            if [[ "${IPs[i]}" == "$ip" ]]; then
                echo $i
                return
            fi
        done
    done
    echo "Local-IP-Not-Match"
}

nnodes=$(((WORLD_SIZE + RANK_PER_NODE - 1) / RANK_PER_NODE))
node_rank=$(get_node_idx)

if [[ $ip_size -ne $nnodes ]]; then
    echo "run ${WORLD_SIZE} ranks process but IPs size is not match"
    exit 1
fi

export WORLD_SIZE=$WORLD_SIZE
export TEST_TYPE=$TEST_TYPE
export CURRENT_DIR=$CURRENT_DIR
export HCCL_OP_EXPANSION_MODE="AIV"

rm -rf golden output
mkdir -p golden output
python3 ${CURRENT_DIR}/scripts/data_gen.py $WORLD_SIZE $TEST_TYPE --case_num $CASE_NUM --case_list $CASE_LIST

export CHECK_PRECISION=0
export ENABLE_PROFILING=1

if [[ $nnodes -eq 1 ]]; then
    torchrun --nproc-per-node $WORLD_SIZE --master-port 8779 ${CURRENT_DIR}/test_zbal_reducescatter.py hccl --case_num $CASE_NUM --case_list $CASE_LIST
    torchrun --nproc-per-node $WORLD_SIZE --master-port 8779 ${CURRENT_DIR}/test_zbal_reducescatter.py zbal --case_num $CASE_NUM --case_list $CASE_LIST
else
    torchrun --nnodes ${nnodes} --nproc-per-node $RANK_PER_NODE --node_rank ${node_rank} --master_addr "${IPs[0]}" --master_port 8779 ${CURRENT_DIR}/test_zbal_reducescatter.py hccl --case_num $CASE_NUM --case_list $CASE_LIST
    torchrun --nnodes ${nnodes} --nproc-per-node $RANK_PER_NODE --node_rank ${node_rank} --master_addr "${IPs[0]}" --master_port 8779 ${CURRENT_DIR}/test_zbal_reducescatter.py zbal --case_num $CASE_NUM --case_list $CASE_LIST
fi