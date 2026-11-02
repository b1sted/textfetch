# textfetch C99 Engineering Style Guide

This document defines the coding, architecture, and safety rules for the
`textfetch` codebase. Every contribution must follow these rules. Automated
checks (`make format-check`, `make lint`, `make test`, and `make leaks`) enforce
the mechanical parts of the guide; reviewers enforce the rest.

## 1. Language Standard

- The project targets **strict ISO C99** (`-std=c99`) plus **POSIX.1-2008**
  (`-D_POSIX_C_SOURCE=200809L`). These flags are part of the default build.
- Do **not** use C11 or later features. In particular, the following are
  explicitly banned:
  - `_Generic`
  - anonymous structs and anonymous unions
  - `<threads.h>`
  - `<stdatomic.h>`
  - `_Static_assert`
  - variable length arrays (VLAs)
- Any array whose size depends on runtime data must be allocated on the heap or
  bounded by a compile-time constant. Prefer fixed-size buffers from
  `include/defs.h` and always pass their capacity explicitly.
- Do not rely on compiler extensions. GNU statement expressions, nested
  functions, and `typeof` are not permitted.
- Every source and header file starts with the SPDX identifier:
  `/* SPDX-License-Identifier: MIT */`.

## 2. Repository Architecture

The code is split into a portable core and a thin platform abstraction layer
(PAL):

```
include/            Public headers shared by the core and the PAL
  pal/              Platform abstraction interfaces
src/core/           Portable logic: configuration, UI, system, hardware,
                    utilities, containers, and the program entry point
src/platform/       Platform implementations
  posix/            POSIX.1-2008 fallbacks
  linux-common/     Shared Linux/Android implementation
  linux/            Linux-specific implementation
  android/          Android-specific implementation
  macos/            macOS-specific implementation
tests/              Unit and integration tests
```

Rules for the layering:

- `src/core/` must not include platform-specific headers or call
  platform-specific APIs directly. It talks to the PAL declared in
  `include/pal/`.
- Platform implementations provide the PAL symbols and may include their own
  OS headers.
- Shared helpers live in the core and are exported through `include/`.
- Headers must be self-contained: each header includes everything it needs and
  is protected by an include guard.

## 3. Naming and Namespacing

- All identifiers use strict `snake_case`.
- Every non-static function, type, and global carries a module prefix. The
  prefix identifies the owning module, not the file name alone:

  | Prefix      | Module                                      |
  |-------------|---------------------------------------------|
  | `util_`     | Generic utilities and file readers          |
  | `hw_`       | Hardware detection                          |
  | `sys_`      | System and kernel information               |
  | `tf_`       | Top-level orchestration and entry helpers   |
  | `ui_`       | Terminal rendering and output               |
  | `cfg_`      | Configuration and command-line state        |
  | `term_`     | Shell and terminal discovery                |
  | `capture_`  | External command capture                    |
  | `set_`      | Bitset container                            |
  | `strset_`   | String set container                        |
  | `hash_`     | Hashtable container                         |
  | `tree_`     | Binary tree container                       |

- Static functions may omit the prefix when the file is small and the name is
  unambiguous, but a module prefix is preferred.
- Macros and compile-time constants use `SCREAMING_SNAKE_CASE`.
- Type names use `snake_case` with a `_t` suffix (for example `data_unit_t`).
- Enum members use `SCREAMING_SNAKE_CASE` with a module-related prefix.
- `main` is the only function exempt from the prefix rule.

## 4. Formatting

Formatting is enforced by `clang-format` using the repository `.clang-format`.

- Indent with **4 spaces**. Never use tab characters.
- Maximum line length is **100 columns**.
- Braces follow a K&R style:
  - Function definitions place the opening brace on its own line.
  - Control statements (`if`, `for`, `while`, `switch`, `do`) place the
    opening brace on the same line.
  - `struct`, `enum`, and `union` bodies place the opening brace on the same
    line.
  - `else` stays on the same line as the closing brace.
- **Braces are mandatory for every body**, including single-statement
  `if`/`else`, loops, and `do`/`while`. `clang-format` inserts them
  automatically through `InsertBraces: true`.
- Pointer placement attaches the asterisk to the identifier:
  `char *buf`, not `char* buf`; `const char *name`, not `const char* name`.
- Do not align consecutive assignments or declarations.
- One statement per line. Do not use the comma operator to pack statements.
- Comments are part of the source of truth. Automated formatting may re-align
  comments but must never change their text.

Run the formatter before committing:

```bash
make format
make format-check
```

## 5. Types

- Include `<stdint.h>` and use fixed-width types for all data that crosses a
  module, file, syscall, or hardware boundary: `uint8_t`, `uint16_t`,
  `uint32_t`, `uint64_t`, `int8_t`, `int16_t`, `int32_t`, `int64_t`.
- Use `size_t` for object sizes, buffer capacities, indices, and counts.
- Use `bool` from `<stdbool.h>` for boolean values.
- Plain `int` is restricted to:
  - function exit codes and status values,
  - `argc` and `argv` processing,
  - trivial loop counters over small, local, compile-time-bounded ranges.
