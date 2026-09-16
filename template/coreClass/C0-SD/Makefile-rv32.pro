#
#	Copyright (c) 2026, Signaloid.
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

PROGRAM             ?= main
COMMON              ?= .


# Target specific variables
DEVICE_TYPE         := SIGNALOID_C0_SD
ENABLE_RV32_SUBSET  := IMFC

BUILD_FLAGS         += -DC0_SD_BUILD

# LD_SCRIPT and LOADADDR are mode-dependent (lram | psram | xip). They are set after
# the config.mk include below (where MODE is known), from assets baked by common-pro.


# Target agnostic
TARGET              := riscv32
TARGET_ARCH         := riscv32-unknown-elf
MAKEFILE            := Makefile-rv32.pro

BUILD_TIMESTAMP     := "$(shell date '+%Y%m%d-%H%M%S')"
UXHW_SDK_VERSION    := $(shell cat $(PATH_TO_UXHW_SDK)/.sdk_installed_release)

# Start by including config.mk to avoid it overwriting important variables
ifneq ("$(wildcard config.mk)","")
        $(info config.mk detected)
        include config.mk
endif

# Execution mode (lram | psram | xip), supplied by config.mk. Selects the
# pre-built, self-contained linker script and its matching load address -- both
# baked from the regmaps by common-pro and copied in alongside this Makefile.
MODE                ?= psram
ifeq ($(filter $(MODE),lram psram xip),)
        $(error MODE must be one of `lram`, `psram`, `xip` for C0-SD, got `$(MODE)`)
endif
LD_SCRIPT           := $(COMMON)/$(MODE).ld
include $(COMMON)/loadaddr.mk
LOADADDR            := $(LOADADDR_$(MODE))

ifeq ($(strip $(SOURCES)),)
        $(info SOURCES variable is empty. Autodetecting sources in root.)
        SRC_DIRS    := ./
        SOURCES     := $(shell find $(SRC_DIRS) \( -type f -or -type l \) \( -name "*.c" -o -name "*.C" -o -name "*.cc" -o -name "*.cpp" -o -name "*.CPP" -o -name "*.c++" -o -name "*.cp" -o -name "*.cxx" \))
endif

TREEROOT            := $(PATH_TO_UXHW_SDK)
ABS_TREEROOT        := $(shell realpath $(TREEROOT))

# Get definitions for UxHw SDK installation
include $(TREEROOT)/include/UxHw-SDK.mk

BUILD_DIR           ?= .

INC_DIRS            += $(INC)

CSOURCES            := $(filter %.c, $(SOURCES))
CXXSOURCES          := $(filter %.cpp, $(SOURCES))
CCSOURCES           := $(filter %.cc, $(SOURCES))
CXXSOURCES          := $(filter-out ./startup.cpp,$(CXXSOURCES))
# sbrk.c is compiled separately as $(SBRK).o; exclude it from autodetected
# sources so it is not also pulled into the uncertainty IR (duplicate _sbrk).
CSOURCES            := $(filter-out ./sbrk.c,$(CSOURCES))
# cache.c / dma.c / dma_memops.c are the pre-baked SoC drivers, compiled
# separately as $(DRIVER_OBJS) below. Excluded here for the same reason as
# sbrk.c, plus one more: they are hardware drivers, volatile MMIO and inline asm
# with no uncertainty anywhere in them, so the uncertainty IR pipeline has
# nothing to do to them. dma_memops.c must not reach it at all -- its
# __real_memcpy / __real_memset come from the linker's --wrap, not from any
# translation unit, so autodetecting it into a build without the wrap flags
# fails the link on an undefined symbol.
CSOURCES            := $(filter-out ./cache.c ./dma.c ./dma_memops.c,$(CSOURCES))
CXXSOURCES_C        := $(filter %.C, $(SOURCES))
CXXSOURCES_CPP      := $(filter %.CPP, $(SOURCES))
CXXSOURCES_CPLUS    := $(filter %.c++, $(SOURCES))
CXXSOURCES_CP       := $(filter %.cp, $(SOURCES))
CXXSOURCES_CXX      := $(filter %.cxx, $(SOURCES))

