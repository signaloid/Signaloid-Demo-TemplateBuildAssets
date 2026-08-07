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

TARGET			= x86
TARGET_ARCH		= x86_64-unknown-linux-gnu
MAKEFILE		= Makefile.pro
M_CONFIG_FILE		?= empty-m-file.m
PATH_TO_UXHW_SDK	?= /opt/Signaloid-UxHw-SDK/

#	Start by including config.mk to avoid it overwriting important variables
ifneq ("$(wildcard config.mk)","")
$(info config.mk detected)

include config.mk

ifeq ($(strip $(SOURCES)),)
	SRC_DIRS	= ./
	SOURCES		:= $(shell find $(SRC_DIRS) \( -type f -or -type l \) \( -name "*.c" -o -name "*.C" -o -name "*.cc" -o -name "*.cpp" -o -name "*.CPP" -o -name "*.c++" -o -name "*.cp" -o -name "*.cxx" \))
endif

else
$(info config.mk not provided)
SRC_DIRS	= ./

SOURCES		:= $(shell find $(SRC_DIRS) \( -type f -or -type l \) \( -name "*.c" -o -name "*.C" -o -name "*.cc" -o -name "*.cpp" -o -name "*.CPP" -o -name "*.c++" -o -name "*.cp" -o -name "*.cxx" \))
CXXFLAGS	= -std=c++14
endif

TREEROOT	= $(PATH_TO_UXHW_SDK)

include $(TREEROOT)/include/UxHw-SDK.mk

PROGRAM		= main
BUILD_DIR	= ./build
INC_DIRS	:= $(TREEROOT)/include $(INC)

CSOURCES 	:= $(filter %.c, $(SOURCES))
CXXSOURCES 	:= $(filter %.cpp, $(SOURCES))
CCSOURCES 	:= $(filter %.cc, $(SOURCES))
CXXSOURCES 	:= $(filter-out ./startup.cpp,$(CXXSOURCES))
CXXSOURCES_C	:= $(filter %.C, $(SOURCES))
CXXSOURCES_CPP  := $(filter %.CPP, $(SOURCES))
CXXSOURCES_CPLUS	:= $(filter %.c++, $(SOURCES))
CXXSOURCES_CP	:= $(filter %.cp, $(SOURCES))
CXXSOURCES_CXX	:= $(filter %.cxx, $(SOURCES))

LLSOURCES	:= $(patsubst %.c,$(BUILD_DIR)/%.c.ll,$(CSOURCES))
LLSOURCES	+= $(patsubst %.cpp,$(BUILD_DIR)/%.cpp.ll,$(CXXSOURCES))
LLSOURCES	+= $(patsubst %.cc,$(BUILD_DIR)/%.cc.ll,$(CCSOURCES))
LLSOURCES	+= $(patsubst %.C,$(BUILD_DIR)/%.C.ll,$(CXXSOURCES_C))
LLSOURCES	+= $(patsubst %.CPP,$(BUILD_DIR)/%.CPP.ll,$(CXXSOURCES_CPP))
LLSOURCES	+= $(patsubst %.c++,$(BUILD_DIR)/%.c++.ll,$(CXXSOURCES_CPLUS))
LLSOURCES	+= $(patsubst %.cp,$(BUILD_DIR)/%.cp.ll,$(CXXSOURCES_CP))
LLSOURCES	+= $(patsubst %.cxx,$(BUILD_DIR)/%.cxx.ll,$(CXXSOURCES_CXX))

OBJS		:= $(BUILD_DIR)/$(PROGRAM)-unc.o

INC_FLAGS	:= $(addprefix -I,$(INC_DIRS))
OPTFLAGS	?= -O0 -gdwarf-4
CFLAGS		+= $(INC_FLAGS) -Wall -Wno-sometimes-uninitialized -gdwarf-4 $(OPTFLAGS) --target=$(TARGET_ARCH)

LLVM_REDUCE	= $(LLVM_INSTALL)/bin/llvm-reduce

# The Makefile will build the recipes below and then filter and output the build logs
# to stderr to be consumed by others, e.g., the SCCE backend. On error it will only filter and output to stderr.
all: Makefile.pro
	@echo "[INFO] Starting build with SDK: "$(shell cat $(PATH_TO_UXHW_SDK)/.sdk_installed_release)
	make --makefile=Makefile.pro $(PROGRAM) filterAndOutput || { rc=$$?; make --makefile=Makefile.pro onError; make --makefile=Makefile.pro anonymizeDisabled >/dev/null 2>&1 || true; exit $$rc; }
	@make --makefile=Makefile.pro anonymizeDisabled >/dev/null 2>&1 || true

