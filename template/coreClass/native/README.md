# Native Build Template

`Makefile.native` builds an application for the Native core class. It compiles the application's C/C++ sources with stock GCC into a statically linked x86-64 executable named `main`. No Signaloid SDK is involved.

## The UxHw® API

Calls to the API are served by the [UxHw compatibility layer](https://github.com/signaloid/Signaloid-Demo-UxHwCompatibilityForNativeExecution), which implements each call by drawing one random sample with GSL. As a result:

- **A run is a single Monte Carlo sample.** A C0 Pro run returns a distribution. A native run returns one draw from it.
- **Runs are not reproducible.** The random number generator is seeded from the time of day, so two runs of the same program give different output.
- **GSL is linked in.** The compatibility layer uses GNU GSL, which is statically linked into the executable.

Every build gets a `uxhw.h` and a `uxhw.c`. Each is the application's if the build directory contains one, and the compatibility layer's otherwise:

- If the `SOURCES` in `config.mk` list a `uxhw.c`, that file is the implementation. Otherwise the template compiles the application's `uxhw.c` closest to the top of the build directory, or, if there is none, the compatibility layer's.
- The template adds the directory of a `uxhw.h` to the include path, so `#include <uxhw.h>` finds it wherever it sits in the application. It takes the `uxhw.h` beside the chosen `uxhw.c`, otherwise the one closest to the top of the build directory, otherwise the compatibility layer's.
- An application `uxhw.h` without its own `uxhw.c` is linked with the compatibility layer's implementation, which is compiled against the layer's header. The two headers must therefore declare the same API.
- A symbolic link counts only if it leads to a file. A link into a submodule that was not checked out is ignored, even when `SOURCES` list it, and the compatibility layer's file is used instead.
- If the application manages both files outside the build directory, set `USE_SIGNALOID_UXHW_COMPAT = 0` in `config.mk`, and the template adds neither.

`-lgsl -lgslcblas -lm` are always linked.

## config.mk

As with the other templates, `config.mk` can set `SOURCES`, `INC`, `CFLAGS`, `CXXFLAGS`, `LDFLAGS`, `LIBS` and `OPTFLAGS`. Without a `config.mk`, every C/C++ source under the build directory is compiled, with `-std=c++14` for C++. `OPTFLAGS` defaults to `-O2 -gdwarf-4`.

The executable is always linked with `-static`, because the execution sandbox has no C library and no dynamic loader. Kea Cloud rejects a build whose `main` cannot run there:

- an executable that is dynamically linked or needs shared libraries
- anything other than an x86-64 ELF executable, such as an object file or a script
- a `main` that is a symbolic link, or anything else that is not a regular file. Only `main` itself is kept after the build, so a link would point at nothing when the task runs. Make the build write the executable as `main` directly.

A rejected build is reported with the reason, which also appears in the build log.

## Execution environment

- The program runs inside the gVisor sandbox, with no network access.
- stdin is empty (end of file).
- The environment contains only `PATH`, `HOME`, `TMPDIR` and `SQLITE_TMPDIR`.
- `/tmp` is writable.
- A non-zero exit status marks the task as failed.

## Build logs

Compiler output goes to `ucc.output` and linker output to `uld.output`. `filterAndOutput` prints both to stderr with the build directory stripped. The `all` target preserves the exit code of the recipe that failed, which Kea Cloud uses to tell a compile error in the application from a toolchain failure.
