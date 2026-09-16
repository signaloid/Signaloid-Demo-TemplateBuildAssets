# Signaloid SoC application (C0-microSD+)

This directory holds the application that runs in the Signaloid C0-microSD+.

## Execution mode

The application's boot/execution mode is selected with the `MODE` variable in
`config.mk` (defaults to `lram`):

| `MODE`  | Where the application runs                                            |
|---------|----------------------------------------------------------------------|
| `lram`  | Whole application in on-die SRAM (fastest; bounded by LRAM size).     |
| `xip`   | Code executes in place from flash; only writable data lives in SRAM. |

`MODE` selects the matching pre-built linker script (`lram.ld` / `xip.ld`) and
load address; the `init-pro.S` boot stub then brings the system up on its own
(no separate bootloader): it claims the DMA engine, DMA-copies the writable image
from flash to SRAM, zeroes `.bss` with a second DMA operation, invalidates the
data cache once, runs C++ global constructors, and calls `main`.

In `lram` the whole application is copied into SRAM, so the image must fit LRAM.
In `xip` only the writable sections are copied, so the image is bounded by the
**flash user region**, not by SRAM. An `xip` application may be far larger than
LRAM. Overflow in either mode is caught at link time by the linker script's
`MEMORY` sizes and its overflow `ASSERT`.

## SoC drivers

Three files ship alongside the boot assets and the linker scripts:

| File                  | What it provides                                        |
|-----------------------|---------------------------------------------------------|
| `cache.c` / `cache.h` | RV32RX instruction- and data-cache maintenance.         |
| `dma.c` / `dma.h`     | The AXIL PIB DMA engine driver.                         |
| `dma_memops.c`        | DMA-backed `memcpy` / `memset` (opt-in, see below).     |

`cache.c` and `dma.c` are linked into every application. The data cache is
write-through, so cache maintenance is only ever needed to **invalidate** before
reading memory that the DMA engine or the SD host wrote; `cacheInvalidateRange()`
is the usual entry point. `dmaTryRun()` performs one operation synchronously and
only if the engine is free, and invalidates a cacheable destination afterwards.

Read the comment block at the top of `dma.h` before driving the engine yourself.
It is the hardware contract, and several of the constraints in it have no
diagnostic when you get them wrong: a length of `0`, or one within a burst of
2^32, is an unabortable multi-gigabyte runaway; `CONTROL` is sampled only in
`Idle`, so a write in `Done` is dropped and the stale `Done` then reads back as
success; and a core reset does not reset the engine.

Both are compiled as plain objects rather than through the uncertainty
optimisation pipeline -- they are hardware drivers, with no uncertainty in them
for it to act on -- so they must stay out of `SOURCES`. `Makefile-rv32.pro`
already excludes all three by name, so autodetected builds need no change.

### DMA-backed `memcpy` and `memset`

Set `DMA_MEMOPS=1` in `config.mk` to route `memcpy` and `memset` through the
engine, via the linker's `--wrap`. `DMA_MEMOPS_THRESHOLD` (default `128`) is the
length below which the wrapper calls the C library instead.

It is off by default, and worth measuring before turning on: it replaces CPU
single-beat stores with 512-byte bursts, which is measurable in SD-side latency,
and it shares the single DMA engine with the application. Two things it does not
cover -- a large struct assignment (`*a = *b`) is expanded inline and never
reaches the wrapper, and `memmove` is not wrapped.

## How to flash

1. Modify the `TOOLKIT` flag in the `Makefile` to point to your C0-microSD+ toolkit, and the `DEVICE` flag to point to your C0-microSD+ device path.
2. Run `make`, `make flash`, then `make start`. These targets use `sudo`.

`make flash` stops the RV32 core before the write (`config core-stop`) and
**leaves it stopped**. This is mandatory in `xip`, where the core would otherwise
be fetching instructions from the very flash blocks being erased and
reprogrammed. Release the core with `make start` (`config core-start`) once
flashing has completed; `make stop` exposes the stop control on its own.
