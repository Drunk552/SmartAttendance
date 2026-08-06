# CMake modules

This directory contains shared build modules. Future board toolchain files must
use a dedicated sysroot and must not reference host include or library paths.
No board toolchain is provided until the target ABI, C library and BSP are
fixed.

The checked-in preset uses build preset schema support introduced in CMake
3.20. Hosts limited to the project's minimum CMake 3.16 continue to use the
documented `cmake -S` and `cmake --build` commands.
