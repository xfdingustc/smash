##
## Makefile
##
## History:
##    2011/12/13 - [linn song]
##
## Copyright (C) 2004-20011
##
##

default: all

RM = rm -f

PWD := $(shell pwd)
PROJECT_TOPDIR := $(word 1, $(subst /service_upnp, ,$(PWD)))
CASTLE_TOPDIR := $(PROJECT_TOPDIR)

APP_INSTALL_DIR	:= $(CASTLE_TOPDIR)/../ambarella/rootfs/packages/ts/service_upnp/usr/local/bin/
ifeq ($(board),hd22a)
APP_INSTALL_DIR	:= $(CASTLE_TOPDIR)/../ambarella/boards/tsf_hd22a/rootfs/default/usr/local/bin/
endif
ifeq ($(board),hd23a)
APP_INSTALL_DIR	:= $(CASTLE_TOPDIR)/../ambarella/boards/tsf_hd23a/rootfs/default/usr/local/bin/
endif
ifeq ($(board),diablo)
APP_INSTALL_DIR	:= $(CASTLE_TOPDIR)/../ambarella/boards/agb_diablo/rootfs/default/usr/local/bin/
endif
RES_INSTALL_DIR	:= $(CASTLE_TOPDIR)/../ambarella/rootfs/packages/ts/service_upnp/usr/local/share/ui/

MODULE_SRCS = $(wildcard *.cpp)

include rules.mk

MODULE_SRCS = \
	class_sd_service_decode.cpp \
	class_sd_service_network.cpp \
	class_sd_service_power.cpp \
	class_sd_service_rec.cpp \
	class_sd_service_storage.cpp \
	class_sd_service_stream.cpp \
	service_upnp.cpp \
	upnp_sdcc.cpp \
	class_ipc_rec.cpp \
	wavAudioPlay.cpp \
	MotorController.cpp \
	Class_Upgrader.cpp
	
ifeq ($(avf),1)
MODULE_SRCS += avf_media_client.cpp
else
MODULE_SRCS += ipc_vlc_client.cpp
endif
MODULE_OBJS = $(MODULE_SRCS:%.cpp=%.o)

