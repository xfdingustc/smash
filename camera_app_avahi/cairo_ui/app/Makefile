##
## Makefile
##
## History:
##    2011/07/13 - [linn song]
##
## Copyright (C) 2004-2016, Transee, Inc.
##
## All rights reserved. No Part of this file may be reproduced, stored
## in a retrieval system, or transmitted, in any form, or by any means,
## electronic, mechanical, photocopying, recording, or otherwise,
## without the prior consent of Ambarella, Inc.
##

default: all

PWD			:= $(shell pwd)
CASTLE_TOPDIR	?= $(word 1, $(subst /camera_app, ,$(PWD)))
export CASTLE_TOPDIR

include $(CASTLE_TOPDIR)/camera_app/rules.mk

MODULE_SRCS = $(wildcard *.cpp)
MODULE_OBJS = $(MODULE_SRCS:%.cpp=%.o)

DEFINES += -DUSE_AVF

INCLUDE_DIR = -I$(CASTLE_TOPDIR)/../ambarella/include/ \
		-I$(CASTLE_TOPDIR)/camera_app/cairo_ui/ui_lib/ \
		-I$(CASTLE_TOPDIR)/camera_app/cairo_ui/app/ \
		-I$(CASTLE_TOPDIR)/camera_app/cairo_ui/ \
		-I$(CASTLE_TOPDIR)/camera_app/ClassThread/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/curl/usr/include/ \
		-I$(CASTLE_TOPDIR)/camera_app/ \
		-I$(CASTLE_TOPDIR)/ipc/ipc_lib/ \
		-I$(CASTLE_TOPDIR)/ipc/ipc_drv/ \
		-I$(CASTLE_TOPDIR)/libavf/ipc

ifeq ($(board),hellfire)
INCLUDE_DIR += -I$(CASTLE_TOPDIR)/../ambarella/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/transee/armv7-a-hf/agbox/usr/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libqrencode/usr/include \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/alsa/alsa-lib/usr/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/cairo/usr/include/cairo
else ifeq ($(board),hachi)
INCLUDE_DIR += -I$(CASTLE_TOPDIR)/../ambarella/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/transee/armv7-a-hf/agbox/usr/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libqrencode/usr/include \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/alsa/alsa-lib/usr/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/freetype/usr/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/freetype/usr/include/freetype2/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/cairo/usr/include/cairo/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/curl/usr/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/avahi/usr/include/
else
INCLUDE_DIR += -I$(CASTLE_TOPDIR)/../ambarella/build/include/ \
		-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/agbox/usr/include/ \
		-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libqrencode/usr/include/ \
		-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/alsa/alsa-lib/usr/include/
endif

.PHONY: all clean MODULE_OBJS

all : $(MODULE_OBJS)
	@echo "  [Build ok]"	
	
$(MODULE_OBJS) :
	@$(GCC) -c $(subst .o,.cpp,$(notdir $(subst \,/,$@))) $(INCLUDE_DIR) $(DEFINES) $(COMPILE_FLAGS) -o $@ 
	
clean:
	@echo "  [Clean ]"



