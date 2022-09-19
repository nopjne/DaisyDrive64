# Project Name
TARGET = DaisyDrive64

LDSCRIPT ?= Linkerscript.ld

# Sources
CPP_SOURCES = daisydrive64main.cpp cic_c.cpp n64pi.cpp n64si.cpp
C_SOURCES = stm32h7xx_it.c ../Middlewares/Third_Party/FatFs/src/option/unicode.c

# Library Locations
# Library Locations
LIBDAISY_DIR = ..
#DAISYSP_DIR = ../libDaisySP

# Includes FatFS source files within project.
DUSE_FATFS=1
D_FS_EXFAT=1
D_LFN_UNICODE=1
USE_FULL_LL_DRIVER=1
#DDEBUG=1
#DDEBUG_DEFAULT_INTERRUPT_HANDLERS=1
#DEBUG_DEFAULT_INTERRUPT_HANDLERS=1
OPTIMIZE_FLAG = -ggdb3 -O3 -marm

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile


#LDFLAGS += -T$(LDSCRIPT)

#LDSCRIPT ?= ../core/STM32H750IB_flash.lds

