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
PROJECT_TOPDIR := $(word 1, $(subst /camera_app, ,$(PWD)))
CASTLE_TOPDIR := $(PROJECT_TOPDIR)

APP_INSTALL_DIR	:= $(CASTLE_TOPDIR)/../ambarella/rootfs/packages/ts/service_upnp/usr/local/bin/
ifeq ($(board),hd22a)
	APP_INSTALL_DIR	:= $(CASTLE_TOPDIR)/../ambarella/boards/tsf_hd22a/rootfs/default/usr/local/bin/
else ifeq ($(board),hd23a)
	APP_INSTALL_DIR	:= $(CASTLE_TOPDIR)/../ambarella/boards/tsf_hd23a/rootfs/default/usr/local/bin/
else ifeq ($(board),diablo)
	APP_INSTALL_DIR	:= $(CASTLE_TOPDIR)/../ambarella/boards/agb_diablo/rootfs/default/usr/local/bin/
else ifeq ($(board),hellfire)
	APP_INSTALL_DIR	:= $(CASTLE_TOPDIR)/../ambarella/boards/agb_hellfire/rootfs/default/usr/local/bin/
else ifeq ($(board),hachi)
	APP_INSTALL_DIR	:= $(CASTLE_TOPDIR)/../ambarella/boards/tw_hachi/rootfs/default/usr/local/bin/
endif

RES_INSTALL_DIR := $(CASTLE_TOPDIR)/../ambarella/rootfs/packages/ts/service_upnp/usr/local/share/ui/
ifeq ($(board),hellfire)
	RES_INSTALL_DIR	:= $(CASTLE_TOPDIR)/../ambarella/boards/agb_hellfire/rootfs/default/usr/local/share/ui/
else ifeq ($(board),hachi)
	RES_INSTALL_DIR	:= $(CASTLE_TOPDIR)/../ambarella/boards/tw_hachi/rootfs/default/usr/local/share/ui/
endif

RES_DIR := $(CASTLE_TOPDIR)/camera_app/fast_ui/resource
UI_DIR := fast_ui
ifeq ($(board),hachi)
	RES_DIR := $(CASTLE_TOPDIR)/camera_app/cairo_ui/resource
	UI_DIR := cairo_ui
endif

MODULE_SRCS = $(wildcard *.cpp)

include rules.mk

MODULE_SRCS = \
	AudioPlayer.cpp \
	service_upnp.cpp \
	upnp_sdcc.cpp \
	class_ipc_rec.cpp \
	Class_Upgrader.cpp \
	Class_DeviceManager.cpp

ifeq ($(board),hachi)
MODULE_SRCS += SensorManager.cpp
endif

ifeq ($(avf),1)
MODULE_SRCS += avf_media_client.cpp
else
MODULE_SRCS += ipc_vlc_client.cpp
endif


MODULE_OBJS = $(MODULE_SRCS:%.cpp=%.o)

