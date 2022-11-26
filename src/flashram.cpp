#include <stdio.h>
#include <string.h>
#include "daisy_seed.h"
#include "fatfs.h"
#include "stm32h7xx_hal.h"
#include "sys/system.h"
#include "stm32h7xx_hal_dma.h"
#include "n64common.h"
#include "daisydrive64.h"

uint64_t* StatusRegister = (uint64_t*)FlashRamStorage;

// mapped read of flashram
int read_flashram(void *opaque, uint32_t address, uint32_t *word)
{
  if (pi->flashram.data == NULL)
    return -1;

  *word = pi->flashram.status >> 32;
  return 0;
}

// mapped write of flashram, commands
extern "C"
ITCM_FUNCTION
void FlashRAMProcessCommand(void)
{
  if (pi->flashram.data == NULL) {
    debug("write to FlashRAM but no FlashRAM present\n");
    return 1;
  }

  if (address == 0x08000000) {
    debug("write to flash status, ignored");
    return 0;
  }

  switch (word >> 24) {
    case 0x4B: // set erase offset
      pi->flashram.offset = (word & 0xFFFF) * 128;
      break;

    case 0x78: // erase
      pi->flashram.mode = FLASHRAM_ERASE;
      *StatusRegister = 0x1111800800C20000LL;
      break;

    case 0xA5: // set write offset
      pi->flashram.offset = (word & 0xFFFF) * 128;
      *StatusRegister = 0x1111800400C20000LL;
      break;

    case 0xB4: // write
      pi->flashram.mode = FLASHRAM_WRITE;
      break;

    case 0xD2: // execute
        // TODO bounds checks
        if (pi->flashram.mode == FLASHRAM_ERASE)
            memset(pi->flashram.data + pi->flashram.offset, 0xFF, 0x80);

        else if (pi->flashram.mode == FLASHRAM_WRITE)
            memcpy(pi->flashram.data + pi->flashram.offset,
                pi->bus->ri->ram + pi->flashram.rdram_pointer, 0x80);

      break;

    case 0xE1: // status
        pi->flashram.mode = FLASHRAM_STATUS;
        *StatusRegister = 0x1111800100C20000LL;
        break;

    case 0xF0: // read
        pi->flashram.mode = FLASHRAM_READ;
        *StatusRegister = 0x11118004F0000000LL;
        break;

    default:
  }

  return 0;
}