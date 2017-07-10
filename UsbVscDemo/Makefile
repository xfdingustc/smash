
PWD := $(shell pwd)
PROJECT_TOPDIR := $(word 1, $(subst /UsbVscDemo, ,$(PWD)))
CASTLE_TOPDIR := $(PROJECT_TOPDIR)

CC = $(TOOLCHAIN_PATH)/$(CROSS_COMPILE)g++

LIBUSB_INCLUDE = -I$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libusb/usr/include/
LIB_PATH = -L$(CASTLE_TOPDIR)/../prebuild/third-party/armv7-a-hf/libusb/usr/lib/

CFLAGS=-Wall
CFLAGS += $(LIBUSB_INCLUDE)
CFLAGS += $(LIB_PATH)
CFLAGS += -lpthread
LIBUSB_LIBS = usb-1.0

VSC_OBJ = vscPC.o

all: vscPC 

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) 

vscPC: $(VSC_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) -l$(LIBUSB_LIBS)

.PHONY: clean
clean:
	rm *.o 
	rm vscPC 
