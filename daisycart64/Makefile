# Project Name
TARGET = DaisyCart64

# Sources
CPP_SOURCES = MCUMain.cpp cic_c.cpp
C_SOURCES = stm32h7xx_it.c ../Middlewares/Third_Party/FatFs/src/option/unicode.c

# Library Locations
# Library Locations
LIBDAISY_DIR = ..
#DAISYSP_DIR = ../libDaisySP

# Includes FatFS source files within project.
DUSE_FATFS=1
D_FS_EXFAT=1
D_LFN_UNICODE=1
DDEBUG=1
#DEBUG_DEFAULT_INTERRUPT_HANDLERS=1

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

LDSCRIPT ?= ../core/STM32H750IB_flash.lds
