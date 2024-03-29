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
ROMFSTATFAILEDMP3 = ../tools/romfstatfailed.mp3 "__attribute__((section(\".qspiflash_text\"))) romfstatfailedmp3"
ROMREADFAILEDMP3 = ../tools/romreadfailed.mp3 "__attribute__((section(\".qspiflash_text\"))) romreadfailedmp3"
ROMSIZEMISMATCHMP3 = ../tools/romsizemismatch.mp3 "__attribute__((section(\".qspiflash_text\"))) romsizemismatchmp3"
ROMFOPENFAILEDMP3 = ../tools/romfopenfailed.mp3 "__attribute__((section(\".qspiflash_text\"))) romfopenfailedmp3"
ROMLOADSIZEMISMATCHMP3 = ../tools/romloadsizemismatch.mp3 "__attribute__((section(\".qspiflash_text\"))) romloadsizemismatchmp3"
EEPROMCREATESIZEMISMATCHMP3 = ../tools/eepromcreatesizemismatch.mp3 "__attribute__((section(\".qspiflash_text\"))) eepromcreatesizemismatchmp3"
EEPROMSTATSIZEMISMATCHMP3 = ../tools/eepromstatsizemismatch.mp3 "__attribute__((section(\".qspiflash_text\"))) eepromstatsizemismatchmp3"
EEPROMLOADFAILEDMP3 = ../tools/eepromloadfailed.mp3 "__attribute__((section(\".qspiflash_text\"))) eepromloadfailedmp3"
FLASHCREATESIZEMISMATCHMP3 = ../tools/flashcreatesizemismatch.mp3 "__attribute__((section(\".qspiflash_text\"))) flashcreatesizemismatchmp3"
FLASHFSTATSIZEMISMATCHMP3 = ../tools/flashfstatsizemismatch.mp3 "__attribute__((section(\".qspiflash_text\"))) flashfstatsizemismatchmp3"
FLASHLOADFAILEDMP3 = ../tools/flashloadfailed.mp3 "__attribute__((section(\".qspiflash_text\"))) flashloadfailedmp3" 
# The bootrom absolutely needs to be in the following format n64: 37 80 40 12
BOOTROM = ../tools/OS64daisyboot.n64 "__attribute__((section(\".qspiflash_text\"))) bootrom"

# Sources
CPP_SOURCES = daisydrive64main.cpp cic_c.cpp n64pi.cpp n64si.cpp flashram.cpp menu.cpp bootrom.cpp sounds.cpp
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

#TODO: fix the duplication here.
generate_data:
	$(shell $(BINTOARRAY) $(BOOTROM) > bootrom.cpp)
	$(shell $(BINTOARRAY) $(NOBOOTLOADERMP3) > sounds.cpp)
	$(shell $(BINTOARRAY) $(NOSDCARDMP3) >> sounds.cpp)
	$(shell $(BINTOARRAY) $(SDFAILEDMP3) >> sounds.cpp)
	$(shell $(BINTOARRAY) $(REGIONSWITCHMP3) >> sounds.cpp)
	$(shell $(BINTOARRAY) $(ROMFSTATFAILEDMP3) >> sounds.cpp)
	$(shell $(BINTOARRAY) $(ROMREADFAILEDMP3) >> sounds.cpp)
	$(shell $(BINTOARRAY) $(ROMSIZEMISMATCHMP3) >> sounds.cpp)
	$(shell $(BINTOARRAY) $(ROMFOPENFAILEDMP3) >> sounds.cpp)
	$(shell $(BINTOARRAY) $(ROMLOADSIZEMISMATCHMP3) >> sounds.cpp)
	$(shell $(BINTOARRAY) $(EEPROMCREATESIZEMISMATCHMP3) >> sounds.cpp)
	$(shell $(BINTOARRAY) $(EEPROMSTATSIZEMISMATCHMP3) >> sounds.cpp)
	$(shell $(BINTOARRAY) $(EEPROMLOADFAILEDMP3) >> sounds.cpp)
	$(shell $(BINTOARRAY) $(FLASHCREATESIZEMISMATCHMP3) >> sounds.cpp)
	$(shell $(BINTOARRAY) $(FLASHFSTATSIZEMISMATCHMP3) >> sounds.cpp)
	$(shell $(BINTOARRAY) $(FLASHLOADFAILEDMP3) >> sounds.cpp)
	$(shell $(BINTOARRAY) $(BOOTROM) "-h" > bootrom.h)
	$(shell $(BINTOARRAY) $(NOBOOTLOADERMP3) "-h" > sounds.h)
	$(shell $(BINTOARRAY) $(NOSDCARDMP3) "-h" >> sounds.h)
	$(shell $(BINTOARRAY) $(SDFAILEDMP3) "-h" >> sounds.h)
	$(shell $(BINTOARRAY) $(REGIONSWITCHMP3) "-h" >> sounds.h)
	$(shell $(BINTOARRAY) $(ROMFSTATFAILEDMP3) "-h" >> sounds.h)
	$(shell $(BINTOARRAY) $(ROMREADFAILEDMP3) "-h" >> sounds.h)
	$(shell $(BINTOARRAY) $(ROMSIZEMISMATCHMP3) "-h" >> sounds.h)
	$(shell $(BINTOARRAY) $(ROMFOPENFAILEDMP3) "-h" >> sounds.h)
	$(shell $(BINTOARRAY) $(ROMLOADSIZEMISMATCHMP3) "-h" >> sounds.h)
	$(shell $(BINTOARRAY) $(EEPROMCREATESIZEMISMATCHMP3) "-h" >> sounds.h)
	$(shell $(BINTOARRAY) $(EEPROMSTATSIZEMISMATCHMP3) "-h" >> sounds.h)
	$(shell $(BINTOARRAY) $(EEPROMLOADFAILEDMP3) "-h" >> sounds.h)
	$(shell $(BINTOARRAY) $(FLASHCREATESIZEMISMATCHMP3) "-h" >> sounds.h)
	$(shell $(BINTOARRAY) $(FLASHFSTATSIZEMISMATCHMP3) "-h" >> sounds.h)
	$(shell $(BINTOARRAY) $(FLASHLOADFAILEDMP3) "-h" >> sounds.h)

.PHONY: all generate_data

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) --remove-section *qspifl* $< $(BUILD_DIR)/$(TARGET)_fw.bin
	$(BIN) --only-section *qspifl* $< $(BUILD_DIR)/$(TARGET)_data.bin
