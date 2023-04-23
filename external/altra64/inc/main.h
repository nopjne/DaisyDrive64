//
// Copyright (c) 2017 The Altra64 project contributors
// Portions (c) 2013 saturnu (Alt64) based on libdragon, Neo64Menu, ED64IO, libn64-hkz, libmikmod
// See LICENSE file in the project root for full license information.
//
#ifndef MAIN_H
#define MAIN_H

//TODO: these should probably be static not protos in main
void bootRom(display_context_t disp, int silent, char* filename, char* parentFilePath, uint32_t offset1, uint32_t offset2);
void loadrom(display_context_t disp, u8 *buff, int fast);

void readSDcard(display_context_t disp, char *directory);
int saveTypeToSd(display_context_t disp, char* save_filename ,int type);

void drawShortInfoBox(display_context_t disp, char* text, u8 mode);
void drawTextInput(display_context_t disp,char *msg);

//#define ishexchar(c) (((c >= '0') && (c <= '9')) || ((c >= 'A') && (c <= 'F')) || ((c >= 'a') && (c <= 'f')))

/**
 * @brief Return the uncached memory address of a cached address
 *
 * @param[in] x
 *            The cached address
 *uint32_t
 * @return The uncached address
 */
//#define UNCACHED_ADDR(x)    ((void *)(((uint32_t)(x)) | 0xA0000000))

/**
 * @brief Align a memory address to 16 byte offset
 *
 * @param[in] x
 *            Unaligned memory address
 *
 * @return An aligned address guaranteed to be >= the unaligned address
 */
//#define ALIGN_16BYTE(x)     ((void *)(((uint32_t)(x)+15) & 0xFFFFFFF0))

#endif
