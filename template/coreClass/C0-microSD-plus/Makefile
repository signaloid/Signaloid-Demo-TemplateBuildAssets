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

PROGRAM				= main
TOOLKIT				= <path-to-C0_SD_toolkit>
DEVICE				?= <path-to-device>
PYTHON				= python3
PATH_TO_UXHW_SDK 		= <path-to-uxhw-sdk>
REPRESENTATION_TYPE		= Athens
REPRESENTATION_SIZE		?= 16
CORRELATION_TRACKING		= CORRELATION_OFF
UNCERTAIN_MEMORY_POOL_SIZE	= 40
UNCERTAIN_MEMORY_SIZE		= 400
M_CONFIG_FILE			= run.m
ENABLE_RV32_SUBSET		= IMFC

SUDO 				:= $(shell OWNER=$$(stat -c '%U' $(DEVICE) 2>/dev/null || stat -f '%Su' $(DEVICE) 2>/dev/null); [ "$$OWNER" = "root" ] && echo "sudo" || echo "")


all:
	make -f Makefile-rv32.pro \
		PATH_TO_UXHW_SDK=$(PATH_TO_UXHW_SDK) \
		ENABLE_RV32_SUBSET=$(ENABLE_RV32_SUBSET) \
		REPRESENTATION_TYPE=$(REPRESENTATION_TYPE) \
		REPRESENTATION_SIZE=$(REPRESENTATION_SIZE) \
		CORRELATION_TRACKING=$(CORRELATION_TRACKING) \
		M_CONFIG_FILE=$(M_CONFIG_FILE) \
		UNCERTAIN_MEMORY_POOL_SIZE=$(UNCERTAIN_MEMORY_POOL_SIZE) \
		UNCERTAIN_MEMORY_SIZE=$(UNCERTAIN_MEMORY_SIZE)

#	The core is stopped before the flash write and LEFT stopped afterwards. This
#	is mandatory in MODE=xip, where the core fetches every instruction from the
#	very blocks being erased and reprogrammed, and it is considered good practise
#	in lram mode too.
#	Run `make start` to release the core once flashing has completed.
flash: $(PROGRAM).bin
	$(SUDO) $(PYTHON) $(TOOLKIT) --variant=C0-microSD+ $(DEVICE) config core-stop
	$(SUDO) $(PYTHON) $(TOOLKIT) --variant=C0-microSD+ $(DEVICE) flash-application $(PROGRAM).bin

stop:
	$(SUDO) $(PYTHON) $(TOOLKIT) --variant=C0-microSD+ $(DEVICE) config core-stop

start:
	$(SUDO) $(PYTHON) $(TOOLKIT) --variant=C0-microSD+ $(DEVICE) config core-start

clean:
	rm -rf build emulator/*.o main main.bin opt.err ucc.output uld.output main.map main.sr main.uncert.S
#	The objects built from the assets that sit alongside this Makefile: the
#	boot assembly, the toolchain's startup/sbrk, and the cache/DMA drivers.
	rm -f init-pro.o trap_vector.o startup.o sbrk.o cache.o dma.o dma_memops.o
