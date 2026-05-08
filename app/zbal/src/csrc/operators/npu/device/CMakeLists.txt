set(RUN_MODE "npu" CACHE STRING "run mode: npu")

ascendc_library(zbal_kernel STATIC
        ${ZBAL_SRC_DIR}/csrc/operators/npu/device/classic/zbal_kernel_allgather.cpp
        ${ZBAL_SRC_DIR}/csrc/operators/npu/device/classic/zbal_kernel_allreduce.cpp
        ${ZBAL_SRC_DIR}/csrc/operators/npu/device/classic/zbal_kernel_alltoall.cpp
        ${ZBAL_SRC_DIR}/csrc/operators/npu/device/classic/zbal_kernel_alltoallv.cpp
        ${ZBAL_SRC_DIR}/csrc/operators/npu/device/classic/zbal_kernel_barrier.cpp
        ${ZBAL_SRC_DIR}/csrc/operators/npu/device/classic/zbal_kernel_reducescatter.cpp
        ${ZBAL_SRC_DIR}/csrc/operators/npu/device/classic/zbal_kernel_broadcast.cpp
        ${ZBAL_SRC_DIR}/csrc/operators/npu/device/classic/zbal_kernel_scatter.cpp
        ${ZBAL_SRC_DIR}/csrc/operators/npu/device/classic/zbal_kernel_send.cpp
        ${ZBAL_SRC_DIR}/csrc/operators/npu/device/classic/zbal_kernel_recv.cpp
        ${ZBAL_SRC_DIR}/csrc/operators/npu/device/dispatch_combine_normal/zbal_kernel_dispatch_layout.cpp
        ${ZBAL_SRC_DIR}/csrc/operators/npu/device/dispatch_combine_normal/zbal_kernel_notify_dispatch.cpp
        ${ZBAL_SRC_DIR}/csrc/operators/npu/device/dispatch_combine_normal/zbal_kernel_dispatch_normal.cpp
        ${ZBAL_SRC_DIR}/csrc/operators/npu/device/dispatch_combine_normal/zbal_kernel_combine_normal.cpp
        ${ZBAL_SRC_DIR}/csrc/operators/npu/device/dispatch_combine_low_latency/zbal_kernel_dispatch_low_latency.cpp
        ${ZBAL_SRC_DIR}/csrc/operators/npu/device/dispatch_combine_low_latency/zbal_kernel_combine_low_latency.cpp
)

ascendc_include_directories(zbal_kernel PRIVATE
        $ENV{SHMEM_HOME_PATH}/memfabric_hybrid/include/smem/device
        ${ASCEND_HOME_PATH}/include
        ${ASCEND_HOME_PATH}/include/aclnn
        ${ASCEND_HOME_PATH}/include/aclnn/opdev
        ${ASCEND_HOME_PATH}/asc/include
        ${ZBAL_SRC_DIR}/include
        ${ZBAL_SRC_DIR}/csrc/common
        ${ZBAL_SRC_DIR}/csrc/operators
        ${ZBAL_SRC_DIR}/csrc/operators/npu
        ${ZBAL_SRC_DIR}/csrc/operators/npu/device/common
)

ascendc_compile_definitions(zbal_kernel PRIVATE
        ASCENDC_DUMP=0
)