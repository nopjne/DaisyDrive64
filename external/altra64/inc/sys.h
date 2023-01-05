//
// Copyright (c) 2017 The Altra64 project contributors
// Portions (c) 2011 KRIK
// See LICENSE file in the project root for full license information.
//

#ifndef _SYS_H
#define	_SYS_H

#include "types.h"


void dma_wait(void);
void dma_read_s(void * ram_address, unsigned long pi_address, unsigned long len);
void dma_write_s(const void * ram_address, unsigned long pi_address, unsigned long len);


void sleep(u32 ms);
void dma_write_sram(void* src, u32 offset, u32 size);
void dma_read_sram(void *dest, u32 offset, u32 size);
u8 getSaveType();


typedef struct SP_regs_s {
    u32 mem_addr;
    u32 dram_addr;
    u32 rd_len;
    u32 wr_len;
    u32 status;
} _SP_regs_s;

//#define SP_PC *((volatile u32 *)0xA4080000)
#define SP_IBIST_REG *((volatile u32 *)0xA4080004)

static volatile struct AI_regs_s * const AI_regs = (struct AI_regs_s *) 0xa4500000;
static volatile struct MI_regs_s * const MI_regs = (struct MI_regs_s *) 0xa4300000;
static volatile struct VI_regs_s * const VI_regs = (struct VI_regs_s *) 0xa4400000;
static volatile struct PI_regs_s * const PI_regs = (struct PI_regs_s *) 0xa4600000;
static volatile struct SP_regs_s * const SP_regs = (struct SP_regs_s *) 0xA4040000;

extern u32 native_tv_mode;

typedef struct {
    u16 sd_speed;
    u16 font_size;
    u16 tv_mode;
    u8 wall[256];
} Options_st;


extern Options_st options;
extern u32 asm_date;

#endif	/* _SYS_H */
