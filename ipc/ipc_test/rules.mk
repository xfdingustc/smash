
######################################################################
#		tools
######################################################################
GCC    = $(TOOLCHAIN_PATH)/$(CROSS_COMPILE)gcc
CPP    = $(TOOLCHAIN_PATH)/$(CROSS_COMPILE)g++
LD     = $(TOOLCHAIN_PATH)/$(CROSS_COMPILE)ld
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

COMPILE_FLAGS += -march=armv6k -mtune=arm1136j-s -msoft-float -mlittle-endian
COMPILE_FLAGS += -fno-rtti -fno-exceptions -fstrict-aliasing -Wstrict-aliasing
COMPILE_FLAGS += -O3 -DNDEBUG

COMPILE_LIB_OBJS = $(addprefix $(MODULE_DIR)/out/, $(LIB_OBJS))
COMPILE_EXE_OBJS = $(addprefix $(MODULE_DIR)/out/, $(EXE_OBJS))

ifneq ($(strip $(LIB_NAME)),)
LIB_OUT = $(addprefix $(MODULE_DIR)/out/lib, $(LIB_NAME)).a
else
LIB_OUT =
endif
EXE_OUT = $(addprefix $(MODULE_DIR)/out/, $(EXE_NAME))

######################################################################
#		targets
######################################################################
default: all
.PHONY: all depend clean

all: $(LIB_OUT) $(EXE_OUT)

clean:
	$(RM) $(MODULE_DIR)/out

ifneq ($(strip $(LIB_NAME)),)
$(LIB_OUT): $(COMPILE_LIB_OBJS)
	@echo Building lib $(LIB_OUT)...
	@$(AR) rucs $(LIB_OUT) $(COMPILE_LIB_OBJS)
endif

ifneq ($(strip $(EXE_OUT)),)
$(EXE_OUT): $(COMPILE_EXE_OBJS) $(LIB_OUT)
	@echo Building exe $(EXE_OUT)...
	@$(CPP) -o $@ $(COMPILE_EXE_OBJS) $(EXE_LDFLAGS)
endif

######################################################################
#   rules
######################################################################
$(MODULE_DIR)/out/%.o: %.c
	@echo   compiling $< ...
	@$(MKDIR) $(dir $@)
	@$(CPP) $(LIB_INCLUDES) $(LIB_DEFINES) $(COMPILE_FLAGS) $(LIB_CFLAGS) $(LIB_CPPFLAGS) -c -MMD -o $@ $<

-include $(COMPILE_LIB_OBJS:.o=.d)

