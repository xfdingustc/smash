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


MODULE_SRCS = $(wildcard *.cpp)
MODULE_OBJS = $(MODULE_SRCS:%.cpp=%.o)


INCLUDE_DIR += -I$(CASTLE_TOPDIR)/../ambarella/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/cairo/usr/include/cairo/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/freetype/usr/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/freetype/usr/include/freetype2/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libqrencode/usr/include/

export CASTLE_TOPDIR

.PHONY: all clean MODULE_OBJS

all : $(MODULE_OBJS)
	@echo "  [Build ok]"

$(MODULE_OBJS) :
	@$(GCC) -c $(subst .o,.cpp,$(notdir $(subst \,/,$@))) $(INCLUDE_DIR) $(DEFINES) $(COMPILE_FLAGS)  -o $@

clean:
	@echo "  [Clean ]"

