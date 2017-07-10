##############################################################################
# Makefile:
##############################################################################
#  Author: Anthony Ginger <mapfly@gmail.com>
#
#  Copyright (C) 1979 - 2012, Anthony Ginger.
#
##############################################################################/

default: modules

MODULE_DIR			= $(shell pwd)
MODULE_NAME			= Sample
RELATE_TOP_DIR			= $(MODULE_DIR)/../../..
BUILD_TOP_DIR			= $(shell cd $(RELATE_TOP_DIR) 2>/dev/null && pwd)
BUILD_KDIR			?= $(BUILD_TOP_DIR)/ambarella/kernel/linux
BUILD_RDIR			?= $(BUILD_TOP_DIR)/ambarella/rootfs/fakeroot/lib/modules
BUILD_ARCH			?= arm

export PATH:=$(TOOLCHAIN_PATH):$(PATH)

.PHONY: all clean

modules:
	@echo "Make $(MODULE_NAME) ..."
	make ARCH=$(BUILD_ARCH) -C $(BUILD_KDIR) M=$(MODULE_DIR) $@

clean:
	@echo "Clean $(MODULE_NAME) ..."
	make ARCH=$(BUILD_ARCH) -C $(BUILD_KDIR) M=$(MODULE_DIR) $@

modules_install:
	@echo "Install $(MODULE_NAME) ..."
	make ARCH=$(BUILD_ARCH) INSTALL_MOD_PATH=$(BUILD_RDIR) -C $(BUILD_KDIR) M=$(MODULE_DIR) $@

