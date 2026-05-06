#!/bin/bash

# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.

# Script to run transfer performance test with both rank 0 and rank 1

set -e  # Exit on any error

# Default values
STORE_URL="tcp://127.0.0.1:12050"
NUM_THREADS=2
DATA_OP_TYPE="sdma"
NPU_ID_0=0  # NPU ID for rank 0
NPU_ID_1=7  # NPU ID for rank 1

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --store-url)
            STORE_URL="$2"
            shift 2
            ;;
        --num-threads)
            NUM_THREADS="$2"
            shift 2
            ;;
        --data-op-type)
            DATA_OP_TYPE="$2"
            shift 2
            ;;
        --npu-id-0)
            NPU_ID_0="$2"
            shift 2
            ;;
        --npu-id-1)
            NPU_ID_1="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --store-url STRING     Config store URL (default: tcp://127.0.0.1:12050)"
            echo "  --num-threads INT      Number of concurrent threads (default: 1)"
            echo "  --data-op-type STRING  Data operation type: sdma or rdma (default: sdma)"
            echo "  --npu-id-0 INT         NPU device ID for rank 0 (default: 0)"
            echo "  --npu-id-1 INT         NPU device ID for rank 1 (default: 0)"
            echo "  -h, --help            Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use -h or --help for usage information."
            exit 1
            ;;
    esac
done

echo "Starting transfer performance test..."
echo "Store URL: $STORE_URL"
echo "Num threads: $NUM_THREADS"
echo "Data operation type: $DATA_OP_TYPE"
echo "NPU ID for rank 0: $NPU_ID_0"
echo "NPU ID for rank 1: $NPU_ID_1"
echo

# Start rank 0 (sender) in background first, as it creates the config store
echo "Starting rank 0 (sender)..."
python transfer_performance.py --rank-id 0 --store-url "$STORE_URL" --num-threads "$NUM_THREADS" --data-op-type "$DATA_OP_TYPE" --npu-id "$NPU_ID_0" &
RANK0_PID=$!

# Wait a moment for rank 0 to initialize and create config store
sleep 5

# Start rank 1 (receiver)
echo "Starting rank 1 (receiver)..."
python transfer_performance.py --rank-id 1 --store-url "$STORE_URL" --num-threads "$NUM_THREADS" --data-op-type "$DATA_OP_TYPE" --npu-id "$NPU_ID_1"

# Wait for rank 0 to finish
echo "Waiting for rank 0 to finish..."
wait $RANK0_PID

echo "Transfer performance test completed!"