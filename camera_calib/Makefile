##############################################################################
# Makefile.am:
##############################################################################
#  Author: Chun Xiang <cxiang@cam2cloud.com>
#
#  Copyright (C) 1979 - , Chun Xiang.
#
#  All rights reserved. No Part of this file may be reproduced, stored
#  in a retrieval system, or transmitted, in any form, or by any means,
#  electronic, mechanical, photocopying, recording, or otherwise,
#  without the prior consent of Chun Xiang.
#
##############################################################################/


######################################################################
#		tools
######################################################################
GCC    = $(TOOLCHAIN_PATH)/$(CROSS_COMPILE)g++
CPP    = $(TOOLCHAIN_PATH)/$(CROSS_COMPILE)g++
LD     = $(TOOLCHAIN_PATH)/$(CROSS_COMPILE)gcc
AS     = $(TOOLCHAIN_PATH)/$(CROSS_COMPILE)as
AR     = $(TOOLCHAIN_PATH)/$(CROSS_COMPILE)ar
RANLIB = $(TOOLCHAIN_PATH)/$(CROSS_COMPILE)ranlib
MKDIR  = mkdir -p
RM     = rm -rf
CP     = cp -a
MV     = mv

COMPILE_FLAGS += -O0 -g -DDEBUG

######################################################################
#		variables
######################################################################

#COMPILE_FLAGS += -march=armv6k -mtune=arm1136j-s -msoft-float -mlittle-endian -fstrict-aliasing -Wstrict-aliasing
COMPILE_FLAGS += -fno-rtti -fno-exceptions -Wall -fexceptions
DEFINES =
INCLUDE_DIR =
LD_FLAGS =


default: all


PWD := $(shell pwd)
PROJECT_TOPDIR := $(word 1, $(subst /camera_calib, ,$(PWD)))
CASTLE_TOPDIR := $(PROJECT_TOPDIR)


TARGET_BOARD_DIR = $(CASTLE_TOPDIR)/../ambarella/boards/tw_hachi/rootfs/default


MODULE_SRCS = $(wildcard *.cpp)
MODULE_OBJS = $(MODULE_SRCS:%.cpp=%.o)

UILIB_OBJS = $(wildcard $(PWD)/ui_lib/*.o)

INCLUDE_DIR += -I$(CASTLE_TOPDIR)/camera_calib/ui_lib/ \
		-I$(CASTLE_TOPDIR)/../ambarella/include/ \
		-I$(CASTLE_TOPDIR)/../ambarella/include/arch_s2l/ \
		-I$(CASTLE_TOPDIR)/../prebuild/transee/armv7-a-hf/agbox/usr/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/cairo/usr/include/cairo/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/freetype/usr/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/freetype/usr/include/freetype2/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/alsa/alsa-lib/usr/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libqrencode/usr/include/ \
		-I$(CASTLE_TOPDIR)/../ambarella/unit_test/private/vin_test \
		-I$(CASTLE_TOPDIR)/../ambarella/unit_test/private/vout_test


LIBS =  -lstdc++ -lc -lrt -ldl -lm -lpthread -lagbox \
		-lfreetype -lz -lpng -lpixman-1 -lcairo -lasound -lqrencode

LIBDIRS = -L$(CASTLE_TOPDIR)/../prebuild/transee/armv7-a-hf/agbox/usr/lib/ \
		-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/freetype/usr/lib/ \
		-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libqrencode/usr/lib/ \
		-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libpng/usr/lib/ \
		-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/zlib/usr/lib/ \
		-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/openssl/usr/lib \
		-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libxml2/usr/lib \
		-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libjpeg/usr/lib \
		-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv6k/libzxing/usr/lib \
		-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/alsa/alsa-lib/usr/lib/ \
		-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/avahi/usr/lib/ \
		-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/cairo/usr/lib/ \
		-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/pixman/usr/lib/


export CASTLE_TOPDIR

.PHONY: all ui_lib clean

all : $(MODULE_OBJS) ui_lib
	@$(LD) $(LD_FLAGS) -o camera_calib $(MODULE_OBJS) $(UILIB_OBJS) $(LIBDIRS) ${LIBS}
	mkdir -p $(TARGET_BOARD_DIR)/usr/sbin/
	@echo "[  making camera_calib successfully  ]"
	cp camera_calib $(TARGET_BOARD_DIR)/usr/sbin/

$(MODULE_OBJS):
	@$(GCC)  $(INCLUDE_DIR) $(DEFINES) $(COMPILE_FLAGS)  -c -MMD -o $@ $(subst .o,.cpp,$(notdir $(subst \,/,$@)))

ui_lib:
	$(AMBA_MAKEFILE_V)$(MAKE) -C $(PWD)/ui_lib

clean:
	$(RM) ./*.o
	$(RM) ./*.d
	$(RM) ./ui_lib/*.o
	$(RM) ./ui_lib/*.d