# Link with libraries and libuxhwrt.
$(PROGRAM): $(OBJS)
	$(UNC_LD) $(LDFLAGS) $(OBJS) -o $@ $(LIBS) 2>uld.output

# C source

# Convert to LLVM IR
$(BUILD_DIR)/%.c.ll: %.c
	$(MKDIR_P) $(dir $@)
	$(CLANG) $(CLANGFLAGS) $(CFLAGS) -c $< -o $@ 2>>ucc.output

$(BUILD_DIR)/%.cpp.ll: %.cpp
	$(MKDIR_P) $(dir $@)
	$(CLANG)++ $(CLANG_CXX_FLAGS) $(OPTFLAGS) $(CXXFLAGS) $(INC_FLAGS) -c $< -o $@ 2>>ucc.output

$(BUILD_DIR)/%.cc.ll: %.cc
	$(MKDIR_P) $(dir $@)
	$(CLANG)++ $(CLANG_CXX_FLAGS) $(OPTFLAGS) $(CXXFLAGS) $(INC_FLAGS) -c $< -o $@ 2>>ucc.output

# Link all LLVM IR to a single LLVM IR file. Link with UxHw SDK runtime library bitcode files as well.
$(BUILD_DIR)/$(PROGRAM)-link.ll: $(LLSOURCES) $(UXHW_SDK_RUNTIME_BC)
	$(LLVM-LINK) -S $^ -o $@

# Pass combined LLVM IR file through the Uncertainty Optimisation
$(BUILD_DIR)/$(PROGRAM)-unc.bc: $(BUILD_DIR)/$(PROGRAM)-link.ll
	$(OPT) $(LLVM_OPT_FLAGS) < $< > $@ 2>opt.err

# Compile LLVM IR to object file
$(BUILD_DIR)/$(PROGRAM)-unc.o: $(BUILD_DIR)/$(PROGRAM)-unc.bc
	$(LLC) $(LLCFLAGS) $< -o $@

# Test here means the test that llvm-reduce applies to assess whether a part of the code is important.
$(BUILD_DIR)/$(PROGRAM)-reduced-anon.ll: $(BUILD_DIR)/$(PROGRAM)-link.ll
	@echo '#!/bin/bash' > $(BUILD_DIR)/llvm-reduce-test.sh
	@echo '! $(OPT) $(LLVM_OPT_FLAGS) "$$1" 2>/dev/null' >> $(BUILD_DIR)/llvm-reduce-test.sh
	@chmod +x $(BUILD_DIR)/llvm-reduce-test.sh
	$(LLVM_REDUCE) --test=$(BUILD_DIR)/llvm-reduce-test.sh $< -o $@

reduceAndAnonymize: $(BUILD_DIR)/$(PROGRAM)-reduced-anon.ll
	# 2. Rename symbols
	$(OPT) -passes="metarenamer,strip-dead-prototypes" -S $(BUILD_DIR)/$(PROGRAM)-reduced-anon.ll -o $(BUILD_DIR)/$(PROGRAM)-reduced-renamed.ll
	# 3. Strip
	$(OPT) --strip-debug -S $(BUILD_DIR)/$(PROGRAM)-reduced-renamed.ll -o $(BUILD_DIR)/$(PROGRAM)-reduced-anon-final.ll

anonymize:
	# 2. Rename symbols
	$(OPT) -passes="metarenamer,strip-dead-prototypes" -S $(BUILD_DIR)/$(PROGRAM)-link.ll -o $(BUILD_DIR)/$(PROGRAM)-renamed.ll
	# 3. Strip
	$(OPT) --strip-debug -S $(BUILD_DIR)/$(PROGRAM)-renamed.ll -o $(BUILD_DIR)/$(PROGRAM)-anon-final.ll

anonymizeDisabled:
	@true

filterAndOutput:
	touch uld.output
	sed 's?'$(TREEROOT)'??g;s?'$(PREFIX)/include/'??g;s?'$(CLANG):\ '??g;s?'`pwd`/'??g;s?/opt/musl-signaloid/??g' ucc.output 1>&2
	sed 's?'$(CLANG):\ '??g;s?'`pwd`/'??g;s?/opt/musl-signaloid/??g' uld.output 1>&2

onError: filterAndOutput

clean:
	$(RM) -r $(BUILD_DIR) $(PROGRAM) *.o *.ll *.bc *.out ucc.output uld.output

-include $(DEPS)

MKDIR_P ?= mkdir -p
