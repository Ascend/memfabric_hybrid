#!/bin/bash
#
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# This file is a part of the CANN Open Software.
# Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
#

# eg. bash script/performance_compare.sh

readonly SCRIPT_FULL_PATH=$(dirname $(readlink -f "$0"))
readonly PROJECT_FULL_PATH=$(dirname "$SCRIPT_FULL_PATH")
readonly PROJECT_PATH="$PROJECT_FULL_PATH/temp"
readonly COMPARE_PATH="$PROJECT_PATH/shmem"
readonly ALLREDUCE_PATH="$COMPARE_PATH/examples/matmul_allreduce"

# Default Args
RANK_SIZE="8"
M=1024
N=2048
K=8192
AVE_ORIGIN=0
AVE_NEW=0
RANGE=10

function get_average()
{
    values=($(find ./output -name "task_time*.csv" -exec grep "MIX_AIC" {} + | awk -F',' '{print$6}' | sed -n '1~10!p'))

    sum=0
    count=${#values[@]}

    for value in "${values[@]}"; do
        sum=$(echo "$sum + $value" | bc)
    done

    if [ "$1" = "origin" ]; then
        AVE_ORIGIN=$(echo "scale=6;$sum / $count" | bc)
    else
        AVE_NEW=$(echo "scale=6;$sum / $count" | bc)
    fi
}

# obtain shmem, compile shmem
mkdir -p ${PROJECT_PATH}
cd ${PROJECT_PATH}
git clone https://gitee.com/ascend/shmem.git
bash $COMPARE_PATH/scripts/build.sh -examples

# obtain original avg.
cd ${ALLREDUCE_PATH}
if [ -d ${ALLREDUCE_PATH}/output ]; then
    rm -rf ${ALLREDUCE_PATH}/output
fi

bash ./scripts/run.sh -ranks ${RANK_SIZE} -M ${M} -K ${K} -N ${N}
if [[ $? != 0 ]]; then
    echo "[ERROR] origin run.sh error.."
fi
get_average "origin"

# obtain new avg.
rm -rf ./output
if [ -d ${PROJECT_FULL_PATH}/output ]; then
    bash script/build.sh RELEASE OFF OFF OFF
fi

\cp -rf $PROJECT_FULL_PATH/output/smem/include/smem $COMPARE_PATH/install/memfabric_hybrid/include
\cp -f $PROJECT_FULL_PATH/output/hybm/lib64/* $COMPARE_PATH/install/memfabric_hybrid/lib
\cp -f $PROJECT_FULL_PATH/output/smem/lib64/* $COMPARE_PATH/install/memfabric_hybrid/lib
cd $ALLREDUCE_PATH
bash ./scripts/run.sh -ranks ${RANK_SIZE} -M ${M} -K ${K} -N ${N}
if [[ $? != 0 ]]; then
    echo "[ERROR] new run.sh error.."
fi
get_average "new"

# echo
rm -rf ${PROJECT_PATH}
diff=$(echo "scale=6;${AVE_NEW} - ${AVE_ORIGIN}" | bc)
echo "----------------------------------------------------"
echo "        [AVE_ORIGIN is ${AVE_ORIGIN}]"
echo "        [AVE_NEW is ${AVE_NEW}]"

if [[ $(echo "${diff} < 0" | bc -l) -eq 1 ]]; then
    echo "        Performance Optimization After Modification"
elif [[ $(echo "${diff} <= ${RANGE}" | bc -l) -eq 1 ]]; then
    echo "        The performance gap after modification is not significant."
elif [[ $(echo "${diff} > ${RANGE}" | bc -l) -eq 1 ]]; then
    echo "        Performance Degradation after modification"
    echo "----------------------------------------------------"
    exit 1
fi
echo "----------------------------------------------------"
exit 0