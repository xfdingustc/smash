
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

ifeq ($(board),hellfire)
	LIBAVF_DIR := $(CASTLE_TOPDIR)/../prebuild/libavf/linux/usr/include/
	LIBIPC_DIR := $(CASTLE_TOPDIR)/../prebuild/libavf/linux/usr/include
else ifeq ($(board),hachi)
	LIBAVF_DIR := $(CASTLE_TOPDIR)/../prebuild/libavf/linux/usr/include/
	LIBIPC_DIR := $(CASTLE_TOPDIR)/../prebuild/libavf/linux/usr/include
else
	LIBAVF_DIR := $(CASTLE_TOPDIR)/../ambarella/rootfs/packages/ts/libavf/usr/include
	LIBIPC_DIR := $(CASTLE_TOPDIR)/../ambarella/rootfs/packages/ts/libipc/usr/include
endif

#COMPILE_FLAGS += -march=armv6k -mtune=arm1136j-s -msoft-float -mlittle-endian -fstrict-aliasing -Wstrict-aliasing
COMPILE_FLAGS += -fno-rtti -fno-exceptions -Wall -fexceptions
DEFINES =
INCLUDE_DIR = 
LD_FLAGS = 

ifeq ($(xmpp),1)
DEFINES += -DXMPPENABLE
INCLUDE_DIR += -I$(CASTLE_TOPDIR)/../external/gloox/out/include/ 
xmlcmd = 1
endif

ifeq ($(noupnp), 1)
DEFINES += -Dnoupnp
endif


ifeq ($(debug),1)
COMPILE_FLAGS += -O0 -g -DDEBUG
else
COMPILE_FLAGS += -O3
LD_FLAGS += -s
endif

ifeq ($(board),hd22a)
DEFINES += -DPLATFORM22A
DEFINES += -DARCH_A5S
avahi = 1
endif

ifeq ($(board),hd22d)
DEFINES += -DPLATFORM22D
DEFINES += -DARCH_A5S
endif

ifeq ($(board),diablo)
DEFINES += -DPLATFORMDIABLO
DEFINES += -DARCH_A5S
avahi = 1
endif

ifeq ($(board),hellfire)
DEFINES += -DPLATFORMHELLFIRE
DEFINES += -DARCH_S2L
DEFINES += -DENABLE_STILL_CAPTURE
DEFINES += -DENABLE_LED
DEFINES += -DENABLE_LOUDSPEAKER
avahi = 1
endif

ifeq ($(board),hachi)
DEFINES += -DPLATFORMHACHI
DEFINES += -DARCH_S2L
DEFINES += -DENABLE_STILL_CAPTURE
#DEFINES += -DENABLE_LED
DEFINES += -DENABLE_LOUDSPEAKER
avahi = 1
endif

ifeq ($(avahi), 1)
DEFINES += -DAVAHI_ENABLE
INCLUDE_DIR += -I$(CASTLE_TOPDIR)/avahi/
ifeq ($(board),hellfire)
	INCLUDE_DIR += -I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/avahi/usr/include
else ifeq ($(board),hachi)
	INCLUDE_DIR += -I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/avahi/usr/include
else
	INCLUDE_DIR += -I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/avahi/usr/include
endif
xmlcmd = 1
endif

ifeq ($(xmlcmd),1)
DEFINES += -DisServer
INCLUDE_DIR += -I$(CASTLE_TOPDIR)/xml_cmd/ 
endif


avf = 1
ifeq ($(avf),1)
DEFINES += -DUSE_AVF
else
INCLUDE_DIR += -I$(CASTLE_TOPDIR)/ipc/ipc_vlc/ -UUSE_AVF
endif

ifeq ($(youkucam),1)
DEFINES += -DSUPPORT_YKCAM
endif