ONNXSOURCES         := $(filter %.onnx, $(SOURCES))

LLSOURCES           := $(patsubst %.c,$(BUILD_DIR)/%.c.ll,$(CSOURCES))
LLSOURCES           += $(patsubst %.cpp,$(BUILD_DIR)/%.cpp.ll,$(CXXSOURCES))
LLSOURCES           += $(patsubst %.cc,$(BUILD_DIR)/%.cc.ll,$(CCSOURCES))
LLSOURCES           += $(patsubst %.C,$(BUILD_DIR)/%.C.ll,$(CXXSOURCES_C))
LLSOURCES           += $(patsubst %.CPP,$(BUILD_DIR)/%.CPP.ll,$(CXXSOURCES_CPP))
LLSOURCES           += $(patsubst %.c++,$(BUILD_DIR)/%.c++.ll,$(CXXSOURCES_CPLUS))
LLSOURCES           += $(patsubst %.cp,$(BUILD_DIR)/%.cp.ll,$(CXXSOURCES_CP))
LLSOURCES           += $(patsubst %.cxx,$(BUILD_DIR)/%.cxx.ll,$(CXXSOURCES_CXX))
LLSOURCES           += $(patsubst %.onnx,$(BUILD_DIR)/%.onnx.ll,$(ONNXSOURCES))

# Pre-baked, self-contained boot assembly (from common-pro): the init-pro boot
# stub and the trap_vector handler, as SEPARATE objects. Add more .S here if the
# boot set grows; each is assembled by the static pattern rule below.
ASM_SRCS            := $(COMMON)/init-pro.S $(COMMON)/trap_vector.S
ASM_OBJS            := $(ASM_SRCS:.S=.o)
STARTUP             := $(COMMON)/startup
SBRK                := $(COMMON)/sbrk

# Pre-baked, self-contained SoC drivers (from common-pro): RV32RX cache
# maintenance (cache.c, cache.h) and the AXIL PIB DMA engine driver (dma.c,
# dma.h -- dma.h carries the engine's hardware contract, read it before driving
# the engine yourself). Linked into every application, like the boot assembly:
# the data cache is write-through, so cache maintenance is only ever needed to
# invalidate before reading memory the DMA or the SD host wrote.
DRIVER_SRCS         := $(COMMON)/cache.c $(COMMON)/dma.c

# DMA-backed memcpy / memset (dma_memops.c). Set DMA_MEMOPS=1 in config.mk to
# route both through the engine; DMA_MEMOPS_THRESHOLD is the length below which
# the wrapper calls the C library instead.
#
# OFF by default and opt-in per application: it turns CPU single-beat stores
# into 512-byte bursts, which is measurable in SD-side latency, and it shares
# the single DMA engine with the application.
#
# The wrap flags and the object must stay together: the object alone leaves
# __real_memcpy undefined, the flags alone leave __wrap_memcpy undefined.
DMA_MEMOPS          ?= 0
DMA_MEMOPS_THRESHOLD ?= 128

ifeq ($(filter $(DMA_MEMOPS),0 1),)
        $(error DMA_MEMOPS must be 0 or 1, got `$(DMA_MEMOPS)`)
endif

ifeq ($(DMA_MEMOPS),1)
DRIVER_SRCS         += $(COMMON)/dma_memops.c
DMA_MEMOPS_LDFLAGS  := --wrap=memcpy --wrap=memset
endif

DRIVER_OBJS         := $(DRIVER_SRCS:.c=.o)
OBJS                := $(ASM_OBJS) $(STARTUP).o $(SBRK).o $(DRIVER_OBJS) $(BUILD_DIR)/$(PROGRAM)-unc.o

INC_FLAGS           := $(addprefix -I,$(INC_DIRS))

# Use this to help customers avoid double-precision emulation functions.
# Tried -fsingle-precision-constant, but it is not supported in Clang.
SINGLE_PRECISION_CFLAGS ?= -Wdouble-promotion -Wfloat-conversion

OPTFLAGS            ?= -Os -fno-vectorize -fno-slp-vectorize -gdwarf-4

