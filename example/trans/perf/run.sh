#!/bin/bash
# transfer_perf {rankSize} {rankId} {deviceID} {useSdma} {testBm} tcp://{ip}:{port} {memType}

./transfer_perf 2 0 2 1 1 tcp://127.0.0.1:22052 0 &
./transfer_perf 2 1 3 1 1 tcp://127.0.0.1:22052 0 &