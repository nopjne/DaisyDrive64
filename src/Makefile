# Library Locations
LIBDAISY_DIR = ../external/libdaisy

# Project Name
TARGET = DaisyDrive64
LDSCRIPT ?= Linkerscript.ld

# Generate the rom to be flashed.
BINTOARRAY = ../tools/bintoarray
ROMPARAMETERS = ../tools/menurom.z64 "__attribute__((section(\".qspiflash_text\"))) menurom"
# TODO: Enable when qspi init for stm32 is working.
#GENERATE := $(shell $(BINTOARRAY) $(ROMPARAMETERS) > menurom.h)

# Sources
CPP_SOURCES = daisydrive64main.cpp cic_c.cpp n64pi.cpp n64si.cpp flashram.cpp menu.cpp
C_SOURCES = stm32h7xx_it.c ../external/libdaisy/Middlewares/Third_Party/FatFs/src/option/unicode.c

# Includes FatFS source files within project.
DUSE_FATFS=1
D_FS_EXFAT=1
D_LFN_UNICODE=1
USE_FULL_LL_DRIVER=1
#DDEBUG=1
OPTIMIZE_FLAG = -ggdb3 -O3

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

generate:
	@echo $(GENERATE)

.PHONY: all generate
