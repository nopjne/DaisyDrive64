#
# Copyright (c) 2017 The Altra64 project contributors
# See LICENSE file in the project root for full license information.
#

ROOTDIR = $(N64_INST)
GCCN64PREFIX = $(ROOTDIR)/bin/mips64-elf-
CHKSUM64PATH = $(ROOTDIR)/bin/chksum64
MKDFSPATH = $(ROOTDIR)/bin/mkdfs
N64TOOL = $(ROOTDIR)/bin/n64tool

HEADERNAME = header.ed64
HEADERTITLE = "EverDrive OS"

SRCDIR = ./src
INCDIR = ./inc
RESDIR = ./res
OBJDIR = ./obj
BINDIR = ./bin
TOOLSDIR = ./tools

LINK_FLAGS = -L$(ROOTDIR)/lib -L$(ROOTDIR)/mips64-elf/lib -ldragon -lmad -lmikmod -lyaml -lc -lm -ldragonsys -lnosys $(LIBS) -Tn64.ld --gc-sections
PROG_NAME = OS64P
CFLAGS = -std=gnu99 -march=vr4300 -mtune=vr4300 -ggdb -O3 -I$(INCDIR) -I~/libyaml/include -I$(ROOTDIR)/include -I$(ROOTDIR)/mips64-elf/include -lpthread -lrt -D_REENTRANT -DUSE_TRUETYPE $(SET_DEBUG)
ASFLAGS = -mtune=vr4300 -march=vr4300
CC = $(GCCN64PREFIX)gcc
AS = $(GCCN64PREFIX)as
LD = $(GCCN64PREFIX)ld
OBJCOPY = $(GCCN64PREFIX)objcopy

SOURCES := $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

$(PROG_NAME).z64: $ $(PROG_NAME).elf $(PROG_NAME).dfs
	$(OBJCOPY) $(BINDIR)/$(PROG_NAME).elf $(BINDIR)/$(PROG_NAME).bin -O binary
	rm -f $(BINDIR)/$(PROG_NAME).z64
	$(N64TOOL) -l 4M -t $(HEADERTITLE) -h $(RESDIR)/$(HEADERNAME) -o $(BINDIR)/$(PROG_NAME).z64 $(BINDIR)/$(PROG_NAME).bin -s 1M $(BINDIR)/$(PROG_NAME).dfs
	$(CHKSUM64PATH) $(BINDIR)/$(PROG_NAME).z64

$(PROG_NAME).elf : $(OBJECTS)
	@mkdir -p $(BINDIR)
	$(LD) -o $(BINDIR)/$(PROG_NAME).elf $(OBJECTS) $(LINK_FLAGS)

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

copy: $(PROG_NAME).z64
	sh $(TOOLSDIR)/upload.sh

$(PROG_NAME).dfs:
	$(MKDFSPATH) $(BINDIR)/$(PROG_NAME).dfs $(RESDIR)/filesystem/

all: $(PROG_NAME).z64

debug: $(PROG_NAME).z64

debug: SET_DEBUG=-DDEBUG

clean:
	rm -f $(BINDIR)/*.z64 $(BINDIR)/*.elf $(OBJDIR)/*.o $(BINDIR)/*.bin $(BINDIR)/*.dfs
