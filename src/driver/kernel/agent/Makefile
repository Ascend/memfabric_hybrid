INCLUDE_PATH += -I$(PWD)
INCLUDE_PATH += -I$(PWD)/../include

obj-m := hybm_gvm_agent.o
ccflags-y := $(INCLUDE_PATH) -DTARGET_LINUX -Wall -Wno-unused-function -O2

ARCH := arm64

export ARCH           # pass to kbuild
export CROSS_COMPILE  # pass to kbuild
export CC             # pass to kbuild

MOUT ?= $(PWD)/../build/agent
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
