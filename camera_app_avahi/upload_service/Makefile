##
## Makefile
##
## History:
##    2015/11/05 - [cxiang@cam2cloud.com]
##
## Copyright (C) 2004-2015, Ambarella, Inc.
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

INCLUDE_DIR = -I$(CASTLE_TOPDIR)/camera_app/ykcamera_sdk/include \
			-I$(CASTLE_TOPDIR)/camera_app/ \
			-I$(CASTLE_TOPDIR)/camera_app/ClassThread/ \
			-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/curl/usr/include/ \
			-I$(LIBIPC_DIR)/ \
			-I$(LIBAVF_DIR)/

ifeq ($(board),hellfire)
INCLUDE_DIR += -I$(CASTLE_TOPDIR)/../prebuild/transee/armv7-a-hf/agbox/usr/include/
else ifeq ($(board),hachi)
INCLUDE_DIR += -I$(CASTLE_TOPDIR)/../prebuild/transee/armv7-a-hf/agbox/usr/include/ \
			   -I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/openssl/usr/include/
else
INCLUDE_DIR += -I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/agbox/usr/include/
endif
.PHONY: all clean MODULE_OBJS

all : $(MODULE_OBJS)
	@echo "  [build upload_service class]"

$(MODULE_OBJS) :
	@$(GCC)  $(INCLUDE_DIR) $(DEFINES) $(COMPILE_FLAGS)  -c -MMD -o $@ $(subst .o,.cpp,$(notdir $(subst \,/,$@)))

clean:
	@echo "  [Clean upload_service]"
