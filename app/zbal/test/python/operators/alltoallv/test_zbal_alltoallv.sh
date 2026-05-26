#!/bin/bash

CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE:-$0}) && pwd)
echo $CURRENT_DIR

TEST_TYPE=bfloat16_t
export TEST_TYPE=$TEST_TYPE
export CURRENT_DIR=$CURRENT_DIR
rm -rf golden output
mkdir -p golden output

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

CASE_LIST=${1:-917504}
WORLD_SIZE=${2:-4}
rank_size=${WORLD_SIZE}

python3 ${CURRENT_DIR}/scripts/data_gen.py $TEST_TYPE $rank_size

export ENABLE_PROFILING=1

nnodes=$(((rank_size + RANK_PER_NODE - 1) / RANK_PER_NODE))
node_rank=$(get_node_idx)
if [[ $nnodes -eq 1 ]]; then
    if [[ ${ZBAL_ENABLE_ALLTOALL_PERF_TEST} = "1" ]]; then
        echo; echo -e "run hccl..."; torchrun --nnodes ${nnodes} --nproc-per-node $rank_size --master_port 8877 ${CURRENT_DIR}/test_zbal_alltoallv.py hccl
    fi
    echo; echo -e "run zbal..."; torchrun --nnodes ${nnodes} --nproc-per-node $rank_size --master_port 8877 ${CURRENT_DIR}/test_zbal_alltoallv.py zbal
else
    if [[ $ip_size -eq $nnodes ]]; then
        echo; echo -e "run hccl..."; torchrun --nnodes ${nnodes} --nproc-per-node $RANK_PER_NODE --node_rank ${node_rank} --master_addr "${IPs[0]}" --master_port 8877 ${CURRENT_DIR}/test_zbal_alltoallv.py hccl
        echo; echo -e "run zbal..."; torchrun --nnodes ${nnodes} --nproc-per-node $RANK_PER_NODE --node_rank ${node_rank} --master_addr "${IPs[0]}" --master_port 8877 ${CURRENT_DIR}/test_zbal_alltoallv.py zbal
    else
        echo "run ${rank_size} ranks process but IPs size is not match"
    fi
fi