BUILD_FLAGS         += -DBUILD_FOR=$(DEVICE_TYPE)
BUILD_FLAGS         += -DBUILD_TIMESTAMP=\"$(BUILD_TIMESTAMP)\"
BUILD_FLAGS         += -DUXHW_SDK_VERSION=\"$(UXHW_SDK_VERSION)\"

CFLAGS              += $(INC_FLAGS)
CFLAGS              += -Wall
CFLAGS              += -Wno-sometimes-uninitialized
CFLAGS              += -gdwarf-4
CFLAGS              += $(SINGLE_PRECISION_CFLAGS)
CFLAGS              += $(OPTFLAGS)
CFLAGS              += --target=$(TARGET_ARCH)
CFLAGS              += $(BUILD_FLAGS)

CXXFLAGS            += -std=c++14
CXXFLAGS            += $(BUILD_FLAGS)

ifneq ($(strip $(ONNXSOURCES)),)
EXTRA_LIBS += -lcruntime
endif

# -z max-page-size=4 keeps the linker from page-padding a load segment's file
# offset to match its VMA. With a flash LMA and a RAM VMA (every mode's copy
# region) the default 4 KiB page would insert up to 4 KiB of padding into the
# raw image, desynchronising the .data image from LOADADDR(.data) -- the address
# init-pro hands to the DMA. Harmless for a bare-metal image with no MMU.
LDFLAGS             += -Ttext $(LOADADDR) -T$(LD_SCRIPT) --no-relax -z max-page-size=4 -Map $(PROGRAM).map
LDFLAGS             += $(DMA_MEMOPS_LDFLAGS)

ASFLAGS             := --arch=$(TARGET) --mattr=$(MATTR) --filetype=obj

STRICT_ALIGN_MATTR  ?= -unaligned-scalar-mem
LLCFLAGS            += -mattr=$(STRICT_ALIGN_MATTR)


# The Makefile will build the recipes below and then filter and output the build logs
# to stderr to be consumed by others, e.g., the SCCE backend. On error it will only filter and output to stderr.
all: $(MAKEFILE)
	@echo "[INFO] Starting build with SDK: "$(UXHW_SDK_VERSION)
	make --makefile=$(MAKEFILE) $(PROGRAM).sr $(PROGRAM).bin filterAndOutput || { rc=$$?; make --makefile=$(MAKEFILE) onError; make --makefile=$(MAKEFILE) anonymizeDisabled >/dev/null 2>&1 || true; exit $$rc; }
	@make --makefile=$(MAKEFILE) anonymizeDisabled >/dev/null 2>&1 || true

# Link with libraries and libuxhw_runtime.
# Note: Using &> for LLVM-OBJDUMP does not seem to work.
$(PROGRAM): $(OBJS)
	$(UNC_LD) $(LDFLAGS) $(OBJS) -o $@ $(LIBS) 2>uld.output
	$(LLVM-STRIP) --strip-all $(PROGRAM)
	$(LLVM-OBJCOPY) --add-section .sdk_release=$(ABS_TREEROOT)/.sdk_installed_release $@

$(PROGRAM).sr: $(PROGRAM)
	$(OBJCOPY) -O srec $< $@

# Assemble each pre-baked boot .S (init-pro.S, trap_vector.S) to its own object.
# Static pattern rule: scoped to $(ASM_OBJS), so it never shadows the C/C++
# recipes or make's built-in .S rules.
$(ASM_OBJS): $(COMMON)/%.o: $(COMMON)/%.S
	$(LLVM-MC) $(ASFLAGS) $< -o $@

$(STARTUP).o: $(STARTUP).cpp
	$(CLANG)++ --target=$(TARGET-TRIPLE) -march=$(MARCH) $(TARGET_CXX_INCLUDES) $(OPTFLAGS) -c $< -o $@

$(SBRK).o: $(SBRK).c
	$(CLANG) --target=$(TARGET-TRIPLE) -march=$(MARCH) $(OPTFLAGS) -c $< -o $@

