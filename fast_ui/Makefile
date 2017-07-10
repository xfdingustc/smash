##
## Makefile
##
## History:
##    2012/10/31 - [linn song]
##
## Copyright (C) 2004-20011, Transee, Inc.
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

UILIB_OBJS = $(wildcard ui_lib/*.o)
CAMERA_OBJS = $(wildcard camera_app/*.o)
THREAD_OBJS = $(wildcard $(CASTLE_TOPDIR)/service_upnp/ClassThread/*.o)

INCLUDE_DIR = -I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/freetype/usr/include/ \
							-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/freetype/usr/include/freetype2/ \
							-I$(CASTLE_TOPDIR)/../ambarella/build/include/ \
							-I$(CASTLE_TOPDIR)/fast_ui/ui_lib/ \
							-I$(CASTLE_TOPDIR)/fast_ui/camera_app/ \
							-I$(CASTLE_TOPDIR)/service_upnp/ClassThread/ \
							-I$(CASTLE_TOPDIR)/service_upnp/ \
							-I$(CASTLE_TOPDIR)/service_upnp/ClassUpnpService/ \
							-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/agbox/usr/include/ \
							-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/sqlite/usr/include \
							-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libqrencode/usr/include


LIBS =  -lfreetype -lstdc++ -lrt -ldl -lm -lpthread -lc
			 
LIBDIRS = -L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/freetype/usr/lib/ 
			
#UNIT_TEST_LDFLAG	?= $(AMBARELLA_LDFLAGS)

export CASTLE_TOPDIR

.PHONY: all clean camera_app ui_lib THREAD_OBJS


##
all : $(MODULE_OBJS) camera_app ui_lib THREAD_OBJS
	@echo "  [Build third-party unit tests]"	
#$(GCC) -v -s -o fc $(MODULE_OBJS) $(THREAD_OBJS) $(UILIB_OBJS) $(CAMERA_OBJS) $(LIBDIRS) ${LIBS}
	
$(MODULE_OBJS):	
	$(GCC) -c -Wall $(subst .o,.cpp,$(notdir $(subst \,/,$@))) $(INCLUDE_DIR) $(DEFINES) $(COMPILE_FLAGS) -o $@
	
camera_app : 
	$(AMBA_MAKEFILE_V)$(MAKE) -C $(PWD)/camera_app all 
	
ui_lib : 
	$(AMBA_MAKEFILE_V)$(MAKE) -C $(PWD)/ui_lib all 
	
THREAD_OBJS :
	$(AMBA_MAKEFILE_V)$(MAKE) -C $(CASTLE_TOPDIR)/service_upnp/ClassThread all
	 
clean:
	@echo "  [Clean third-party unit tests]"
	rm ./ui_lib/*.o
	rm ./camera_app/*.o
	rm ./*.o
	rm $(CASTLE_TOPDIR)/service_upnp/ClassThread/*.o


