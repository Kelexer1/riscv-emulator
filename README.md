**About**

A full RISC-V 32 bit assembly suite of tools built from the ground up in C, including a full assembler pipeline, custom
virtual memory model, custom binary loader, emulator, and debugger. This project is designed only for unix systems, or
WSL systems on Windows machines. The project currently has support for RISC-V32 I+M, and various pseudo-instructions.

**Dependencies**
- clangd
- cmake
- gcc

**Building**
```sh
cmake -B build/* -DCMAKE_BUILD_TYPE=*
cmake --build build/*
```

**Usage**
```sh
riscv-emulator [run|debug] <path_to_assembly>
```

**Running Tests**
```sh
ctest --test-dir build/test --output-on-failure
```

**Supported Syscalls**

Syscalls mirror the RISC-V syscalls for [CPUlator](https://cpulator.01xz.net/doc/)

**Memory Model**
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