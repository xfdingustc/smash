####################################################################################
#             FILE  NAME  :    Makefile
#             FILE  DESC  :    Makefile	template file
#             AUTHOR      :    wangxutao
#             DATE        :    20090722
####################################################################################

LIB_PATH = . #/usr/local/openssl-1.0.1/lib

LIBS     =   pthread crypto ssl uuid m

INCLUDE_PATH = . /usr/local/include	/usr/local/include/mmseg /usr/local/openssl-1.0.1/include

####################################################################################

CFLAGS = -g -O -Wall  -fPIC 
#CC = gcc
CXX = g++

####################################################################################
DIRS := . 
SOURCE := $(foreach DIRS, $(DIRS),$(wildcard $(DIRS)/*.cpp))
HEADERS := $(foreach DIRS, $(DIRS),$(wildcard $(DIRS)/*.h))

OBJS_1 := $(patsubst ../..%, objs%, $(patsubst %.cpp, %.o, $(SOURCE)))
CORE_OBJS := $(patsubst ..%, objs%, $(patsubst %.cpp, %.o, $(OBJS_1)))
OBJS :=$(patsubst .%, objs%, $(CORE_OBJS))

OUTPUT := ./main

#####################################################################################

objs/%.o: ./%.cpp
	$(CXX) $(CFLAGS)  -Isrc $(addprefix -I, $(INCLUDE_PATH)) -c $< -o $@

all:$ predir $(OUTPUT)

$(OUTPUT): $(OBJS)
	$(CXX) $(CFLAGS) -o $(OUTPUT) $(OBJS) $(addprefix -L,$(LIB_PATH)) $(addprefix -l, $(LIBS)) -ldl -lstdc++
	mv $@  ./bin

predir:
	test -d  'objs/'                        || mkdir -p  'objs/'

#clean obj
######################################################################################
#	rm -rf $(OBJS)

######################################################################################
clean:
	rm -rf objs
	rm -rf ./bin/$(OUTPUT)

