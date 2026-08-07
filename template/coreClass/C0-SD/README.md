# Signaloid SoC application (C0-SD)

This directory holds the application that runs in the Signaloid C0-SD.

## Execution mode

The application's boot/execution mode is selected with the `MODE` variable in
`config.mk` (defaults to `psram`):

| `MODE`  | Where the application runs                                             |
|---------|-----------------------------------------------------------------------|
| `lram`  | Whole application in on-die SRAM (fastest; bounded by LRAM size).      |
| `psram` | Whole application in external HyperRAM (largest; leaves LRAM free).    |
| `xip`   | Code executes in place from flash; only writable data lives in SRAM.  |

`MODE` selects the matching pre-built linker script (`lram.ld` / `psram.ld` /
`xip.ld`) and load address; the `init-pro.S` boot stub then brings the system up
on its own (no separate bootloader): it DMA-copies the writable image from flash
to the runtime region, zeroes `.bss`, runs C++ global constructors, and calls
`main`.

In `lram` and `psram` the whole application is copied into RAM, so the image must
fit that region. In `xip` only the writable sections are copied, so the image is
bounded by the **flash user region**, not by SRAM. An `xip` application may be
far larger than LRAM. Overflow in any mode is caught at link time by the linker
script's `MEMORY` sizes and its overflow `ASSERT`.

## How to flash

1. Modify the `DEVICE` flag in the `Makefile` to point to your C0-SD device path.
2. Run `make`, `make flash`, then `make start`.

`make flash` stops the RV32 core before the write (`config core-stop`) and
**leaves it stopped**. This is mandatory in `xip`, where the core would otherwise
be fetching instructions from the very flash blocks being erased and
reprogrammed. Release the core with `make start` (`config core-start`) once
flashing has completed; `make stop` exposes the stop control on its own.
