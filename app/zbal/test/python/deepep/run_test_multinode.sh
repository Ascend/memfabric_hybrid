#!/bin/bash

# --- Configuration ---
RANK_IPS="141.61.39.237 141.61.39.238"
PER_NODES=8

# Convert IP list (supports space, comma, semicolon) to array
RANK_IPS_ARR=(${RANK_IPS//[,;]/ })
NODE_SIZE=${#RANK_IPS_ARR[@]}

# Get all local IPs to handle multi-NIC environments
LOCAL_IPS=($(hostname -I))
[[ ${#LOCAL_IPS[@]} -eq 0 ]] && LOCAL_IPS=("127.0.0.1")

# --- Directory Setup ---
script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
cd "$script_dir" || exit

# --- Distributed Env Setup ---
export WORLD_SIZE=${NODE_SIZE}
export MASTER_ADDR=${RANK_IPS_ARR[0]} # Rank 0 is the master

# Match local IP against RANK_IPS to determine RANK
export RANK=-1
for i in "${!RANK_IPS_ARR[@]}"; do
  NODE_IP="${RANK_IPS_ARR[$i]}"
  for MY_IP in "${LOCAL_IPS[@]}"; do
    if [ "${MY_IP}" == "${NODE_IP}" ]; then
      export RANK=$i
      break 2
    fi
  done
done

# --- Validation ---
if [ "${RANK}" -eq -1 ]; then
  echo "Error: Local IP (${LOCAL_IPS[*]}) not found in RANK_IPS: ${RANK_IPS}"
  exit 1
fi

echo "=== Env Setup Success ==="
echo "  MASTER_ADDR : ${MASTER_ADDR}"
echo "  LOCAL_ADDR  : ${MY_IP}"
echo "  WORLD_SIZE  : ${WORLD_SIZE}"
echo "  MY_NODE_RANK    : ${RANK}"
echo "========================="

export ASCEND_PROCESS_LOG_PATH=./logs
export ASCEND_GLOBAL_LOG_LEVEL=2
rm -rf ./logs

# for normal mode
export DEEP_NORMAL_MODE_USE_INT8_QUANT=1
python test_normal.py --num-processes ${PER_NODES}

# for low lantency mode
#rm -rf ./export_only_prof_dir/*
#python test_low_latency.py --num-processes ${PER_NODES}

cat ./logs/rank00_*.log