- Raw `long`, `unsigned long`, `short`, and `unsigned short` must not be used
  for stored data. When a libc or POSIX API requires them (for example
  `strtol`, `sysconf`, or `ftell`), convert at the call site and immediately
  narrow to a fixed-width type with an explicit range check.
- Always cast explicitly when narrowing. Never rely on implicit truncation.
- Use `PRIu32`, `PRId32`, and the other `<inttypes.h>` macros when formatting
  fixed-width types.
- Floating-point values are limited to the display and size-formatting paths;
  do not use them for identifiers, counters, or flags.

## 6. Const Correctness

- Mark pointer parameters `const` whenever the callee does not modify the
  pointee: `const char *path`, `const uint8_t *data`.
- Mark pointer-to-pointer arrays `const` at the appropriate level, for example
  `const char *const items[]`.
- Apply `const` only when it is safe. Do not add `const` when the address is
  passed to an API that expects a mutable pointer and would emit a
  `discarded-qualifiers` warning. Typical examples are `execv`, `open`,
  `strtol`, `strtoul`, and legacy platform interfaces.
- Prefer `const` on locals that are assigned once and never modified.
- Never cast away `const` to silence a warning. Fix the interface instead.

## 7. Initialization and Memory Safety

- Initialize every variable at declaration. Pointers default to `NULL`,
  scalars to `0`, and structures to `{0}`.
- When initializing a structure with designated initializers, initialize every
  field that matters to avoid `-Wmissing-field-initializers`. Prefer `{0}`
  followed by explicit assignments for partially populated structures.
- Check every allocation, every syscall result, and every pointer returned by a
  library function before use.
- `free()` accepts `NULL`; set pointers to `NULL` after freeing when the
  variable remains in scope.
- Never index an array without proving the index is within bounds.
- Use `snprintf` rather than `sprintf`, and always pass the destination
  capacity.
- Do not ignore return values from `read`, `write`, `open`, `close`, or
  allocation functions in production paths.

### Single exit point and `goto cleanup`

Functions that acquire more than one resource (file descriptors, heap
allocations, locks, platform handles) use a single exit point and unwind in
LIFO order:

```c
bool example(void) {
    int fd = -1;
    char *buf = NULL;
    bool ok = false;

    fd = open("/path", O_RDONLY);
    if (fd < 0) {
        goto cleanup;
    }

    buf = malloc(BUFFER_SIZE);
    if (buf == NULL) {
        goto cleanup;
    }

    /* ... use fd and buf ... */
    ok = true;

cleanup:
    free(buf);
    if (fd >= 0) {
        close(fd);
    }
    return ok;
}
```

- The `cleanup` label is the last statement before the return.
- Release resources in the reverse order of acquisition.
- Guard cleanup with checks so it is safe on partially initialized paths.
- Do not use `goto` for ordinary control flow; it is reserved for cleanup.

## 8. Header Ordering

Within every `.c` file, include headers in this order, separated by blank
lines:

1. The module's own header (for example `#include "sys_utils.h"`).
2. C99 standard headers (`<stdbool.h>`, `<stdint.h>`, `<stdio.h>`, ...).
3. POSIX headers (`<fcntl.h>`, `<unistd.h>`, `<sys/...>`, ...).
4. Project headers (`"config.h"`, `"defs.h"`, ...).

Headers use include guards rather than `#pragma once`. Do not include a header
that is not needed by the translation unit.

## 9. Functions and Control Flow

- Keep functions focused and short enough to review. Extract helpers when a
  function grows beyond a single responsibility.
- Validate all parameters at the top of public functions and return early on
  invalid input.
- Prefer early returns for error checks. Use `goto cleanup` only for resource
  unwinding as described above.
- Every `switch` handles all enum values, either with explicit cases or a
  `default` that fails safely.
- Do not use assignments inside conditions. Assign first, then test.
- Avoid macros that hide control flow. Prefer `static inline` functions for
  small helpers when a macro is not required.

## 10. Diagnostics and Output

- User-facing output goes through the `ui_` module so formatting, colors, and
  labels remain consistent.
- Verbose diagnostics use the `V_PRINTF` wrapper and go to `stderr`.
- Error messages are prefixed with `[ERROR]`.
- Do not print from library-style helpers unless the existing interface
  explicitly does so.
- Output must remain stable across formatting-only changes. Formatting and
  brace insertion must never change CLI output or program behavior.

## 11. Testing and Validation

Every change must pass:

```bash
make format-check   # formatting and brace insertion are clean
make dev            # debug build succeeds
make test           # all test suites pass
make leaks          # AddressSanitizer reports zero leaks
make lint           # clang-tidy, cppcheck, and format-check pass
```

Tests live in `tests/` and are split into `core/` and `platform/`. New
behavior requires a regression test. Platform-specific tests must remain
skippable or stubbable on other platforms.

## 12. Commits

- Use Conventional Commits: `feat:`, `fix:`, `refactor:`, `style:`, `docs:`,
  `test:`, `build:`, or `chore:`.
- Keep commits focused. Formatting-only changes belong in a `style:` commit.
- All commits and merge commits must be GPG-signed (`git commit -S`,
  `git merge -S`).
- Write commit messages in English and describe the intent, not just the files
  touched.
