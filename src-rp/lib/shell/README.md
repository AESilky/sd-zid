# Interactive Shell - On the terminal on Pico (RP2040/RP2350)

Provides an interactive shell and terminal client interface for the
Raspberry Pi Pico (Pico, Pico-W, Pico2, Pico2-W, etc.).

ANSI Terminal control/escape sequences are used for the terminal interface.

The shell provides a command interpreter that provides a few base commands and
allows other commands to be registered.

The terminal and shell use STDIO but don't initialize it, allowing a parent
project to initialize it as desired.

## Test **main**

The test directory **shell_test.c** contains a **main** method, and the Makefile
creates an executable that can be used to interactively test the Shell command
processing.

In addition, it contains comments that describe how the Shell would be added
into an SD Cooperative Multitasking (CMT) application.

## Git Submodule

The project is laid out to be included in another project as a Git Submodule.
When including as a submodule, only the src directory needs to be included by
the parent module. The root level and test CMakeLists.txt files do not need to
be included when adding the shell and command processor to a project.

## License

Copyright 2023-26 AESilky
SPDX-License-Identifier: MIT License
