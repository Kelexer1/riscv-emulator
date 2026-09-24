# RISC-V Emulator

![CI](https://github.com/Kelexer1/riscv-emulator/actions/workflows/ci.yml/badge.svg)

A RISC-V 32-bit emulator written from scratch in C, with a virtual memory model, an ELF32 loader, and an interactive debugger. It runs ELF32 executables from the companion [riscv-assembler](https://github.com/Kelexer1/riscv-assembler) or any RV32IM toolchain.

Designed for Unix systems, or WSL on Windows.

## Features

- RV32I and the M extension, plus `fence` and `fence.i` (as no-ops)
- Two-level Sv32-style virtual memory with permission-checked reads and writes
- ELF32 loader
- Interactive debugger with breakpoints, watchpoints, and symbol-aware disassembly
- Syscalls for console I/O, timing, heap allocation, and exit
- Validated against the official [riscv-tests](https://github.com/riscv-software-src/riscv-tests) rv32ui and rv32um suites (50/50)

## Dependencies

- cmake 3.21 or newer
- gcc 13 or newer
- clangd (optional, for editor tooling)

## Building

```sh
cmake -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
```

| Build type | Description |
| --- | --- |
| `Debug` (default) | `-O0 -g` with AddressSanitizer |
| `Test` | Builds the unit tests instead of the emulator, with AddressSanitizer |
| `Release` | `-O2` |
| `Profile` | `-O2 -g` with frame pointers, for `perf` |

## Usage

```sh
riscv-emulator <run|debug> <program.elf>
```

`run` executes the program, and `debug` starts the interactive debugger. To assemble and run a program:

```sh
riscv-assembler -o hello.elf hello.s
riscv-emulator run hello.elf
```

The program image must end below `0x7FDFF000`, where the stack guard page begins.

## Supported Syscalls

The syscall numbers are chosen in `a7`, and follow the numbering of the RISC-V syscalls in [CPUlator](https://cpulator.01xz.net/doc/).

| `a7` | Name | Arguments and results |
| --- | --- | --- |
| 1 | print integer | `a0` = value |
| 4 | print string | `a0` = address of a null-terminated string |
| 5 | read integer | result in `a0` (decimal, or hex if decimal parsing fails) |
| 8 | read string | `a0` = buffer address; reads one line and null-terminates it |
| 9 | sbrk | `a0` = bytes to allocate; previous break returned in `a0` |
| 10 | exit | |
| 11 | print character | `a0` = character |
| 12 | read character | result in `a0` |
| 17 | exit with code | `a0` = exit code |
| 30 | time | `a0` = low 32 bits, `a1` = high 32 bits of milliseconds since the Unix epoch |
| 32 | sleep | `a0` = milliseconds |
| 34 | print hexadecimal | `a0` = value |
| 35 | print binary | `a0` = value |
| 36 | print unsigned integer | `a0` = value |

## Running Tests

Unit tests:

```sh
cmake -B build/test -DCMAKE_BUILD_TYPE=Test
cmake --build build/test
ctest --test-dir build/test --output-on-failure
```

## Validation with riscv-tests

The emulator passes all 50 rv32ui and rv32um tests from riscv-tests. The tests are assembled with GNU binutils, linked at `0x10000`, and run under a minimal user-mode environment, so the privileged (rv32mi) tests are not covered.

Requirements: a RISC-V GNU toolchain (`riscv64-unknown-elf-` binutils and GCC), Python 3, and a checkout of riscv-tests.

```sh
git clone https://github.com/riscv-software-src/riscv-tests
cmake -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
python3 tests/riscv-tests/run_riscv_tests.py \
  --riscv-tests riscv-tests \
  --emulator build/release/riscv-emulator \
  --emulator-only
```

Both the unit tests and this suite run in CI on every push and pull request.

## Memory Model

```
Low Addresses
+-----------------------------------------------------------+ 0x00000000
|  .text (Executable program code)                          |
|  Permissions: Read / Execute                              |
+-----------------------------------------------------------+
|  .rodata (Read-only data / constants)                     |
|  Permissions: Read                                        |
+-----------------------------------------------------------+
|  .data (Initialized read-write data)                      |
|  Permissions: Read / Write                                |
+-----------------------------------------------------------+
|  .bss (Uninitialized data, zeroed out)                    |
|  Permissions: Read / Write                                |
+-----------------------------------------------------------+
|  Heap                                                     |
|                                                           |
+-----------------------------------------------------------+
|  Unmapped Free Space                                      |
|                                                           |
+-----------------------------------------------------------+ 0x7FDFF000
|  Stack Guard Page                                         |
|  Size: 1 Page                                             |
+-----------------------------------------------------------+ 0x7FE00000
|  Stack                                                    |
|  Size: 2 MiB                                              |
|  Permissions: Read / Write                                |
+-----------------------------------------------------------+ 0x80000000
High Addresses
```