

default: all
.PHONY: all depend clean

GCC    := $(TOOLCHAIN_PATH)/$(CROSS_COMPILE)gcc
CPP    := $(TOOLCHAIN_PATH)/$(CROSS_COMPILE)g++
CFLAGS := -march=armv6k -mtune=arm1136j-s -msoft-float -mlittle-endian -fno-rtti -fno-exceptions -fstrict-aliasing -Wstrict-aliasing -O3 -DNDEBUG
MODULE_DIR	:= $(shell pwd)
LINK_FLAGS := -lpthread

all:
	@echo making test_server
	@$(CPP) $(CFLAGS) -o test_server test_server.c $(LINK_FLAGS)
	@echo making test_client
	@$(CPP) $(CFLAGS) -o test_client test_client.c $(LINK_FLAGS)

clean:
	rm -f test_server test_client

