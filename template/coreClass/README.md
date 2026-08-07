# Supported Signaloid Core Classes

This directory contains template build assets, including Makefiles, startup files, and linker scripts, for the following classes of Signaloid cores:
- C0: Baseline implementation of the Signaloid cores. This class generates binaries for the RISC-V architecture.
- C0 Pro: High-performance variant of the C0 core class.
- C0-microSD: Class targeting the Signaloid C0-microSD hardware module. This class generates binaries for the RISC-V architecture.
- C0-microSD-plus: Class targeting the high-performance Signaloid C0-microSD+ hardware module. This class generates binaries for the RISC-V architecture.

Files inside the `common/` directory are necessary for building an application for any of the Signaloid core classes above.
