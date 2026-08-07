# Template Build Assets for the Signaloid UxHw SDK
This repository contains template build assets for compiling C/C++ applications using the Signaloid UxHw SDK. The assets include Makefiles, startup files, and linker scripts. For each Signaloid core class (e.g., C0 vs. C0 Pro), you can find the template assets in the `template/coreClass/` directory. This directory also contains a `common/` sub-directory that includes template assets, e.g., startup files, that you need to build an application for any Signaloid core class.

## Configuring the Build

When using the template assets to build an application, you can configure the default build behavior using `config.mk` in the source directory of your application.

For example, for C repositories, there are two relevant variables in `config.mk`:
- `SOURCES`, a list of C source files to compile;
- `CFLAGS`, the options that are passed to the C compiler.

If the `SOURCES` variable is not set, the template build assets search for and compile all the C/C++ files in the source code directory of your application.
