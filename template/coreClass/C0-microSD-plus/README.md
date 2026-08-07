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
(no separate bootloader): it DMA-copies the writable image from flash to SRAM,
zeroes `.bss`, runs C++ global constructors, and calls `main`.

In `lram` the whole application is copied into SRAM, so the image must fit LRAM.
In `xip` only the writable sections are copied, so the image is bounded by the
**flash user region**, not by SRAM. An `xip` application may be far larger than
LRAM. Overflow in either mode is caught at link time by the linker script's
`MEMORY` sizes and its overflow `ASSERT`.

## How to flash

1. Modify the `TOOLKIT` flag in the `Makefile` to point to your C0-microSD+ toolkit, and the `DEVICE` flag to point to your C0-microSD+ device path.
2. Run `make`, `make flash`, then `make start`. These targets use `sudo`.

`make flash` stops the RV32 core before the write (`config core-stop`) and
**leaves it stopped**. This is mandatory in `xip`, where the core would otherwise
be fetching instructions from the very flash blocks being erased and
reprogrammed. Release the core with `make start` (`config core-start`) once
flashing has completed; `make stop` exposes the stop control on its own.
