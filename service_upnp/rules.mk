
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

LIBAVF_DIR 	:= $(CASTLE_TOPDIR)/../ambarella/rootfs/packages/ts/libavf/usr/include
LIBIPC_DIR	:= $(CASTLE_TOPDIR)/../ambarella/rootfs/packages/ts/libipc/usr/include

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

ifeq ($(board),hd23a)
DEFINES += -DPLATFORM23
endif

ifeq ($(board),hd22a)
DEFINES += -DPLATFORM22A
#avahi = 1
endif

ifeq ($(board),hd22d)
DEFINES += -DPLATFORM22D
endif

ifeq ($(board),diablo)
DEFINES += -DPLATFORMDIABLO
avahi = 1
endif

ifeq ($(avahi), 1)
DEFINES += -DAVAHI_ENABLE
INCLUDE_DIR += -I$(CASTLE_TOPDIR)/service_upnp/avahi/ \
							-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/avahi/usr/include
xmlcmd = 1
endif

ifeq ($(xmlcmd),1)
DEFINES += -DisServer
INCLUDE_DIR += -I$(CASTLE_TOPDIR)/service_upnp/xml_cmd/ 
endif


avf = 1
ifeq ($(avf),1)
INCLUDE_DIR += -I$(CASTLE_TOPDIR)/libavf/ipc \
							 -I$(CASTLE_TOPDIR)/libavf/misc 
DEFINES += -DUSE_AVF
else
INCLUDE_DIR += -I$(CASTLE_TOPDIR)/ipc/ipc_vlc/ -UUSE_AVF
endif
