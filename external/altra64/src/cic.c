//
// si/cic.c: PIF CIC security/lock out algorithms.
//
// CEN64: Cycle-Accurate Nintendo 64 Emulator.
// Copyright (C) 2015, Tyler J. Stachecki.
//
// This file is subject to the terms and conditions defined in
// 'LICENSE', which is part of this source code package.
//

//#include "common.h"
//#include "si/cic.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <libdragon.h>
#include "regsinternal.h"
#include "cic.h"
#include "types.h"
#include "rom.h" //TODO: perhaps the pifram defines should be global



// CIC seeds and status bits passed from PIF to IPL through PIF RAM
// Bits     | Description
// 00080000 | ROM type (0 = Game Pack, 1 = DD)
// 00040000 | Version
// 00020000 | Reset Type (0 = cold reset, 1 = NMI)
// 0000FF00 | CIC IPL3 seed value
// 000000FF | CIC IPL2 seed value
// #define CIC_SEED_NUS_5101 0x0000AC00U
// #define CIC_SEED_NUS_6101 0x00043F3FU
// #define CIC_SEED_NUS_6102 0x00003F3FU
// #define CIC_SEED_NUS_6103 0x0000783FU
// #define CIC_SEED_NUS_6105 0x0000913FU
// #define CIC_SEED_NUS_6106 0x0000853FU
// #define CIC_SEED_NUS_8303 0x0000DD00U

#define CRC_NUS_5101 0x587BD543U
#define CRC_NUS_6101 0x6170A4A1U
#define CRC_NUS_7102 0x009E9EA3U
#define CRC_NUS_6102 0x90BB6CB5U
#define CRC_NUS_6103 0x0B050EE0U
#define CRC_NUS_6105 0x98BC2C86U
#define CRC_NUS_6106 0xACC8580AU
#define CRC_NUS_8303 0x0E018159U

static uint32_t si_crc32(const uint8_t *data, size_t size);

// Determines the CIC seed for a cart, given the ROM data.
//int get_cic_seed(const uint8_t *rom_data, uint32_t *cic_seed) {
int get_cic(unsigned char *rom_data) {
    uint32_t crc = si_crc32(rom_data + 0x40, 0x1000 - 0x40);
    uint32_t aleck64crc = si_crc32(rom_data + 0x40, 0xC00 - 0x40);

    if (aleck64crc == CRC_NUS_5101) 
        return 4;//*cic_seed = CIC_SEED_NUS_5101;
    else 
    {
        switch (crc) {
        case CRC_NUS_6101:
        case CRC_NUS_7102:
        //*cic_seed = CIC_SEED_NUS_6101;
        return 1;
        break;

        case CRC_NUS_6102:
        //*cic_seed = CIC_SEED_NUS_6102;
        return 2;
        break;

        case CRC_NUS_6103:
        //*cic_seed = CIC_SEED_NUS_6103;
        return 3;
        break;

        case CRC_NUS_6105:
        //*cic_seed = CIC_SEED_NUS_6105;
        return 5;
        break;

        case CRC_NUS_6106:
        //*cic_seed = CIC_SEED_NUS_6106;
        return 6;
        break;

        //case CRC_NUS_8303: //not sure if this is necessary as we are using cart conversions
        //*cic_seed = CIC_SEED_NUS_8303;
        //return 7;
        //break;

        default:
        break;
        }
    }
    return 2;
}

uint32_t si_crc32(const uint8_t *data, size_t size) {
  uint32_t table[256];
  unsigned n, k;
  uint32_t c;

  for (n = 0; n < 256; n++) {
    c = (uint32_t) n;

    for (k = 0; k < 8; k++) {
      if (c & 1)
        c = 0xEDB88320L ^ (c >> 1);
      else
        c = c >> 1;
    }

    table[n] = c;
  }

  c = 0L ^ 0xFFFFFFFF;

  for (n = 0; n < size; n++)
    c = table[(c ^ data[n]) & 0xFF] ^ (c >> 8);

  return c ^ 0xFFFFFFFF;
}

static volatile struct SI_regs_s * const SI_regs = (struct SI_regs_s *) 0xa4800000;
static void * const PIF_RAM = (void *) 0x1fc007c0;

/** @brief SI DMA busy */
#define SI_STATUS_DMA_BUSY  ( 1 << 0 )
/** @brief SI IO busy */
#define SI_STATUS_IO_BUSY   ( 1 << 1 )

static void __SI_DMA_wait(void) {
    while (SI_regs->status & (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY));
}

static void __controller_exec_PIF(void *inblock, void *outblock) {
  volatile uint64_t inblock_temp[8];
  volatile uint64_t outblock_temp[8];

  data_cache_hit_writeback_invalidate(inblock_temp, 64);
  memcpy(UncachedAddr(inblock_temp), inblock, 64);

  /* Be sure another thread doesn't get into a resource fight */
  disable_interrupts();

  __SI_DMA_wait();

  SI_regs->DRAM_addr = inblock_temp; // only cares about 23:0
  MEMORY_BARRIER();
  SI_regs->PIF_addr_write = PIF_RAM; // is it really ever anything else?
  MEMORY_BARRIER();

  __SI_DMA_wait();

  data_cache_hit_writeback_invalidate(outblock_temp, 64);

  SI_regs->DRAM_addr = outblock_temp;
  MEMORY_BARRIER();
  SI_regs->PIF_addr_read = PIF_RAM;
  MEMORY_BARRIER();

  __SI_DMA_wait();

  /* Now that we've copied, its safe to let other threads go */
  enable_interrupts();

  memcpy(outblock, UncachedAddr(outblock_temp), 64);
}

int pifram_x105_response_test() {
  
      static unsigned long long SI_eeprom_read_block[8] = {
      0xFFFFFFFFFFFFFFFF,
      0xFFFFFFFFFFFFFFFF,
      0xFFFFFFFFFFFFFFFF,
      0xFFFFFFFFFFFFFFFF,
      0xFFFFFFFFFFFFFFFF,
      0xFFFFFFFFFFFF0F0F,
      0x8B00620018000600,
      0x0100C000B0000002 //0x3f=02
  
      };
      static unsigned long long output[8];
  
      __controller_exec_PIF(SI_eeprom_read_block,output);
  
      /*
        expected result
      FF FF FF FF FF FF FF FF
      FF FF FF FF FF FF FF FF
      FF FF FF FF FF FF FF FF
      FF FF FF FF FF FF FF FF
      FF FF FF FF FF FF FF FF
      FF FF FF FF FF FF 00 00
      3E C6 C0 4E BD 37 15 55
      5A 8C 2A 8C D3 71 71 00
        */
  
      /* We are looking for 0x55 in [6], which
       * signifies that there is an x105 present.*/
  
      if( (output[6] & 0xFF) == 0x55 )
      {
          //x105 found!
          return 1;
  
      } else {
          //x105 not found!
          return 0;
  
      }
  
  }
