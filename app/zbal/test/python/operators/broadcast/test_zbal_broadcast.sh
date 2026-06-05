#!/bin/bash

set -e

CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)

echo $CURRENT_DIR

TEST_TYPE=bfloat16_t
CASE_NUM=0  # if CASE_NUM is 0 will use CASE_LIST instead
CASE_LIST=${1:-917504}
WORLD_SIZE=${2:-16}
DATA_OP_TYPE=${3:-0}
H_SIZE=1

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
                export HCCL_IF_IP="${IPs[i]}"
                return
            fi
        done
    done
    echo "Local-IP-Not-Match"
}

export WORLD_SIZE=$WORLD_SIZE
export TEST_TYPE=$TEST_TYPE
export CURRENT_DIR=$CURRENT_DIR
export HCCL_OP_EXPANSION_MODE="AIV"

rm -rf golden output profiling.hccl* profiling.zbal*
mkdir -p golden output
python3 ${CURRENT_DIR}/scripts/data_gen.py $WORLD_SIZE $TEST_TYPE --case_num $CASE_NUM --case_list $CASE_LIST --hidden_size $H_SIZE

export CHECK_PRECISION=1
export ENABLE_PROFILING=0
nnodes=$(((WORLD_SIZE + RANK_PER_NODE - 1) / RANK_PER_NODE))
node_rank=$(get_node_idx)

if [[ $nnodes -eq 1 ]]; then
    if [[ ${ZBAL_ENABLE_PERF_TEST} = "1" ]]; then
        echo; echo -e "run hccl..."; torchrun --nnodes ${nnodes} --nproc-per-node $WORLD_SIZE --master_port 8775 ${CURRENT_DIR}/test_zbal_broadcast.py hccl --case_num $CASE_NUM --case_list $CASE_LIST --hidden_size $H_SIZE --data_op_type $DATA_OP_TYPE
    fi
    echo; echo -e "run zbal..."; torchrun --nnodes ${nnodes} --nproc-per-node $WORLD_SIZE --master_port 8775 ${CURRENT_DIR}/test_zbal_broadcast.py zbal --case_num $CASE_NUM --case_list $CASE_LIST --hidden_size $H_SIZE --data_op_type $DATA_OP_TYPE
else
    if [[ $ip_size -eq $nnodes ]]; then
        echo; echo -e "run hccl..."; torchrun --nnodes ${nnodes} --nproc-per-node $RANK_PER_NODE --node_rank ${node_rank} --master_addr "${IPs[0]}" --master_port 8775 ${CURRENT_DIR}/test_zbal_broadcast.py hccl --case_num $CASE_NUM --case_list $CASE_LIST --hidden_size $H_SIZE --data_op_type $DATA_OP_TYPE
        echo; echo -e "run zbal..."; torchrun --nnodes ${nnodes} --nproc-per-node $RANK_PER_NODE --node_rank ${node_rank} --master_addr "${IPs[0]}" --master_port 8775 ${CURRENT_DIR}/test_zbal_broadcast.py zbal --case_num $CASE_NUM --case_list $CASE_LIST --hidden_size $H_SIZE --data_op_type $DATA_OP_TYPE
    else
        echo "run ${WORLD_SIZE} ranks process but IPs size is not match"
    fi
fi

echo; echo -e "RUN WORLD_SIZE = ${WORLD_SIZE} CASES finished"
