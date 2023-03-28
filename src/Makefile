# Library Locations
LIBDAISY_DIR = ../external/libdaisy

# Project Name
TARGET = DaisyDrive64
LDSCRIPT ?= Linkerscript.ld

# Generate the rom to be flashed.
BINTOARRAY = ../tools/bintoarray
NOBOOTLOADERMP3 = ../tools/nobootloader.mp3 "__attribute__((section(\".qspiflash_text\"))) nobootloadermp3"
NOSDCARDMP3 = ../tools/sd_card_not_detected.mp3 "__attribute__((section(\".qspiflash_text\"))) nosdcardmp3"
SDFAILEDMP3 = ../tools/sdfailed.mp3 "__attribute__((section(\".qspiflash_text\"))) sdfailedmp3"
REGIONSWITCHMP3 = ../tools/regionswitch.mp3 "__attribute__((section(\".qspiflash_text\"))) regionswitchmp3"
# The bootrom absolutely needs to be in the following format n64: 37 80 40 12
BOOTROM = ../tools/OS64daisyboot.n64 "__attribute__((section(\".qspiflash_text\"))) bootrom"

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

generate_data:
	$(shell $(BINTOARRAY) $(BOOTROM) > bootrom.h)
	$(shell $(BINTOARRAY) $(NOBOOTLOADERMP3) > sounds.h)
	$(shell $(BINTOARRAY) $(NOSDCARDMP3) >> sounds.h)
	$(shell $(BINTOARRAY) $(SDFAILEDMP3) >> sounds.h)
	$(shell $(BINTOARRAY) $(REGIONSWITCHMP3) >> sounds.h)

.PHONY: all generate_data

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) --remove-section *qspifl* $< $(BUILD_DIR)/$(TARGET)_fw.bin
	$(BIN) --only-section *qspifl* $< $(BUILD_DIR)/$(TARGET)_data.bin