THREAD_OBJS = $(wildcard ClassThread/*.o)
LIBIPC_OBJS = $(wildcard ipc_lib/*.o)

UILIB_OBJS = $(wildcard $(PWD)/fast_ui/ui_lib/*.o)
CAMERA_OBJS = $(wildcard $(PWD)/fast_ui/app/*.o)
UI_OBJS = $(wildcard $(PWD)/fast_ui/*.o)
ifeq ($(board),hachi)
	UILIB_OBJS = $(wildcard $(PWD)/cairo_ui/ui_lib/*.o)
	CAMERA_OBJS = $(wildcard $(PWD)/cairo_ui/app/*.o)
	UI_OBJS = $(wildcard $(PWD)/cairo_ui/*.o)
endif

ifeq ($(avahi), 1)
AVAHI_OBJS = $(wildcard $(CASTLE_TOPDIR)/camera_app/avahi/*.o)
endif

ifeq ($(xmlcmd),1)
XMLCMD_OBJS = $(wildcard $(CASTLE_TOPDIR)/camera_app/xml_cmd/*.o)
endif

ifeq ($(youkucam),1)
YK_CAM_OBJS = $(wildcard $(CASTLE_TOPDIR)/camera_app/ykcamera_sdk/*.o)
endif

ifeq ($(board),hachi)
UPLOAD_OBJS = $(wildcard $(CASTLE_TOPDIR)/camera_app/upload_service/*.o)
endif

INCLUDE_DIR += -I$(CASTLE_TOPDIR)/camera_app/ \
	-I$(CASTLE_TOPDIR)/camera_app/ClassThread/ \
	-I$(CASTLE_TOPDIR)/camera_app/avahi/ \
	-I$(CASTLE_TOPDIR)/camera_app/xml_cmd/ \
	-I$(LIBIPC_DIR)/ \
	-I$(LIBAVF_DIR)/ \
	-I$(CASTLE_TOPDIR)/camera_app/$(UI_DIR)/ui_lib/ \
	-I$(CASTLE_TOPDIR)/camera_app/$(UI_DIR)/app/ \
	-I$(CASTLE_TOPDIR)/camera_app/$(UI_DIR)


ifeq ($(board),hellfire)
	INCLUDE_DIR += -I$(CASTLE_TOPDIR)/../ambarella/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/transee/armv7-a-hf/agbox/usr/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libqrencode/usr/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libxml2/usr/include/libxml2/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/alsa/alsa-lib/usr/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/openssl/usr/include/
else ifeq ($(board),hachi)
	INCLUDE_DIR += -I$(CASTLE_TOPDIR)/../ambarella/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/transee/armv7-a-hf/agbox/usr/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libqrencode/usr/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libxml2/usr/include/libxml2/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/alsa/alsa-lib/usr/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/openssl/usr/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/freetype/usr/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/freetype/usr/include/freetype2/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/cairo/usr/include/cairo/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/curl/usr/include/ \
		-I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/openssl/usr/include/
else
	INCLUDE_DIR += -I$(CASTLE_TOPDIR)/../ambarella/build/include/ \
		-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/agbox/usr/include/ \
		-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libqrencode/usr/include/ \
		-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libxml2/usr/include/libxml2/ \
		-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/alsa/alsa-lib/usr/include/ \
		-I$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/openssl/usr/include/
endif

ifeq ($(youkucam),1)
	INCLUDE_DIR += -I$(CASTLE_TOPDIR)/camera_app/ykcamera_sdk \
		-I$(CASTLE_TOPDIR)/camera_app/ykcamera_sdk/include
endif


LIBS =  -lstdc++ -lc -lrt -ldl -lm -lpthread -lagbox \
		-lfreetype -lqrencode -lz -lssl -lcrypto -lasound -lavahi-core -lavahi-common
ifeq ($(board),hachi)
	LIBS += -lpng -lpixman-1 -lcairo
endif
ifeq ($(youkucam),1)
	LIBS += -lykcamera
endif
ifeq ($(board),hachi)
	LIBS += -lcurl -lssl
endif

ifeq ($(xmlcmd),1)
LIBS += -lxml2
endif


ifeq ($(board),hellfire)
LIBDIRS = -L$(CASTLE_TOPDIR)/../prebuild/transee/armv7-a-hf/agbox/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/freetype/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libqrencode/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libpng/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/zlib/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/openssl/usr/lib \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libxml2/usr/lib \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libjpeg/usr/lib \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv6k/libzxing/usr/lib \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/alsa/alsa-lib/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/avahi/usr/lib/
else ifeq ($(board),hachi)
LIBDIRS = -L$(CASTLE_TOPDIR)/../prebuild/transee/armv7-a-hf/agbox/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/freetype/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libqrencode/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libpng/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/zlib/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/openssl/usr/lib \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libxml2/usr/lib \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libjpeg/usr/lib \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv6k/libzxing/usr/lib \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/alsa/alsa-lib/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/avahi/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/cairo/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/pixman/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/curl/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/openssl/usr/lib/
else
LIBDIRS = -L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/agbox/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/freetype/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libqrencode/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libpng/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/zlib/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/openssl/usr/lib \
	-L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libxml2/usr/lib \
	-L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libjpeg/usr/lib \
	-L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/libzxing/usr/lib \
	-L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/alsa/alsa-lib/usr/lib/ \
	-L$(CASTLE_TOPDIR)/../ambarella/rootfs/packages/avahi/usr/lib/
endif
ifeq ($(youkucam),1)
	LIBDIRS += -L$(CASTLE_TOPDIR)/camera_app/ykcamera_sdk/lib
endif


UNIT_TEST_LDFLAG	?= $(AMBARELLA_LDFLAGS)

export CASTLE_TOPDIR

.PHONY: all clean thread lib_ipc ui_lib xml_cmd avahi yk_cam upload_service

all : $(MODULE_OBJS) thread lib_ipc ui_lib xml_cmd avahi yk_cam upload_service
	@$(LD) $(LD_FLAGS) -o service_upnp $(MODULE_OBJS) $(THREAD_OBJS) $(LIBIPC_OBJS) $(UILIB_OBJS) $(UI_OBJS) $(CAMERA_OBJS) $(XMLCMD_OBJS) $(AVAHI_OBJS) $(YK_CAM_OBJS) $(UPLOAD_OBJS) $(LIBDIRS) ${LIBS}
	mkdir -p $(APP_INSTALL_DIR)
	@echo "[  making service_upnp successfully  ]"
	cp service_upnp $(APP_INSTALL_DIR)
	mkdir -p $(RES_INSTALL_DIR)
	cp -a $(RES_DIR)/* $(RES_INSTALL_DIR)

$(MODULE_OBJS):
	@cp $(LIBAVF_DIR)/avf_media_client.cpp .
	@$(GCC)  $(INCLUDE_DIR) $(DEFINES) $(COMPILE_FLAGS)  -c -MMD -o $@ $(subst .o,.cpp,$(notdir $(subst \,/,$@)))

thread:
	$(AMBA_MAKEFILE_V)$(MAKE) -C $(PWD)/ClassThread all

lib_ipc:
	mkdir -p ipc_lib
	@cp $(LIBIPC_DIR)/libipc.c ipc_lib/
	@$(GCC) $(INCLUDE_DIR) $(DEFINES) $(COMPILE_FLAGS) -c -MMD -o ipc_lib/libipc.o ipc_lib/libipc.c

ui_lib:
	$(AMBA_MAKEFILE_V)$(MAKE) -C $(PWD)/$(UI_DIR) board=$(board)

xml_cmd:
ifeq ($(xmlcmd),1)
	$(AMBA_MAKEFILE_V)$(MAKE) -C $(PWD)/xml_cmd all
endif

avahi:
ifeq ($(avahi),1)
	$(AMBA_MAKEFILE_V)$(MAKE) -C $(PWD)/avahi all
endif

yk_cam:
ifeq ($(youkucam),1)
	$(AMBA_MAKEFILE_V)$(MAKE) -C $(PWD)/ykcamera_sdk all
endif

upload_service:
ifeq ($(board),hachi)
	$(AMBA_MAKEFILE_V)$(MAKE) -C $(PWD)/upload_service all
endif

clean:
	@echo "  [Clean third-party unit tests]"
	$(RM) ./ClassThread/*.o
	$(RM) ./ClassThread/*.d
	$(RM) ipc_lib
	$(RM) ./*.o
	$(RM) ./*.d
	$(RM) ./fast_ui/ui_lib/*.o
	$(RM) ./fast_ui/ui_lib/*.d
	$(RM) ./fast_ui/app/*.o
	$(RM) ./fast_ui/app/*.d
	$(RM) ./fast_ui/*.o
	$(RM) ./fast_ui/*.d
	$(RM) ./cairo_ui/ui_lib/*.o
	$(RM) ./cairo_ui/ui_lib/*.d
	$(RM) ./cairo_ui/app/*.o
	$(RM) ./cairo_ui/app/*.d
	$(RM) ./cairo_ui/*.o
	$(RM) ./cairo_ui/*.d
	$(RM) ./xml_cmd/*.d
	$(RM) ./xml_cmd/*.o
	$(RM) ./avahi/*.d
	$(RM) ./avahi/*.o
	$(RM) ./ykcamera_sdk/*.d
	$(RM) ./ykcamera_sdk/*.o
	$(RM) ./upload_service/*.d
	$(RM) ./upload_service/*.o

