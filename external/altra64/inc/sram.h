//
// Copyright (c) 2017 The Altra64 project contributors
// Portions (c) 2013 saturnu (Alt64) based on libdragon, Neo64Menu, ED64IO, libn64-hkz, libmikmod
// See LICENSE file in the project root for full license information.
//

#ifndef _SRAM_H
#define	_SRAM_H

#include <stdlib.h>
#include "types.h"

void data_cache_hit_writeback_invalidate(volatile void *, unsigned long);
void dma_write_sram(void* src, u32 offset, u32 size);
void dma_read_sram(void *dest, u32 offset, u32 size);
void dma_write_s(const void * ram_address, unsigned long pi_address, unsigned long len);
void dma_read_s(void * ram_address, unsigned long pi_address, unsigned long len);
int writeSram(void* src, u32 size);
void setSDTiming(void);


void PI_Init(void);
void PI_Init_SRAM(void);
void PI_DMAWait(void);
void PI_DMAFromCart(void* dest, void* src, u32 size);
void PI_DMAToCart(void* dest, void* src, u32 size);
void PI_DMAFromSRAM(void *dest, u32 offset, u32 size);
void PI_DMAToSRAM(void* src, u32 offset, u32 size);
void PI_SafeDMAFromCart(void *dest, void *src, u32 size);

//memory
/*** MEMORY ***/
void *safe_memalign(size_t boundary, size_t size);
void *safe_calloc(size_t nmemb, size_t size);
void *safe_malloc(size_t size);
void safe_free(void *ptr);
void *safe_memset(void *s, int c, size_t n);
void *safe_memcpy(void *dest, const void *src, size_t n);

#endif
