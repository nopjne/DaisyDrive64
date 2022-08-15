# Project Name
TARGET = dmaex

# Sources
CPP_SOURCES = main.cpp stm32h7xx_it.cpp
C_SOURCES = ../src/sys/system_stm32h7xx.c

# Library Locations
LIBDAISY_DIR = ..
#DAISYSP_DIR = ../libDaisySP

# Includes FatFS source files within project.
#USE_FATFS = 1

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

LDSCRIPT ?= ../core/STM32H750IB_flash.lds
