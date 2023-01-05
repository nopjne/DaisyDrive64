#include <stdio.h>
#include <string.h>
#include "daisy_seed.h"
#include "fatfs.h"
#include "stm32h7xx_hal.h"
#include "sys/system.h"
#include "stm32h7xx_hal_dma.h"
#include "n64common.h"
#include "daisydrive64.h"

extern "C" ITCM_FUNCTION void FlashRAMRead(void);
extern "C" ITCM_FUNCTION void FlashRAMWrite64(void);
extern "C" ITCM_FUNCTION void FlashRAMWrite0(void);
extern "C" ITCM_FUNCTION void FlashRAMWrite1(void);

DTCM_DATA uint64_t* StatusRegister = (uint64_t*)FlashRamStorage;
DTCM_DATA uint16_t* FlashRamData = (uint16_t*)FlashRamStorage;
DTCM_DATA BYTE FlashBuffer[128];
DTCM_DATA uint16_t* FlashBufferPtr;
DTCM_DATA uint16_t* FlashBufferEnd = (uint16_t*)(FlashBuffer + sizeof(FlashBuffer));

enum FLASH_STATE {
    FLASHRAM_ERASE,
    FLASHRAM_WRITE,
    FLASHRAM_STATUS,
    FLASHRAM_READ
};
DTCM_DATA FLASH_STATE FlashRamState = FLASHRAM_STATUS;

// mapped read of flashram
extern "C"
ITCM_FUNCTION
void FlashRAMReadFirst(void)
{
    EXTI->PR1 = READ_LINE;
    const uint16_t PrefetchRead = *FlashRamData;
    __DMB();
    GPIOA->ODR = PrefetchRead;
    GPIOB->ODR = (((PrefetchRead >> 4) & 0x03F0) | (PrefetchRead & 0xC000));
    SET_PI_OUTPUT_MODE
    // Change interrupt function.
    NVIC_SetVector(EXTI1_IRQn, (uint32_t)&FlashRAMRead);
    FlashRamData += 1;
}

extern "C"
ITCM_FUNCTION
void FlashRAMRead(void)
{
    EXTI->PR1 = READ_LINE;
    const uint16_t PrefetchRead = *FlashRamData;
    __DMB();
    GPIOA->ODR = PrefetchRead;
    GPIOB->ODR = (((PrefetchRead >> 4) & 0x03F0) | (PrefetchRead & 0xC000));
    FlashRamData += 1;
}

extern "C"
ITCM_FUNCTION
void FlashRAMWrite64(void)
{
    const uint32_t ValueInB = GPIOB->IDR;
    const uint32_t ValueInA = GPIOA->IDR;
    const uint16_t Input = ((ValueInB & 0x03F0) << 4) | (ValueInB & 0xC000) | (ValueInA & 0xFF);
    *FlashBufferPtr = Input;
    FlashBufferPtr += 1;
    // May not be needed.
    if (FlashBufferPtr == FlashBufferEnd) {
        NVIC_SetVector(EXTI4_IRQn, (uint32_t)&FlashRAMWrite0);
    }
}

// mapped write of flashram, commands
DTCM_DATA uint16_t FlashRamCommand = 0;
extern "C"
ITCM_FUNCTION
void FlashRAMWrite0(void)
{
    const uint32_t ValueInB = GPIOB->IDR;
    const uint32_t ValueInA = GPIOA->IDR;
    const uint16_t Input = ((ValueInB & 0x03F0) << 4) | (ValueInB & 0xC000) | (ValueInA & 0xFF);
    FlashRamCommand = Input >> 8;
    NVIC_SetVector(EXTI4_IRQn, (uint32_t)&FlashRAMWrite1);
}

extern "C"
ITCM_FUNCTION
void FlashRAMWrite1(void)
{
    const uint32_t ValueInB = GPIOB->IDR;
    const uint32_t ValueInA = GPIOA->IDR;
    const uint16_t Input = ((ValueInB & 0x03F0) << 4) | (ValueInB & 0xC000) | (ValueInA & 0xFF);
    switch (FlashRamCommand) {
        case 0x4B: // set erase offset
            FlashRamData = (uint16_t*)(((uint8_t*)FlashRamStorage + 8) + (Input * 128));
            break;

        case 0x78: // erase
            FlashRamState = FLASHRAM_ERASE;
            *StatusRegister = 0x1111800800C20000LL;
            break;

        case 0xA5: // set write offset
            FlashRamData = (uint16_t*)(((uint8_t*)FlashRamStorage + 8) + (Input * 128));
            *StatusRegister = 0x1111800400C20000LL;
            break;

        case 0xB4: // write
            FlashRamState = FLASHRAM_WRITE;
            FlashBufferPtr = (uint16_t*)FlashBuffer;
            NVIC_SetVector(EXTI4_IRQn, (uint32_t)&FlashRAMWrite64);
            break;

        case 0xD2: // execute
            // TODO this may need to be done at a lower IRQ.
            if (FlashRamState == FLASHRAM_ERASE) {
                memset(FlashRamData, 0xFF, 0x80);
            } else if (FlashRamState == FLASHRAM_WRITE) {
                memcpy(FlashRamData, FlashBuffer, 0x80);
            }

        break;

        case 0xE1: // status
            FlashRamState = FLASHRAM_STATUS;
            FlashRamData = (uint16_t*)(FlashRamStorage);
            *StatusRegister = 0x1111800100C20000LL;
            break;

        case 0xF0: // read
            FlashRamState = FLASHRAM_READ;
            FlashRamData = (uint16_t*)(FlashRamStorage + 8);
            *StatusRegister = 0x11118004F0000000LL;
            break;

        default:
        break;
    }
}