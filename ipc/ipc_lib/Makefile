##
## Makefile
##
## History:
##    2011/07/13 - [linn song]
##
## Copyright (C) 2004-20011, Ambarella, Inc.
##
## All rights reserved. No Part of this file may be reproduced, stored
## in a retrieval system, or transmitted, in any form, or by any means,
## electronic, mechanical, photocopying, recording, or otherwise,
## without the prior consent of Ambarella, Inc.
##

default: all

PWD			:= $(shell pwd)
CASTLE_TOPDIR	?= $(word 1, $(subst /ipc/ipc_lib, ,$(PWD)))
#export AMBABUILD_TOPDIR
#include $(AMBABUILD_TOPDIR)/build/unit_test/common.mk

MODULE_SRCS = $(wildcard *.c)
MODULE_OBJS = $(MODULE_SRCS:%.c=%.o)

INCLUDE_DIR = -I$(CASTLE_TOPDIR)/ipc/ipc_drv/ \

CC = $(TOOLCHAIN_PATH)/$(CROSS_COMPILE)gcc
LD = $(TOOLCHAIN_PATH)/$(CROSS_COMPILE)ld

.PHONY: all clean MODULE_OBJS

all : $(MODULE_OBJS)
	@echo "  [build CThread]"
	
	
$(MODULE_OBJS) :
	$(CC) -c $(subst .o,.c,$(notdir $(subst \,/,$@))) $(INCLUDE_DIR) -o $@ 
	
clean:
	@echo "  [Clean fast_app ui lib]"



