# PBD Module — firmware Makefile (Electro-Smith Daisy Patch)
#
# Requires the ARM toolchain:
#   macOS:  brew install armmbed/formulae/arm-none-eabi-gcc
#   Linux:  sudo apt install gcc-arm-none-eabi
#   Windows: Daisy Toolchain installer (https://daisy.audio/tutorials/cpp-dev-env/)
#
# Targets:
#   make              build build/pbd.bin
#   make program-dfu  flash over USB (module in DFU mode: hold BOOT, tap RESET)
#   make clean

TARGET = pbd

# Sources
CPP_SOURCES = src/main.cpp

# Library locations (libDaisy is the only dependency)
LIBDAISY_DIR = deps/libDaisy

# Bootloader-based app: runs from SRAM, room to grow.
# First-time setup: flash the Daisy bootloader once (see docs/BUILD.md).
APP_TYPE = BOOT_SRAM

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
