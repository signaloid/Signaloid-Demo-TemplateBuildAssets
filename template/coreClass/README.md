# Supported Signaloid Core Classes

This directory contains template build assets, including Makefiles, startup files, and linker scripts, for the following classes of Signaloid cores:
- C0: Baseline implementation of the Signaloid cores. This class generates binaries for the RISC-V architecture.
- C0 Pro: High-performance variant of the C0 core class.
- C0-microSD: Class targeting the Signaloid C0-microSD hardware module. This class generates binaries for the RISC-V architecture.
- C0-microSD-plus: Class targeting the high-performance Signaloid C0-microSD+ hardware module. This class generates binaries for the RISC-V architecture.
- C0-SD: Class targeting the Signaloid C0-SD hardware module. This class generates binaries for the RISC-V architecture.

The `common/` directory contains files that more than one core class needs, but not every class needs every file:
- `startup.cpp`: runs the static constructors on the freestanding RISC-V targets. Used by C0, C0-microSD, C0-microSD-plus, and C0-SD.
- `sbrk.c`: bounds the heap at the `_heap_end` symbol that the linker scripts of the hardware modules define. Used by C0-microSD, C0-microSD-plus, and C0-SD. C0 uses the `_sbrk` of its C library instead.

C0 Pro and the native build (`native/`) target hosted environments and use no file from `common/`.
