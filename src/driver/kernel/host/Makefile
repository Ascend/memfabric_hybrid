INCLUDE_PATH := -I$(PWD)/../../include # header files across kernel and userspace
INCLUDE_PATH += -I$(PWD)/../include # header files across kernel host and device
INCLUDE_PATH += -I$(PWD)/message
INCLUDE_PATH += -I$(PWD)/manager
INCLUDE_PATH += -I$(PWD)/init

obj-m := hybm_gvm_host.o

hybm_gvm_host-objs += init/hybm_gvm_init.o init/hybm_gvm_symbol_get.o
hybm_gvm_host-objs += message/hybm_gvm_p2p.o message/hybm_gvm_s2a.o
hybm_gvm_host-objs += manager/hybm_gvm_proc.o manager/hybm_gvm_phy_page.o manager/hybm_gvm_ioctl.o

ccflags-y :=$(INCLUDE_PATH) -DTARGET_LINUX -Wall -Wno-unused-function -O2

KDIR ?= /lib/modules/$(shell uname -r)/build
MOUT ?= $(PWD)/../build/host
BUILD_DIR = $(MOUT)
BUILD_DIR_MAKEFILE = $(MOUT)/Makefile

all: build

build: $(BUILD_DIR_MAKEFILE)
	make -C $(KDIR)/ M=$(BUILD_DIR) src=$(PWD) modules

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR_MAKEFILE): $(BUILD_DIR)
	touch "$@"

clean:
	make -C $(KDIR)/ M=$(BUILD_DIR) src=$(PWD) clean
