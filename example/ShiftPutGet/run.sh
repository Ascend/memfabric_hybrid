#!/bin/bash
RANK_SIZE=$1
IP_PORT=$2
echo " ====== example run, ranksize: ${RANK_SIZE} ip: ${IP_PORT} ====="

rm -f shm_kernels
cp ./out/bin/shm_kernels ./

export LD_LIBRARY_PATH=$(pwd)/out/lib:$(pwd)/out/lib64:/usr/local/Ascend/ascend-toolkit/latest/lib64:$(pwd)/../../output/smem/lib:$(pwd)/../../output/hybmm/lib:$LD_LIBRARY_PATH

for (( idx = 0; idx < ${RANK_SIZE}; idx = idx + 1 )); do
    ./shm_kernels ${RANK_SIZE} ${idx} ${IP_PORT} &
done