#
#	Copyright (c) 2025, Signaloid.
#
#	Permission is hereby granted, free of charge, to any person obtaining a copy
#	of this software and associated documentation files (the "Software"), to deal
#	in the Software without restriction, including without limitation the rights
#	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#	copies of the Software, and to permit persons to whom the Software is
#	furnished to do so, subject to the following conditions:
#
#	The above copyright notice and this permission notice shall be included in all
#	copies or substantial portions of the Software.
#
#	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#	SOFTWARE.
#

TARGET          = riscv
TARGET-ARCH     = riscv32-unknown-elf
MAKEFILE	= Makefile
#	Start by including config.mk to avoid it overwriting important variables
ifneq ("$(wildcard config.mk)","")
$(info config.mk detected)

include config.mk

ifeq ($(strip $(SOURCES)),)
	SRC_DIRS	= ./

	SOURCES		:= $(shell find $(SRC_DIRS) \( -type f -or -type l \) \( -name "*.c" -o -name "*.C" -o -name "*.cc" -o -name "*.cpp" -o -name "*.CPP" -o -name "*.c++" -o -name "*.cp" -o -name "*.cxx" \))
else
	SOURCES		+= startup.cpp
endif

else
$(info config.mk not provided)
SRC_DIRS	= ./

SOURCES		:= $(shell find $(SRC_DIRS) \( -type f -or -type l \) \( -name "*.c" -o -name "*.C" -o -name "*.cc" -o -name "*.cpp" -o -name "*.CPP" -o -name "*.c++" -o -name "*.cp" -o -name "*.cxx" \))
CXXFLAGS	= -std=c++14
endif

TREEROOT	= $(PATH_TO_LAPLACE_SDK)
include $(TREEROOT)/include/Laplace-SDK.mk

PROGRAM		= main
INIT		= init
STARTUP		= startup

BUILD_DIR	= ./build
INC_DIRS	:= $(PREFIX)/include

OBJS		:= $(INIT).o $(SOURCES:%=$(BUILD_DIR)/%.o)
DEPS		:= $(OBJS:.o=.d)

INC_FLAGS	:= $(addprefix -I,$(INC_DIRS))
OPTFLAGS	?= -O0

WFLAGS		+= -Wall
CWFLAGS         += -Werror=implicit-function-declaration

# WFLAGS goes before pre-existing CFLAGS so that users can overrule them.
CFLAGS		:= $(INC_FLAGS) $(WFLAGS) $(CWFLAGS) $(CFLAGS)   -gdwarf-4 $(OPTFLAGS) -DEMULATOR_STATIC_COMPILATION
CXXFLAGS	:= $(INC_FLAGS) $(WFLAGS)            $(CXXFLAGS) -gdwarf-4 $(OPTFLAGS) -DEMULATOR_STATIC_COMPILATION

LOADADDR	= 0x08004000
LDFLAGS		+= -Ttext $(LOADADDR) -L$(TOOLSLIB)/ -Map $(PROGRAM).map

LIBUXHW		= -luxhw
LIBS		+= -lc $(LIBUXHW) -lstdc++ -lgloss -lgcc -lm

all: Makefile
	make $(PROGRAM) $(PROGRAM).sr onError || { rc=$$?; make onError; exit $$rc; }

$(INIT).o: $(INIT).S Makefile
	$(AS) $(ASFLAGS) $(INIT).S -o $@

$(PROGRAM): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@ $(LWRAP_LIST) --start-group $(LIBS) --end-group 2>uld.output

# c source
$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) $(FNOBUILTIN_FLAGS) -c $< -o $@ 2>>ucc.output

# c++ sources
$(BUILD_DIR)/%.C.o: %.C
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(FNOBUILTIN_FLAGS) -c $< -o $@ 2>>ucc.output

$(BUILD_DIR)/%.cc.o: %.cc
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(FNOBUILTIN_FLAGS) -c $< -o $@ 2>>ucc.output

$(BUILD_DIR)/%.cpp.o: %.cpp
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(FNOBUILTIN_FLAGS) -c $< -o $@ 2>>ucc.output

$(BUILD_DIR)/%.CPP.o: %.CPP
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(FNOBUILTIN_FLAGS) -c $< -o $@ 2>>ucc.output

$(BUILD_DIR)/%.c++.o: %.c++
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(FNOBUILTIN_FLAGS) -c $< -o $@ 2>>ucc.output

$(BUILD_DIR)/%.cp.o: %.cp
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(FNOBUILTIN_FLAGS) -c $< -o $@ 2>>ucc.output

$(BUILD_DIR)/%.cxx.o: %.cxx
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(FNOBUILTIN_FLAGS) -c $< -o $@ 2>>ucc.output

$(PROGRAM).sr: $(PROGRAM)
	$(OBJCOPY) -O srec $(PROGRAM) $@

onError:
	touch uld.output
	sed 's?'$(INC_DIR_1)'??g;s?'$(INC_DIR_2)'??g;s?'$(INC_DIR_3)'??g;s?'$(INC_DIR_4)'??g;s?'$(TREEROOT)'??g;s?'$(TOOLSBIN)'??g;s?'$(PREFIX)/include/'??g;s?'$(CC):\ '??g;s?'`pwd`/'??g' ucc.output 1>&2
	sed 's?'$(UNCERTAIN_LD):\ '??g;s?'`pwd`/'??g' uld.output 1>&2

disassemble:
	$(UNCERTAIN_OBJDUMP) -dSl $(PROGRAM) > $(PROGRAM).uncert.S

run:
	$(SDK_ROOT)/bin/$(OSTYPE)-$(MACHTYPE)/sf --exit-on-error generated-run.m

clean:
	$(RM) -r $(BUILD_DIR) $(PROGRAM) *.o *.out *.s *.sr *.map init.i simlog.node* ucc.output uld.output

-include $(DEPS)

MKDIR_P ?= mkdir -p
