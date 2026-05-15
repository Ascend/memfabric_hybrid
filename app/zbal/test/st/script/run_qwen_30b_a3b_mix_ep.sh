echo performance | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
sysctl -w vm.swappiness=0
sysctl -w kernel.numa_balancing=0
sysctl -w kernel.sched_migration_cost_ns=50000

export SGLANG_SET_CPU_AFFINITY=1
unset https_proxy
unset http_proxy
unset HTTPS_PROXY
unset HTTP_PROXY
unset ASCEND_LAUNCH_BLOCKING
source /usr/local/Ascend/ascend-toolkit/set_env.sh
source /usr/local/Ascend/nnal/atb/set_env.sh
source /usr/local/Ascend/ascend-toolkit/latest/opp/vendors/customize/bin/set_env.bash
export PATH=/usr/local/Ascend/8.5.0/compiler/bishengir/bin:$PATH

export PYTHONPATH=/home/CI_HOME_for_25.2.0/zbal/test_tools/sglang/sglang/python:$PYTHONPATH

MODEL_PATH=/home/weights/Qwen3-30B-A3B-W8A8
EAGLE_PATH=/home/weights/Qwen3-a3B_eagle3

export SGLANG_DISAGGREGATION_BOOTSTRAP_TIMEOUT=600

LOCAL_HOST1=`hostname -I|awk -F " " '{print$1}'`
LOCAL_HOST2=`hostname -I|awk -F " " '{print$2}'`

echo "${LOCAL_HOST1}"
echo "${LOCAL_HOST2}"

export HCCL_SOCKET_IFNAME=lo
export GLOO_SOCKET_IFNAME=lo
export HCCL_OP_EXPANSION_MODE="AIV"
export SGLANG_ENABLE_OVERLAP_PLAN_STREAM=1
export SGLANG_ENABLE_SPEC_V2=1

export HCCL_BUFFSIZE=1300
export DEEPEP_NORMAL_LONG_SEQ_PER_ROUND_TOKENS=1024
export DEEPEP_NORMAL_LONG_SEQ_ROUND=8
export SGLANG_DEEPEP_NUM_MAX_DISPATCH_TOKENS_PER_RANK=192

# zbal
export HCCL_BUFFSIZE=200
unset PYTORCH_NPU_ALLOC_CONF
export SGLANG_ZBAL_LOCAL_MEM_SIZE=58368
export SGLANG_ENABLE_TP_MEMORY_INBALANCE_CHECK=0

# zbal if use mix alloc
export PYTORCH_NPU_ALLOC_CONF=expandable_segments:True
export ZBAL_NPU_ALLOC_CONF=use_vmm_for_static_memory:True
export SGLANG_ZBAL_BOOTSTRAP_URL="tcp://127.0.0.1:24669"
# zbal if support graph（need custom pta）
# export ZBAL_ENABLE_GRAPH=1
# export ZBAL_HCCL_OP="broadcast,scatter,reduce_scatter,_reduce_scatter_base,alltoall_base"

#export TASK_QUEUE_ENABLE=0

#export SGLANG_NPU_PROFILING=True
#export SGLANG_NPU_PROFILING_BS=48
#export SGLANG_NPU_PROFILING_STAGE=decode
#export SGLANG_NPU_PROFILING_STEP=10


python -m sglang.launch_server --model-path $MODEL_PATH \
    --host 127.0.0.1 --port 7239 --trust-remote-code --nnodes 1 --node-rank 0  \
    --attention-backend ascend --device npu  --quantization modelslim  \
    --max-running-requests 192 \
    --disable-radix-cache \
    --disable-cuda-graph \
    --chunked-prefill-size -1 --max-prefill-tokens 8192 \
    --tp-size 4 --enable-dp-attention --enable-dp-lm-head --dp-size 2 --mem-fraction-static 0.80 --cuda-graph-bs 42 88 96 --dtype bfloat16 \
    --speculative-algorithm EAGLE3 --speculative-draft-model-path $EAGLE_PATH \
    --speculative-draft-model-quantization unquant --speculative-num-steps 3 --speculative-eagle-topk 1 --speculative-num-draft-tokens 4 \
    --base-gpu-id 4 \
    --moe-a2a-backend deepep  --deepep-mode auto