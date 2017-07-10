

default: all
.PHONY: all depend clean

vlc_topdir := ../../../ambarella/rootfs/packages/vlc/usr
ipc_drv_topdir := ../ipc_drv
ipc_lib_topdir := ../ipc_lib

GCC    := $(TOOLCHAIN_PATH)/$(CROSS_COMPILE)gcc
CPP    := $(TOOLCHAIN_PATH)/$(CROSS_COMPILE)g++
CFLAGS := -march=armv6k -mtune=arm1136j-s -msoft-float -mlittle-endian -fno-rtti -fno-exceptions -fstrict-aliasing -Wstrict-aliasing -O3 -DNDEBUG
MODULE_DIR	:= $(shell pwd)

INCLUDE := -I$(ipc_drv_topdir) -I$(ipc_lib_topdir) -I $(vlc_topdir)/include/ -I .

all:
	@echo making vlc_server
	@$(CPP) $(CFLAGS) $(INCLUDE) -o vlc_server ipc_vlc_server.c $(ipc_lib_topdir)/libipc.c -lpthread -L$(vlc_topdir)/lib/ -lvlc -lvlccore
	@echo making vlc_test
	@$(CPP) $(CFLAGS) $(INCLUDE) -o vlc_test vlc_test.c ipc_vlc_client.c $(ipc_lib_topdir)/libipc.c -lpthread

clean:
	rm -f vlc_server vlc_test

