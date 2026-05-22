set(RUN_MODE "npu" CACHE STRING "run mode: npu")

set(ZBAL_DEVICE_DIR ${ZBAL_SRC_DIR}/csrc/operators/npu/device)

file(GLOB ZBAL_KERNEL_CLASSIC_SRCS
    ${ZBAL_DEVICE_DIR}/classic/*.cpp
)
source_group(TREE ${ZBAL_DEVICE_DIR} PREFIX "device/classic" FILES ${ZBAL_KERNEL_CLASSIC_SRCS})

file(GLOB ZBAL_KERNEL_AIV_SDMA_SRCS
    ${ZBAL_DEVICE_DIR}/aiv_sdma/*.cpp
)
source_group(TREE ${ZBAL_DEVICE_DIR} PREFIX "device/aiv_sdma" FILES ${ZBAL_KERNEL_AIV_SDMA_SRCS})

file(GLOB ZBAL_KERNEL_DISPATCH_NORMAL_SRCS
    ${ZBAL_DEVICE_DIR}/dispatch_combine_normal/*.cpp
)
source_group(TREE ${ZBAL_DEVICE_DIR} PREFIX "device/dispatch_combine_normal" FILES ${ZBAL_KERNEL_DISPATCH_NORMAL_SRCS})

file(GLOB ZBAL_KERNEL_DISPATCH_LOW_LATENCY_SRCS
    ${ZBAL_DEVICE_DIR}/dispatch_combine_low_latency/*.cpp
)
source_group(TREE ${ZBAL_DEVICE_DIR} PREFIX "device/dispatch_combine_low_latency" FILES ${ZBAL_KERNEL_DISPATCH_LOW_LATENCY_SRCS})

ascendc_library(zbal_kernel STATIC
    ${ZBAL_KERNEL_CLASSIC_SRCS}
    ${ZBAL_KERNEL_AIV_SDMA_SRCS}
    ${ZBAL_KERNEL_DISPATCH_NORMAL_SRCS}
    ${ZBAL_KERNEL_DISPATCH_LOW_LATENCY_SRCS}
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