THREAD_OBJS = $(wildcard ClassThread/*.o)
SERVICE_UPNP_OBJS = $(wildcard ClassUpnpService/*.o)
LIBIPC_OBJS = $(wildcard ipc_lib/*.o)

ifeq ($(xmpp),1)
XMPP_OBJS = $(wildcard xmpp_client/*.o)
scan_OBJS = $(wildcard codeScan/*.o)

endif

ifeq ($(xmlcmd), 1)

endif

UILIB_OBJS = $(wildcard $(CASTLE_TOPDIR)/fast_ui/ui_lib/*.o)
CAMERA_OBJS = $(wildcard $(CASTLE_TOPDIR)/fast_ui/camera_app/*.o)
UI_OBJS = $(wildcard $(CASTLE_TOPDIR)/fast_ui/*.o)

INCLUDE_DIR += -I$(CASTLE_TOPDIR)/service_upnp/ \
	-I$(CASTLE_TOPDIR)/service_upnp/ClassThread/ \
	-I$(CASTLE_TOPDIR)/service_upnp/ClassUpnpService/ \
	-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libupnp/usr/include/ \
	-I$(LIBIPC_DIR)/ \
	-I$(LIBAVF_DIR)/ \
	-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/agbox/usr/include/ \
	-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/freetype/usr/include/ \
	-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/freetype/usr/include/freetype2/ \
	-I$(CASTLE_TOPDIR)/../ambarella/build/include/ \
	-I$(CASTLE_TOPDIR)/fast_ui/ui_lib/ \
	-I$(CASTLE_TOPDIR)/fast_ui/camera_app/ \
	-I$(CASTLE_TOPDIR)/fast_ui \
	-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libqrencode/usr/include/ \
	-I$(CASTLE_TOPDIR)/service_upnp/xmpp_client/ \
	-I$(CASTLE_TOPDIR)/service_upnp/codeScan/ \
	-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libjpeg/usr/include/ \
	-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libzxing/usr/include/ \
	-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libxml2/usr/include/libxml2/ \
	-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/alsa/alsa-lib/usr/include/ \
	-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/openssl/usr/include/
	

LIBS =  -lstdc++ -lc -lrt -ldl -lm -lpthread  -lupnp -lixml -lthreadutil -lagbox \
		-lfreetype -lqrencode -lpng -lz -lssl -lcrypto -lzxing -ljpeg -lasound -lavahi-core -lavahi-common

ifeq ($(xmlcmd),1)
LIBS += -lxml2
endif

ifeq ($(avahi), 1)
AVAHI_OBJS = $(wildcard $(CASTLE_TOPDIR)/service_upnp/avahi/*.o)
endif 

ifeq ($(xmlcmd),1)
XMLCMD_OBJS = $(wildcard $(CASTLE_TOPDIR)/service_upnp/xml_cmd/*.o)
endif

ifeq ($(xmpp),1)
LIBS+= -lgloox -lresolv -lturnclient
endif
		 
LIBDIRS = -L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libupnp/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/agbox/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/freetype/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libqrencode/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libpng/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/zlib/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/openssl/usr/lib \
	-L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libxml2/usr/lib \
	-L$(CASTLE_TOPDIR)/../external/gloox/out/lib/ \
	-L$(CASTLE_TOPDIR)/../external/turnserver-1.8.1.2/lib/ \
	-L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libjpeg/usr/lib \
	-L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libzxing/usr/lib \
	-L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/alsa/alsa-lib/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/avahi/usr/lib/

UNIT_TEST_LDFLAG	?= $(AMBARELLA_LDFLAGS)

export CASTLE_TOPDIR

.PHONY: all clean thread service_upnp lib_ipc ui_lib xmpp_client scan_OBJS xml_cmd avahi

all : $(MODULE_OBJS) thread service_upnp lib_ipc ui_lib xmpp_client scan_OBJS xml_cmd avahi
	$(LD) $(LD_FLAGS) -v -o service_upnp $(MODULE_OBJS) $(THREAD_OBJS) $(SERVICE_UPNP_OBJS) $(LIBIPC_OBJS) $(UILIB_OBJS) $(UI_OBJS) $(CAMERA_OBJS) $(XMPP_OBJS) $(scan_OBJS) $(XMLCMD_OBJS) $(AVAHI_OBJS) $(LIBDIRS) ${LIBS}
	mkdir -p $(APP_INSTALL_DIR)
	cp service_upnp $(APP_INSTALL_DIR)
	mkdir -p $(RES_INSTALL_DIR)
	cp -a $(CASTLE_TOPDIR)/fast_ui/resource/* $(RES_INSTALL_DIR)

$(MODULE_OBJS):
	cp $(LIBAVF_DIR)/avf_media_client.cpp .
	$(GCC)  $(INCLUDE_DIR) $(DEFINES) $(COMPILE_FLAGS)  -c -MMD -o $@ $(subst .o,.cpp,$(notdir $(subst \,/,$@))) 

service_upnp:
	$(AMBA_MAKEFILE_V)$(MAKE) -C $(PWD)/ClassUpnpService all

thread:
	$(AMBA_MAKEFILE_V)$(MAKE) -C $(PWD)/ClassThread all	 

lib_ipc:
	mkdir -p ipc_lib
	cp $(LIBIPC_DIR)/libipc.c ipc_lib/
	$(GCC) $(INCLUDE_DIR) $(DEFINES) $(COMPILE_FLAGS) -c -MMD -o ipc_lib/libipc.o ipc_lib/libipc.c

ui_lib:
	$(AMBA_MAKEFILE_V)$(MAKE) -C $(PWD)/../fast_ui board=$(board)

xml_cmd:
ifeq ($(xmlcmd),1)
	$(AMBA_MAKEFILE_V)$(MAKE) -C $(PWD)/xml_cmd all
endif

avahi:
ifeq ($(avahi),1)
	$(AMBA_MAKEFILE_V)$(MAKE) -C $(PWD)/avahi all
endif

xmpp_client:
ifeq ($(xmpp),1)
	$(AMBA_MAKEFILE_V)$(MAKE) -C $(PWD)/xmpp_client
endif

scan_OBJS:
	$(AMBA_MAKEFILE_V)$(MAKE) -C $(PWD)/codeScan

clean:
	@echo "  [Clean third-party unit tests]"
	$(RM) ./ClassThread/*.o
	$(RM) ./ClassThread/*.d
	$(RM) ./ClassUpnpService/*.o
	$(RM) ./ClassUpnpService/*.d
	$(RM) ipc_lib
	$(RM) ./*.o
	$(RM) ./*.d
	$(RM) $(CASTLE_TOPDIR)/fast_ui/ui_lib/*.o
	$(RM) $(CASTLE_TOPDIR)/fast_ui/ui_lib/*.d
	$(RM) $(CASTLE_TOPDIR)/fast_ui/camera_app/*.o
	$(RM) $(CASTLE_TOPDIR)/fast_ui/camera_app/*.d
	$(RM) $(CASTLE_TOPDIR)/fast_ui/*.o
	$(RM) $(CASTLE_TOPDIR)/fast_ui/*.d
	$(RM) ./xmpp_client/*.o
	$(RM) ./xmpp_client/*.d
	$(RM) ./codeScan/*.d
	$(RM) ./codeScan/*.o
	$(RM) ./xml_cmd/*.d
	$(RM) ./xml_cmd/*.o
	$(RM) ./avahi/*.d
	$(RM) ./avahi/*.o

