#!/bin/bash
# Copyright (c) 2026 Larry Pyeatt.  All rights reserved.
#
# Wrapper for riscv32-xilinx-elf-gdb that skips ~/.gdbinit.
# The Xilinx GDB build lacks Python support, but ~/.gdbinit uses
# gdb-dashboard which requires Python.
exec /opt/Xilinx/2025.1/gnu/riscv/lin/riscv64-unknown-elf/x86_64-oesdk-linux/usr/bin/riscv32-xilinx-elf/riscv32-xilinx-elf-gdb -nx "$@"
