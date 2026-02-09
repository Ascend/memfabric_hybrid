# Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

#Basic path settings
ASCEND_HOME_PATH := $(shell echo $$ASCEND_HOME_PATH)
ASCEND_KERNEL_HAL_HOME := $(shell dirname $(ASCEND_HOME_PATH))
#for a2
#ASCEND_KERNEL_HAL_H = $(ASCEND_KERNEL_HAL_HOME)/driver/kernel/inc/driver/ascend_kernel_hal.h
#ASCEND_KERNEL_HAL_H = $(ASCEND_KERNEL_HAL_HOME)/driver/kernel/dev_inc/inc/ascend_kernel_hal.h
ASCEND_KERNEL_HAL_H = /usr/local/Ascend/driver/kernel/inc/driver/ascend_kernel_hal.h
# for a5
#ASCEND_KERNEL_HAL_H = /usr/local/Ascend/driver/kernel/dev_inc/inc/ascend_kernel_hal.h
NDR_SRC_DIR := $(shell dirname $(CURDIR))
NDR_BUILD_DIR := $(CURDIR)
SECURE_PATH ?= $(NDR_SRC_DIR)

# Kernel module definition
obj-m += ndr_peer_mem.o
ndr_peer_mem-objs := npu_direct_rdma.o

# 编译标志，包含头文件路径
#EXTRA_CFLAGS += -I$(SECURE_PATH)/include -I$(SECURE_PATH)/src -I$(ASCEND_KERNEL_HAL_HOME)/driver/kernel/inc/driver -DSECUREC_EXPORT_KERNEL_SYMBOL=0
EXTRA_CFLAGS += -I$(SECURE_PATH)/include \
                -I$(SECURE_PATH)/src \
                -I/usr/local/Ascend/driver/kernel/dev_inc/inc \
                -I/usr/local/Ascend/driver/kernel/inc/driver \
                -DSECUREC_EXPORT_KERNEL_SYMBOL=0

# RDMA 符号表和头文件路径
OFA_KERNEL ?= /usr/src/ofa_kernel/default
ifneq ($(shell test -d $(OFA_KERNEL) && echo "true" || echo "" ),)
    $(info INFO: Building with MLNX_OFED from: $(OFA_KERNEL))
    EXTRA_CFLAGS += -I$(OFA_KERNEL)/include -I$(OFA_KERNEL)/include/rdma
    KBUILD_EXTRA_SYMBOLS += $(OFA_KERNEL)/Module.symvers
else
    $(info INFO: Building with Inbox InfiniBand Stack)
    KBUILD_EXTRA_SYMBOLS += /lib/modules/$(shell uname -r)/kernel/drivers/infiniband/core/Module.symvers
endif

# Ascend 驱动符号表（替换为实际路径）
ASCEND_SYMVERS ?= /path/to/ascend/Module.symvers
ifneq ($(shell test -f $(ASCEND_SYMVERS) && echo "true" || echo "" ),)
    KBUILD_EXTRA_SYMBOLS += $(ASCEND_SYMVERS)
endif

# 合并符号表
SYMVERS_FILE = $(NDR_BUILD_DIR)/my.symvers

# 伪目标
.PHONY: all clean setup gen_ascend_symvers

# 编译模块
all: setup
	@echo "Building in: $(NDR_BUILD_DIR)"
	@echo "Current directory: $(CURDIR)"
	@echo "Current arch: $(ARCH)"
	make -C /lib/modules/$(shell uname -r)/build M=$(NDR_BUILD_DIR) KBUILD_EXTRA_SYMBOLS="$(SYMVERS_FILE)" modules

# 合并符号表
setup: gen_ascend_symvers
	@echo "Setup - CURDIR: $(CURDIR)"
	@echo "NDR_SRC_DIR: $(NDR_SRC_DIR)"
	@echo "ASCEND_HOME_PATH: $(ASCEND_HOME_PATH)"
	@echo "NDR_BUILD_DIR: $(NDR_BUILD_DIR)"
	@echo "ASCEND_KERNEL_HAL_H: $(ASCEND_KERNEL_HAL_H)"
	@echo "COMPAT_ASCEND_KERNEL_HAL_H: $(COMPAT_ASCEND_KERNEL_HAL_H)"
	@[ -d "$(NDR_BUILD_DIR)/driver" ] || mkdir -p $(NDR_BUILD_DIR)/driver
	@if [ -f "$(ASCEND_KERNEL_HAL_H)" ]; then \
	    cp -f $(ASCEND_KERNEL_HAL_H) $(NDR_BUILD_DIR)/driver/ascend_kernel_hal.h; \
	    echo "Copied ascend_kernel_hal.h"; \
	elif [ -f "$(COMPAT_ASCEND_KERNEL_HAL_H)" ]; then \
	    cp -f $(COMPAT_ASCEND_KERNEL_HAL_H) $(NDR_BUILD_DIR)/driver/ascend_kernel_hal.h; \
	    echo "Copied compat_ascend_kernel_hal.h"; \
	else \
	    echo "Error: No valid header file found"; \
	    exit 1; \
	fi
	@echo -n "" > $(SYMVERS_FILE)
	@if [ -f "$(OFA_KERNEL)/Module.symvers" ]; then \
	    cp -f $(OFA_KERNEL)/Module.symvers $(SYMVERS_FILE); \
	    echo "Added MLNX_OFED symbols to $(SYMVERS_FILE)"; \
	elif [ -f "/lib/modules/$(shell uname -r)/kernel/drivers/infiniband/core/Module.symvers" ]; then \
	    cp -f /lib/modules/$(shell uname -r)/kernel/drivers/infiniband/core/Module.symvers $(SYMVERS_FILE); \
	    echo "Added ib_core symbols to $(SYMVERS_FILE)"; \
	fi
	@if [ -f "$(NDR_SRC_DIR)/ascend.symvers" ]; then \
	    cat $(NDR_SRC_DIR)/ascend.symvers >> $(SYMVERS_FILE); \
	    echo "Added Ascend symbols to $(SYMVERS_FILE)"; \
	fi

# 清理
clean:
	@echo "Cleaning in: $(NDR_BUILD_DIR)"
	make -C /lib/modules/$(shell uname -r)/build M=$(NDR_BUILD_DIR) clean
	rm -f $(SYMVERS_FILE)