# Compile each pre-baked driver .c to its own object, like $(SBRK).o and NOT
# through the uncertainty IR pipeline. Static pattern rule, so it is scoped to
# $(DRIVER_OBJS) and never shadows the %.c.ll recipe below.
$(DRIVER_OBJS): $(COMMON)/%.o: $(COMMON)/%.c
	$(CLANG) --target=$(TARGET-TRIPLE) -march=$(MARCH) $(OPTFLAGS) $(MEMOPS_CFLAGS) -c $< -o $@

# dma_memops.c alone: -fno-builtin-mem* stops the compiler rewriting the
# wrapper's own C-library fallback calls into calls to the symbols the wrapper
# implements, whose failure mode is unbounded recursion at run time.
$(COMMON)/dma_memops.o: MEMOPS_CFLAGS := -DDMA_MEMOPS_THRESHOLD=$(DMA_MEMOPS_THRESHOLD) \
                        -fno-builtin-memcpy -fno-builtin-memset

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

# Generate the LLVM IR from the MLIR using mlir-translate
$(BUILD_DIR)/%.onnx.ll: $(BUILD_DIR)/%.onnx.mlir
	$(MKDIR_P) $(dir $@)
	$(LLVM_INSTALL)/bin/mlir-translate --mlir-to-llvmir --opaque-pointers $< > $@.tmp 2>>ucc.output
	sed 's/@run_main_graph/@run_main_graph_$*/g' $@.tmp > $@ 2>>ucc.output

# Generate the MLIR from the ONNX using onnx-mlir
$(BUILD_DIR)/%.onnx.mlir: %.onnx
	$(MKDIR_P) $(dir $@)
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):$(LLVM_INSTALL)/lib \
	$(ONNX_MLIR) --EmitLLVMIR -O2 --mtriple=$(TARGET_ARCH) $< -o $(BUILD_DIR)/$* 2>>ucc.output

# Link all LLVM IR to a single LLVM IR file. Link with UxHw runtime library bitcode files as well.
$(BUILD_DIR)/$(PROGRAM)-link.ll: $(LLSOURCES) $(UXHW_SDK_RUNTIME_BC)
	$(LLVM-LINK) --only-needed -S $^ -o $@

# Pass combined LLVM IR file through the Uncertainty Optimisation
$(BUILD_DIR)/$(PROGRAM)-unc.bc: $(BUILD_DIR)/$(PROGRAM)-link.ll
	$(OPT) $(LLVM_OPT_FLAGS) < $< > $@ 2>opt.err

# Compile LLVM IR to object file
$(BUILD_DIR)/$(PROGRAM)-unc.o: $(BUILD_DIR)/$(PROGRAM)-unc.bc
	$(LLC) $(CLANG_LLC_FLAGS) $(LLCFLAGS) $< -o $@

$(PROGRAM).bin: $(PROGRAM)
	$(LLVM-OBJCOPY) -O binary $(PROGRAM) $@

# Test here means the test that llvm-reduce applies to assess whether a part of the code is important.
$(BUILD_DIR)/$(PROGRAM)-reduced-anon.ll: $(BUILD_DIR)/$(PROGRAM)-link.ll
	@echo '#!/bin/bash' > $(BUILD_DIR)/llvm-reduce-test.sh
	@echo '! $(OPT) $(LLVM_OPT_FLAGS) "$$1" 2>/dev/null' >> $(BUILD_DIR)/llvm-reduce-test.sh
	@chmod +x $(BUILD_DIR)/llvm-reduce-test.sh
	$(LLVM_REDUCE) --test=$(BUILD_DIR)/llvm-reduce-test.sh $< -o $@

disassemble: $(PROGRAM)
	$(LLVM-OBJDUMP) -dSl $(PROGRAM) 2>&1 1>$(PROGRAM).uncert.S

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
	$(RM) $(PROGRAM) $(BUILD_DIR)/*.o $(BUILD_DIR)/*.ll $(BUILD_DIR)/*.bc *.out *.sr *.map *.bin uld.output opt.err $(OBJS)

-include $(DEPS)

MKDIR_P ?= mkdir -p
