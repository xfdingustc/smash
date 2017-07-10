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
CASTLE_TOPDIR	?= $(word 1, $(subst /service_upnp, ,$(PWD)))
export CASTLE_TOPDIR

include $(CASTLE_TOPDIR)/service_upnp/rules.mk

MODULE_SRCS = $(wildcard *.cpp)
MODULE_OBJS = $(MODULE_SRCS:%.cpp=%.o)

INCLUDE_DIR += -I$(CASTLE_TOPDIR)/service_upnp/ClassThread/ \
	-I$(CASTLE_TOPDIR)/service_upnp/ \
	-I$(LIBIPC_DIR)/ \
	-I$(LIBAVF_DIR)/ \
	-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libupnp/usr/include \
	-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/agbox/usr/include/ \
	-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/sqlite/usr/include \
	-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libxml2/usr/include/libxml2/

DEFINES += -DUSE_AVF

.PHONY: all clean MODULE_OBJS

all : $(MODULE_OBJS)
	@echo "  [build upnp class]"
	
	
$(MODULE_OBJS) :
	$(GCC)  $(INCLUDE_DIR) $(DEFINES) $(COMPILE_FLAGS)  -c -MMD -o $@ $(subst .o,.cpp,$(notdir $(subst \,/,$@))) 
	
clean:
	@echo "  [Clean fast_app ui lib]"



