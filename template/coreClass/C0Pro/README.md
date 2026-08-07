# C0Pro Makefile Build Targets: `anonymize` and `onError`

This document explains the `anonymize` and `onError` targets in `Makefile.pro` and when they are invoked.

## Overview

The `all` target drives the full build and calls both `anonymize` and `onError` automatically. Neither target needs to be invoked manually under normal circumstances.

```makefile
all: Makefile.pro
    make --makefile=Makefile.pro $(PROGRAM) filterAndOutput || { rc=$$?; make --makefile=Makefile.pro onError; make --makefile=Makefile.pro anonymize >/dev/null 2>&1 || true; exit $$rc; }
    @make --makefile=Makefile.pro anonymize >/dev/null 2>&1 || true
```

---

## `anonymize`

### Purpose

Produces a sanitised copy of the compiled LLVM IR that can be shared or inspected without exposing the original symbol names or debug information.

### When it runs

`anonymize` is called by the `all` target unconditionally — both on a successful build and on a build failure:

- **Success path** — runs after `filterAndOutput` completes without error.
- **Failure path** — runs after `onError`, before the original non-zero exit code is propagated.

In both cases, the call is wrapped with `>/dev/null 2>&1 || true`, so a failure of `anonymize` itself is silently ignored and never aborts the build.

### What it does

The target operates on the combined LLVM IR file produced by the link step (`$(BUILD_DIR)/$(PROGRAM)-link.ll`) and applies two successive `opt` passes:

1. **Rename symbols and strip dead prototypes**
   ```
   opt -passes="metarenamer,strip-dead-prototypes" -S build/main-link.ll -o build/main-renamed.ll
   ```
   - `metarenamer` replaces all symbol names with generic, non-descriptive identifiers.
   - `strip-dead-prototypes` removes function declarations that have no corresponding definition and are unreferenced.

2. **Strip debug information**
   ```
   opt --strip-debug -S build/main-renamed.ll -o build/main-anon-final.ll
   ```
   Removes all DWARF debug metadata so that source file paths, line numbers, and variable names are not present in the output.

### Output

| File | Description |
|------|-------------|
| `build/main-renamed.ll` | Intermediate: renamed symbols, dead prototypes removed |
| `build/main-anon-final.ll` | Final: renamed + debug info stripped |

### Related target: `reduceAndAnonymize`

`reduceAndAnonymize` is a heavier variant. It first uses `llvm-reduce` to minimise the IR (keeping only the parts that influence the uncertainty optimisation pass), then applies the same rename and strip steps. It is not called automatically by `all`; invoke it manually when a minimal reproducer is needed.

---

## `onError`

### Purpose

Ensures that compiler and linker diagnostics are forwarded to `stderr` in a clean, readable form when the build fails, so that the output can be consumed by downstream tooling.


### When it runs

`onError` is called only on the failure path of `all`, after the build command exits with a non-zero status:

```makefile
... || { rc=$$?; make --makefile=Makefile.pro onError; make --makefile=Makefile.pro anonymize ...; exit $$rc; }
```

It is not called on a successful build.

### What it does

`onError` simply delegates to `filterAndOutput`:

```makefile
onError: filterAndOutput
```

`filterAndOutput` performs the following:

1. Ensures `uld.output` exists (`touch uld.output`) so the subsequent `sed` call never fails on a missing file.
2. Filters `ucc.output` (compiler diagnostics) through `sed` to strip internal path prefixes, then writes the result to `stderr`:
   - SDK root (`$(PATH_TO_UXHW_SDK)`)
   - System include prefix (`$(PREFIX)/include/`)
   - Clang binary prefix
   - Current working directory
   - musl libc path (`/opt/musl-signaloid/`)
3. Filters `uld.output` (linker diagnostics) through `sed` with a similar set of substitutions, also writing to `stderr`.

Redirecting to `stderr` is intentional: the consuming tooling reads build diagnostics from `stderr`, not `stdout`.


### Output files

| File | Description |
|------|-------------|
| `ucc.output` | Raw compiler (`clang`) diagnostics, written during compilation |
| `uld.output` | Raw linker diagnostics, written during linking |

These files are cleaned up by `make clean`.
