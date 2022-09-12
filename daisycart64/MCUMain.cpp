#define DEBUG_DEFAULT_INTERRUPT_HANDLERS 1
#include <stdio.h>
#include <string.h>
#include "daisy_seed.h"
#include "fatfs.h"
#include "stm32h7xx_hal.h"
#include "sys/system.h"
#include "stm32h7xx_hal_dma.h"

#ifndef _FS_EXFAT
#error FAT FS NEEDS TO BE ENABLED
#endif

extern BYTE EEPROMStore[2048]; // 16KiBit

__IO   uint32_t DMA_TransferErrorFlag = 0;

#define OVERCLOCK 1
#define READ_DELAY_NS 1000
//#define READ_DELAY_NS 500
#define DTCM_DATA  __attribute__((section(".dtcmram_bss")))
DTCM_DATA DMA_HandleTypeDef DMA_Handle_Channel0;
DTCM_DATA DMA_HandleTypeDef DMA_Handle_Channel1;

//#define MENU_ROM_FILE_NAME "menu.z64"
//#define MENU_ROM_FILE_NAME "Super Mario 64 (USA).n64" // Works well.
//#define MENU_ROM_FILE_NAME "Resident Evil 2 (USA).n64" // Does not boot may require CIC communication.
//#define MENU_ROM_FILE_NAME "Mario Tennis (USA).n64" // Lots of audio glitches.
//#define MENU_ROM_FILE_NAME "Legend of Zelda, The - Ocarina of Time - Master Quest (USA) (Debug Version).n64" // Requires CIC communication
//#define MENU_ROM_FILE_NAME "Killer Instinct Gold (USA).n64" // Audio glitches
//#define MENU_ROM_FILE_NAME "Mortal Kombat Trilogy (USA).n64" // Hangs at intro.
//#define MENU_ROM_FILE_NAME "Legend of Zelda, The - Majora's Mask (USA).n64" // Requires CIC communication.
//#define MENU_ROM_FILE_NAME "Mario Kart 64 (USA).n64" // Works mild audio glitches.

const char* RomName[] = {
    //"Mario Kart 64 (USA).n64",
    "Super Mario 64 (USA).n64",
    //"Mario Tennis (USA).n64", // Works on SPEED=4000 lots of audio glitches.
    //"Killer Instinct Gold (USA).n64",
    //"Mortal Kombat Trilogy (USA).n64", // Gets in menu on SPEED=1000, freezes in game.
    //"Resident Evil 2 (USA).n64", 
    //"007 - GoldenEye (USA).n64",
    //"007 - The World Is Not Enough (USA).n64", 
    //"Mario Golf (USA).n64", // Does not work.
    //"Mario Tennis 64 (Japan).n64", // Works audio glitches.
    //"Mortal Kombat 4 (USA).n64", // Works - audio glitches.
    //"Mortal Kombat Mythologies - Sub-Zero (USA).n64", // Works.
    //"Perfect Dark (USA) (Rev A).n64", // Needs different CIC chip.
    //"Star Fox 64 (Japan).n64", // Needs different CIC chip.
    //"Star Fox 64 (USA).n64", // Needs different CIC chip/
};

#define N64_ROM_BASE 0x10000000
#define CART_DOM2_ADDR1_START     0x05000000
#define CART_DOM2_ADDR1_END       0x05FFFFFF

// N64DD IPL ROM
#define CART_DOM1_ADDR1_START     0x06000000
#define CART_DOM1_ADDR1_END       0x07FFFFFF

// Cartridge SRAM
#define CART_DOM2_ADDR2_START     0x08000000
#define CART_DOM2_ADDR2_END       0x0FFFFFFF

using namespace daisy;
BYTE *Sram4Buffer = (BYTE*)0x38000000;
int8_t *ram = (int8_t *)0xC0000000;
uint32_t *LogBuffer = (uint32_t*)(ram + (48*1024*1024));
uint32_t *PortABuffer = (uint32_t*)Sram4Buffer;
uint32_t *PortBBuffer = (uint32_t*)(Sram4Buffer + 16);
uint32_t MaxSize = 0;

static DaisySeed hw;

SdmmcHandler   sd;
FatFSInterface fsi;
FIL            SDFile;

void BlinkAndDie(int wait1, int wait2)
{
    while(1) {
        GPIOC->BSRR = (0x1 << 7);
        System::Delay(wait1);
        GPIOC->BSRR = (0x1 << 7) << 16;
        System::Delay(wait2);
    }
}

#define GP_SPEED GPIO_SPEED_FREQ_LOW
DTCM_DATA volatile uint32_t ADInputAddress = 0;
DTCM_DATA volatile uint32_t PrefetchRead = 0;
DTCM_DATA volatile uint32_t ReadOffset = 0;
static void HAL_TransferError(DMA_HandleTypeDef *hdma);
static void Error_Handler(void);

int InitializeDmaChannels(void)
{
    HAL_DMA_MuxRequestGeneratorConfigTypeDef dmamux_ReqGenParams  = {0};
    __HAL_RCC_BDMA_CLK_ENABLE();

    {
        DMA_Handle_Channel0.Instance                 = BDMA_Channel0;
        DMA_Handle_Channel0.Init.Request             = BDMA_REQUEST_GENERATOR0;
        DMA_Handle_Channel0.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        DMA_Handle_Channel0.Init.PeriphInc           = DMA_PINC_DISABLE;
        DMA_Handle_Channel0.Init.MemInc              = DMA_MINC_ENABLE;
        DMA_Handle_Channel0.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
        DMA_Handle_Channel0.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
        DMA_Handle_Channel0.Init.Mode                = DMA_CIRCULAR;
        DMA_Handle_Channel0.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
        DMA_Handle_Channel0.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
        DMA_Handle_Channel0.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_1QUARTERFULL;
        DMA_Handle_Channel0.Init.MemBurst            = DMA_MBURST_SINGLE;
        DMA_Handle_Channel0.Init.PeriphBurst         = DMA_PBURST_SINGLE;

        // Initialize the DMA for Transmission.
        HAL_StatusTypeDef dmares = HAL_OK;
        dmares = HAL_DMA_Init(&DMA_Handle_Channel0);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        // Select Callbacks functions called after Transfer complete and Transfer error.
        dmares = HAL_DMA_RegisterCallback(&DMA_Handle_Channel0, HAL_DMA_XFER_CPLT_CB_ID, NULL);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        dmares = HAL_DMA_RegisterCallback(&DMA_Handle_Channel0, HAL_DMA_XFER_ERROR_CB_ID, HAL_TransferError);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        // NVIC configuration for DMA transfer complete interrupt.
        HAL_NVIC_SetPriority(BDMA_Channel0_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(BDMA_Channel0_IRQn);

        // Configure and enable the DMAMUX Request generator.
        dmamux_ReqGenParams.SignalID  = HAL_DMAMUX2_REQ_GEN_EXTI0;
        dmamux_ReqGenParams.Polarity  = HAL_DMAMUX_REQ_GEN_RISING_FALLING;
        dmamux_ReqGenParams.RequestNumber = 1;

        dmares = HAL_DMAEx_ConfigMuxRequestGenerator(&DMA_Handle_Channel0, &dmamux_ReqGenParams);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        // NVIC configuration for DMAMUX request generator overrun errors
        HAL_NVIC_SetPriority(DMAMUX2_OVR_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(DMAMUX2_OVR_IRQn);

        dmares = HAL_DMAEx_EnableMuxRequestGenerator (&DMA_Handle_Channel0);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        dmares = HAL_DMA_Start_IT(&DMA_Handle_Channel0, (uint32_t)&(GPIOB->IDR), (uint32_t)(PortBBuffer), 2);
        if (dmares != HAL_OK) {
            Error_Handler();
        }
    }

    {
        DMA_Handle_Channel1.Instance                 = BDMA_Channel1;
        DMA_Handle_Channel1.Init.Request             = BDMA_REQUEST_GENERATOR0;
        DMA_Handle_Channel1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        DMA_Handle_Channel1.Init.PeriphInc           = DMA_PINC_DISABLE;
        DMA_Handle_Channel1.Init.MemInc              = DMA_MINC_ENABLE;
        DMA_Handle_Channel1.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        DMA_Handle_Channel1.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
        DMA_Handle_Channel1.Init.Mode                = DMA_CIRCULAR;
        DMA_Handle_Channel1.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
        DMA_Handle_Channel1.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
        DMA_Handle_Channel1.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_1QUARTERFULL;
        DMA_Handle_Channel1.Init.MemBurst            = DMA_MBURST_SINGLE;
        DMA_Handle_Channel1.Init.PeriphBurst         = DMA_PBURST_SINGLE;

        // Call into the HAL DMA init, this is not a hot path so hal is uzed.
        HAL_StatusTypeDef dmares = HAL_OK;
        dmares = HAL_DMA_Init(&DMA_Handle_Channel1);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        // Select Callbacks functions called after Transfer complete and Transfer error
        dmares = HAL_DMA_RegisterCallback(&DMA_Handle_Channel1, HAL_DMA_XFER_CPLT_CB_ID, NULL);
        if (dmares != HAL_OK) {
            Error_Handler();
        }
        dmares = HAL_DMA_RegisterCallback(&DMA_Handle_Channel1, HAL_DMA_XFER_ERROR_CB_ID, HAL_TransferError);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        // NVIC configuration for DMA transfer complete interrupt.
        HAL_NVIC_SetPriority(BDMA_Channel1_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(BDMA_Channel1_IRQn);

        // Configure and enable the DMAMUX Request generator.
        dmamux_ReqGenParams.SignalID  = HAL_DMAMUX2_REQ_GEN_EXTI0;
        dmamux_ReqGenParams.Polarity  = HAL_DMAMUX_REQ_GEN_RISING_FALLING;
        dmamux_ReqGenParams.RequestNumber = 1;

        dmares = HAL_DMAEx_ConfigMuxRequestGenerator(&DMA_Handle_Channel1, &dmamux_ReqGenParams);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        // NVIC configuration for DMAMUX request generator overrun errors.
        HAL_NVIC_SetPriority(DMAMUX2_OVR_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(DMAMUX2_OVR_IRQn);

        dmares = HAL_DMAEx_EnableMuxRequestGenerator (&DMA_Handle_Channel1);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        dmares = HAL_DMA_Start_IT(&DMA_Handle_Channel1, (uint32_t)&(GPIOA->IDR), (uint32_t)(PortABuffer), 2);
        if (dmares != HAL_OK) {
            Error_Handler();
        }
    }

    return 0;
}

static void HAL_TransferError(DMA_HandleTypeDef *hdma)
{
  DMA_TransferErrorFlag = 1;
}

static void Error_Handler(void)
{
    GPIOC->BSRR = (0x1 << 7);
  while(1)
  {
  }
}

#define ALE_L (1 << 0)
#define READ_LINE (1 << 1)
#define ALE_H (1 << 12)
#define RESET_LINE (1 << 11)

#define ALE_H_IS_HIGH ((GPIOD->IDR & ALE_H) != 0)
#define ALE_H_IS_LOW ((GPIOD->IDR & ALE_H) == 0)
#define ALE_L_IS_HIGH ((GPIOC->IDR & ALE_L) != 0)
#define ALE_L_IS_LOW ((GPIOC->IDR & ALE_L) == 0)
#define READ_IS_HIGH ((GPIOC->IDR & READ_LINE) != 0)
#define READ_IS_LOW ((GPIOC->IDR & READ_LINE) == 0)

typedef struct
{
  __IO uint32_t ISR;   /*!< BDMA interrupt status register */
  __IO uint32_t IFCR;  /*!< BDMA interrupt flag clear register */
} BDMA_Base_Registers;
const uint32_t StreamBaseAddress = (0x58025400UL);

DTCM_DATA volatile uint32_t DMACount = 0;
DTCM_DATA volatile uint32_t ALE_H_Count = 0;
DTCM_DATA volatile uint32_t IntCount = 0;
DTCM_DATA volatile bool Running = false;

#define ITCM_FUNCTION __attribute__((long_call, section(".itcm_text")))
extern "C"
ITCM_FUNCTION
void EXTI15_10_IRQHandler(void)
{
    if ((EXTI->PR1 & RESET_LINE) != 0) {
        // RESET
        EXTI->PR1 = RESET_LINE;
        // Switch to output.
        GPIOA->MODER = 0xABFF5555;
        GPIOB->MODER = 0x5CF555B3;
        GPIOA->ODR = 0;
        GPIOB->ODR = 0;
        GPIOB->MODER = 0x0CF000B3;
        GPIOA->MODER = 0xABFF0000;
        DMACount = 0;
        IntCount = 0;
        ALE_H_Count = 0;
        ADInputAddress = 0;
        Running = false;
        //HAL_NVIC_SystemReset();
        return;
    }

    if ((EXTI->PR1 & ALE_H) == 0) {
        return;
    }

    EXTI->PR1 = ALE_H;
    GPIOB->MODER = 0x0CF000B3;
    GPIOA->MODER = 0xABFF0000;
    ALE_H_Count += 1;
}

extern "C"
ITCM_FUNCTION
void EXTI1_IRQHandler(void)
{
    EXTI->PR1 = READ_LINE;
    if (Running == false) {
        return;
    }

#if (READ_DELAY_NS >= 500)
    if ((ReadOffset & 2) == 0) {
        LogBuffer[IntCount] = ADInputAddress + (ReadOffset & 511);
    } else {
        LogBuffer[IntCount] = PrefetchRead;
    }
#endif

    if ((ReadOffset & 3) == 0) {
        if ((ADInputAddress >= N64_ROM_BASE) && (ADInputAddress <= (N64_ROM_BASE + MaxSize))) {
            PrefetchRead = *((uint32_t*)(ram + (ADInputAddress - N64_ROM_BASE) + (ReadOffset & 511)));
        } else {
            if (ADInputAddress >= CART_DOM2_ADDR1_START && ADInputAddress <= CART_DOM2_ADDR1_END) {
                PrefetchRead = 0;
            } else if (ADInputAddress >= CART_DOM1_ADDR1_START && ADInputAddress <= CART_DOM1_ADDR1_END) {
                PrefetchRead = 0;
            } else if (ADInputAddress >= CART_DOM2_ADDR2_START && ADInputAddress <= CART_DOM2_ADDR2_END) {
                PrefetchRead = 0;
            } else if ( (ADInputAddress <= MaxSize) ) { // HACK: Getting addresses that are missing the high byte, happens often.
                // When this issue occurs attempt to return something, this seems to stabilize the games where they run longer without freezing.
                // Glitches and static can still be seen/heard while playing.
                PrefetchRead = *((uint32_t*)(ram + ADInputAddress + (ReadOffset & 511)));
            } else {
                PrefetchRead = 0;
            }
        }
        // Switch to output.
        // TODO: Can this be done through DMA entirely?
        GPIOA->MODER = 0xABFF5555;
        GPIOB->MODER = 0x5CF555B3;
    }

    // TODO: Value can be DMA-ed directly from ram.
    // PortB upper bits can be used as 0xF0 and port B lower 2 bits can be used as 0x300
    // Then PortA needs to hold the lower-upper 2bits on 10 and 11. Needs 3 DMA channels to realize.
    // And need soldering to USB_OTG_FS_ID and USB_OTG_FS_D_-
    uint32_t Value = (((ReadOffset & 2) == 0) ? PrefetchRead : (PrefetchRead >> 16));
    uint32_t OutB = (((Value >> 4) & 0x03F0) | (Value & 0xC000));
    GPIOA->ODR = Value;
    GPIOB->ODR = OutB;

    ReadOffset += 2;
    IntCount += 1;

    if ((ReadOffset % 4) == 0 && (IntCount != 2)) {
#if OVERCLOCK
#if (READ_DELAY_NS == 4000)
        volatile uint32_t x = 266; // Calculated to offset to ~3964ns. At 540Mhz core (270mhz PLL)
#elif (READ_DELAY_NS == 2000)
        volatile uint32_t x = 128;
#elif (READ_DELAY_NS == 1000)
        volatile uint32_t x = 59;
#elif (READ_DELAY_NS == 500)
        volatile uint32_t x = 26;
#else
    uint32_t x = 0;
#endif
#else
        volatile uint32_t x = 196;
#endif

        while (x) { x--; }
        // GPIOA->ODR = 0xFF;
        // GPIOB->ODR = 0xFFFF;
        GPIOA->ODR = 0x00;
        GPIOB->ODR = 0x0000;
        GPIOA->MODER = 0xABFF0000;
        GPIOB->MODER = 0x0CF000B3;
    }

    //if (IntCount >= (((64 - 48) * 1024 * 1024) / 4)) {
    if (IntCount >= 3) {
        IntCount = 3;
    }

#if 0 // Debugging
    if (IntCount == 565966) {
         LogBuffer[0] = (((BDMA_Base_Registers *)(DMA_Handle_Channel1.StreamBaseAddress))->ISR);
         volatile uint32_t x = *((uint32_t*)0xD1ED1ED1);
         x += 1;
    }
#endif
}

inline void ConstructAddress(void)
{
    SCB_InvalidateDCache_by_Addr(PortABuffer, 16);
    // Construct ADInputAddress
    ADInputAddress = (PortABuffer[1] & 0xFF) | ((PortBBuffer[1] & 0x03F0) << 4) | (PortBBuffer[1] & 0xC000) |
                    (((PortABuffer[0] & 0xFF) | ((PortBBuffer[0] & 0x03F0) << 4) | (PortBBuffer[0] & 0xC000)) << 16);

    ReadOffset = 0;
    if (ADInputAddress == N64_ROM_BASE) {
        IntCount = 0;
    }
}

extern "C"
ITCM_FUNCTION
void BDMA_Channel1_IRQHandler(void)
{
    if (((BDMA_Base_Registers *)(DMA_Handle_Channel1.StreamBaseAddress))->ISR == 0) {
        return;
    }

#if HALT_ON_DMA_COMPLETE_UNKNOWN
    if ((((BDMA_Base_Registers *)(DMA_Handle_Channel1.StreamBaseAddress))->ISR != 0x70) && (((BDMA_Base_Registers *)(DMA_Handle_Channel1.StreamBaseAddress))->ISR != 0x77) && (((BDMA_Base_Registers *)(DMA_Handle_Channel1.StreamBaseAddress))->ISR != 0x75)) {
        LogBuffer[0] = (((BDMA_Base_Registers *)(DMA_Handle_Channel1.StreamBaseAddress))->ISR);
        volatile uint32_t x = *((uint32_t*)0xD1ED1ED1);
        x += 1;
    }
#endif

    ((BDMA_Base_Registers *)(DMA_Handle_Channel1.StreamBaseAddress))->IFCR = 1 << 4;
    const uint32_t DoOp = (DMACount += 1);
    if ((DoOp & 1) == 0) {
        ConstructAddress();
    }
}

extern "C"
ITCM_FUNCTION
void BDMA_Channel0_IRQHandler(void)
{
    if (((BDMA_Base_Registers *)(DMA_Handle_Channel1.StreamBaseAddress))->ISR == 0) {
        return;
    }

#if HALT_ON_DMA_COMPLETE_UNKNOWN
    if ((((BDMA_Base_Registers *)(DMA_Handle_Channel1.StreamBaseAddress))->ISR != 0x70) && (((BDMA_Base_Registers *)(DMA_Handle_Channel1.StreamBaseAddress))->ISR != 0x77) && (((BDMA_Base_Registers *)(DMA_Handle_Channel1.StreamBaseAddress))->ISR != 0x57)) {
        LogBuffer[0] = (((BDMA_Base_Registers *)(DMA_Handle_Channel1.StreamBaseAddress))->ISR);
        volatile uint32_t x = *((uint32_t*)0xD1ED1ED1);
        x += 1;
    }
#endif

    ((BDMA_Base_Registers *)(DMA_Handle_Channel0.StreamBaseAddress))->IFCR = 1;
    const uint32_t DoOp = (DMACount += 1);
    if ((DoOp & 1) == 0) {
        ConstructAddress();
    }
}

void InitializeInterrupts(void)
{
    // Disable systick interrupts. (Needs to be re-enabled for SDCard reads)
    InitializeDmaChannels();

    GPIO_InitTypeDef GPIO_InitStruct;
    // ALEH interrupt setup. -- 
    // This interrupt is used to indicate that ALE_H high, and the mode needs to switch to input.
    GPIO_InitStruct = {ALE_H, GPIO_MODE_IT_RISING, GPIO_NOPULL, GP_SPEED, 0};
    NVIC_SetVector(EXTI15_10_IRQn, (uint32_t)&EXTI15_10_IRQHandler);
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

    // ALEL interrupt setup. Needs to cause a DMA transaction. From Perih to Memory.
    GPIO_InitStruct = {ALE_L, GPIO_MODE_IT_RISING, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct = {READ_LINE, GPIO_MODE_IT_FALLING, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
    NVIC_SetVector(EXTI1_IRQn, (uint32_t)&EXTI1_IRQHandler);
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);

    // Reset line setup
    GPIO_InitStruct = {RESET_LINE, GPIO_MODE_IT_FALLING, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
}

void LoadRom(const char* Name)
{
    GPIOC->BSRR = (0x1 << 7);

    size_t bytesread = 0;
    SdmmcHandler::Config sd_cfg;
    sd_cfg.Defaults();
    sd_cfg.speed = SdmmcHandler::Speed::STANDARD;
    {
        sd.Init(sd_cfg);

        // Links libdaisy i/o to fatfs driver.
        fsi.Init(FatFSInterface::Config::MEDIA_SD);

        // Mount SD Card
        if (f_mount(&fsi.GetSDFileSystem(), "/", 1) != FR_OK) {
            BlinkAndDie(500, 100);
        }

        // Open the eeprom save file for the requested rom.
        char SaveName[265];
        sprintf(SaveName, "%s.eep", Name);
        if(f_open(&SDFile, SaveName, FA_READ) != FR_OK) {
            if (f_open(&SDFile, SaveName, (FA_CREATE_ALWAYS) | (FA_WRITE)) == FR_OK) {
                memset(EEPROMStore, 0, sizeof(EEPROMStore));
                UINT byteswritten;
                f_write(&SDFile, EEPROMStore, sizeof(EEPROMStore), &byteswritten);
                f_close(&SDFile);

                if (byteswritten != sizeof(EEPROMStore)) {
                    // Let the user know something went wrong.
                    BlinkAndDie(1000, 500);
                }
            }

        } else {
            FILINFO FileInfo;
            FRESULT result = f_stat(SaveName, &FileInfo);
            if (FileInfo.fsize != sizeof(EEPROMStore)) {
                BlinkAndDie(200, 300);
            }

            result = f_read(&SDFile, EEPROMStore, FileInfo.fsize, &bytesread);
            if (result != FR_OK) {
                BlinkAndDie(200, 300);
            }

            f_close(&SDFile);
        }

        // TODO: Open the FLASH ram file for the requested rom. (Need to decide where this will live when 64MB roms are loaded)

        // Read requested rom from the SD Card.
        if(f_open(&SDFile, Name, FA_READ) == FR_OK) {
            FILINFO FileInfo;
            FRESULT result = f_stat(Name, &FileInfo);
            if (result != FR_OK) {
                BlinkAndDie(100, 500);
            }

            result = f_read(&SDFile, ram, FileInfo.fsize, &bytesread);
            if (result != FR_OK) {
                BlinkAndDie(200, 200);
            }

            f_close(&SDFile);

            // Blink led on error.
            if (bytesread != FileInfo.fsize) {
                BlinkAndDie(500, 500);
            }

            MaxSize = bytesread;

        } else {
            BlinkAndDie(100, 100);
        }
    }
    // No led on on success.
    GPIOC->BSRR = (0x1 << 7) << 16;

    // Patch speed.
#if (READ_DELAY_NS == 4000)
    *(ram + 3) =  0xFF;
#elif (READ_DELAY_NS == 2000)
    *(ram + 3) =  0x80;
#elif (READ_DELAY_NS == 1000)
    *(ram + 3) =  0x40;
#elif (READ_DELAY_NS == 500)
    *(ram + 3) =  0x20;
#endif
}


#define SI_RESET 0xFF // Tx:1 Rx:3
#define SI_INFO  0x00 // Tx:1 Rx:3
#define EEPROM_READ 0x04 // Tx:2 Rx:8
#define EEPROM_STORE  0x05 // Tx:10 Rx:1 

#define S_DAT_LINE (1 << 4)

uint32_t SDATLog[2048];
uint32_t TransfersHappened = 0;
// 0  = 0,0,0,1
// 1  = 0,1,1,1
// DS = 0,0,1,1
// CS = 0,1,1 
ITCM_FUNCTION
bool SIGetByte(BYTE &Out, bool Block)
{
    volatile uint32_t TimeStamp;
    volatile uint32_t OldTimeStamp;
    volatile BYTE In = 1;

    uint32_t TransferTime = 544; // 540 * 1000 * 1000 / 1000 * 1000 = 1us.
    volatile BYTE BitCount = 0;
    Out = 0;

    // wait for sdat low
    while (((GPIOC->IDR & (S_DAT_LINE)) != 0)) {}

    TimeStamp = DWT->CYCCNT;
    OldTimeStamp = TimeStamp - TransferTime;
    while ((Running != false) && (BitCount < 8)) {
        TimeStamp = DWT->CYCCNT;
        volatile uint32_t OverTime = (TimeStamp - OldTimeStamp);
        if (OverTime >= TransferTime) {
            In <<= 1;
            In |= ((GPIOC->IDR & (S_DAT_LINE)) != 0) ? 1 : 0;
            SDATLog[TransfersHappened] = In | (Out << 8);
            TransfersHappened += 1;
            // 0b0001
            // 0b0010
            // 0b0100
            // 0b10001 0x11
            // 0b10111 0x17
            // 0b10111 0x17 CS

            if ((In & (0x10)) != 0) {
                Out <<= 1;
                Out |= ((In == 0x17) ? 1 : 0);
                BitCount += 1;
                In = 1;
            }

            OldTimeStamp = TimeStamp - (OverTime - TransferTime);
        }
    }

    return true;
}

ITCM_FUNCTION
bool SIGetConsoleTerminator(void)
{
    volatile uint32_t TimeStamp;
    volatile uint32_t OldTimeStamp;
    const uint32_t TransferTime = 560; // 540 * 1000 * 1000 / 1000 * 1000 = 1us.
    // Skip the terminator for now.
    TimeStamp = OldTimeStamp = DWT->CYCCNT;
    while ((Running != false) && ((TimeStamp - OldTimeStamp) < (TransferTime * 3))) {
        TimeStamp = DWT->CYCCNT;
    }

    return true;
}

volatile uint32_t OldTimeStamp;
ITCM_FUNCTION
bool SIPutDeviceTerminator(void)
{
    const BYTE SIDeviceTerminator = 0xC;
    volatile uint32_t TimeStamp;
    BYTE Out = SIDeviceTerminator;

    const uint32_t TransferTime = 555; // 540 * 1000 * 1000 / 1000 * 1000 = 1us.
    //OldTimeStamp = DWT->CYCCNT;
    while ((Running != false) && (Out != 0)) {
        TimeStamp = DWT->CYCCNT;
        volatile uint32_t OverTime = (TimeStamp - OldTimeStamp);
        if (OverTime > TransferTime) {
            GPIOC->BSRR = (((Out & 1) != 0) ? S_DAT_LINE : (S_DAT_LINE << 16));
            Out >>= 1;
            OldTimeStamp = (TimeStamp - (OverTime - TransferTime));
        }
    }

    GPIOC->BSRR = S_DAT_LINE; // Set line to high to enable input.

    return true;
}


ITCM_FUNCTION
bool SIPutByte(BYTE In)
{
    const BYTE SIZero = 0x8;
    const BYTE SIOne = 0xE;
    volatile uint32_t TimeStamp;
    
    BYTE Out = 0;

    const uint32_t TransferTime = 555; // 540 * 1000 * 1000 / 1000 * 1000 = 1us.
    uint BitCount = 0;
    //OldTimeStamp = DWT->CYCCNT;
    while ((Running != false) && (BitCount <= 8)) {
        if (Out == 0) {
            Out = ((In & 1) != 0) ? SIOne : SIZero;
            In >>= 1;
            BitCount += 1;
        }

        TimeStamp = DWT->CYCCNT;
        volatile uint32_t OverTime = (TimeStamp - OldTimeStamp);
        if (OverTime > TransferTime) {
            GPIOC->BSRR = (((Out & 1) != 0) ? S_DAT_LINE : (S_DAT_LINE << 16));
            Out >>= 1;
            OldTimeStamp = (TimeStamp - (OverTime - TransferTime));
        }
    }

    return true;
}

BYTE EEPROMStore[2048]; // 16KiBit
ITCM_FUNCTION
void RunEEPROMEmulator(void)
{
    static BYTE Command = 0;

    // Check state 
    SIGetByte(Command, true);
    SIGetConsoleTerminator();

    if (Running == false) {
        return;
    }

    // 
    if ((Command == SI_RESET) || (Command == SI_INFO)) {
        // Return the info on either 4KiBit or 16KiBit EEPROM, it should be safe to always return 16kbit.
        // 0x0080	N64	4 Kbit EEPROM	Bitfield: 0x80=Write in progress
        // 0x00C0	N64	16 Kbit EEPROM	Bitfield: 0x80=Write in progress
        OldTimeStamp = DWT->CYCCNT;
        SIPutByte(0xC0);
        SIPutByte(0x00);
        SIPutByte(0x00);
        SIPutDeviceTerminator();
    }

    if (Command == EEPROM_READ) {
        BYTE AddressByte;
        SIGetByte(AddressByte, true);
        uint32_t Address = AddressByte * 8;
        OldTimeStamp = DWT->CYCCNT;
        for (int i = 0; i < 8; i += 1) {
            SIPutByte(EEPROMStore[Address + i]);
        }

        SIPutDeviceTerminator();
    }

    if (Command == EEPROM_STORE) {
        BYTE AddressByte;
        SIGetByte(AddressByte, true);
        uint32_t Address = AddressByte * 8;
        Address *= 8;
        for (int i = 0; i < 8; i += 1) {
            SIGetByte(EEPROMStore[Address + i], true);
        }
        SIGetConsoleTerminator();
        OldTimeStamp = DWT->CYCCNT;
        SIPutByte(0x00); // Output not busy, coz we fast.
        SIPutDeviceTerminator();
    }
}

void MeasureTime()
{
    volatile uint32_t TimeStamp = DWT->CYCCNT;
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();

    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    volatile uint32_t NewTimeStamp = DWT->CYCCNT;
    volatile uint32_t Delta = NewTimeStamp - TimeStamp;
    while (Delta) {}
}


extern "C" const unsigned char __dtcmram_bss_start__;
extern "C" const unsigned char __dtcmram_bss_end__;
extern "C" const unsigned char dtcm_data;

extern "C" const unsigned char itcm_text_start;
extern "C" const unsigned char itcm_text_end;
extern "C" const unsigned char itcm_data;

int main(void)
{
    // Relocate vector table to RAM as well as all interrupt functions and high usage variables.
    memcpy((void*)&itcm_text_start, &itcm_data, (int) (&itcm_text_end - &itcm_text_start));
    memcpy((void*)&__dtcmram_bss_start__, &dtcm_data, (int) (&__dtcmram_bss_end__ - &__dtcmram_bss_start__));

    // Init hardware
#if OVERCLOCK
    hw.Init(true);
#else
    hw.Init(false);
#endif

    volatile uint32_t TimerCtrl = SysTick->CTRL;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    // Preconfigure GPIO PortA and PortB, so the following directional changes can be faster.
    GPIO_InitTypeDef PortAPins = {0xFF, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GP_SPEED, 0};
    GPIO_InitTypeDef PortBPins = {0xC3F0, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOA, &PortAPins);
    HAL_GPIO_Init(GPIOB, &PortBPins);

    PortAPins = {0xFF, GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    PortBPins = {0xC3F0, GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOA, &PortAPins);
    HAL_GPIO_Init(GPIOB, &PortBPins);

    PortBPins = {(1 << 1), GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOB, &PortBPins);

    GPIO_InitTypeDef PortCPins = {(1 << 0) | (1 << 1) , GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOC, &PortCPins);

    PortCPins = {S_DAT_LINE, GPIO_MODE_OUTPUT_OD, GPIO_PULLUP, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOC, &PortCPins);
    GPIOC->BSRR = S_DAT_LINE;

    PortCPins = {(1 << 7), GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOC, &PortCPins);

    GPIO_InitTypeDef PortDPins = {(1 << 11), GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOD, &PortDPins);

    GPIO_InitTypeDef PortGPins = {(1 << 10) | (1 << 11), GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    PortGPins.Pin |= (1 << 9); // For now make this an input pin. It actually needs to be an OUT_OD pin.
    HAL_GPIO_Init(GPIOG, &PortGPins);

    LoadRom(RomName[0]);
    //MeasureTime();

    // TODO: "Output complete" timer setup. This timer needs to be setup to hit at the end of second read interrupt.
    // This timer allows the interrupt to complete instead of hogging up the CM7, the completion time needs to 
    // be calculated based on the speed value set in the rom.
    //NVIC_SetVector(EXTI1_IRQn, (uint32_t)&__EXTI0_IRQHandler);
    //LPTIM_Config();
    // Disable LP3 timer.
    //DISABLE_LP3_TIMER();

#if 0
    constexpr Pin D25 = Pin(PORTA, 0); // AD0  // Could be used for DMA
    constexpr Pin D24 = Pin(PORTA, 1); // AD1
    constexpr Pin D28 = Pin(PORTA, 2); // AD2  // Could be used for DMA
    constexpr Pin D16 = Pin(PORTA, 3); // AD3
    constexpr Pin D23 = Pin(PORTA, 4); // AD4
    constexpr Pin D22 = Pin(PORTA, 5); // AD5
    constexpr Pin D19 = Pin(PORTA, 6); // AD6
    constexpr Pin D18 = Pin(PORTA, 7); // AD7

    constexpr Pin D17 = Pin(PORTB, 1); // Write.  TIM3_CH4
    constexpr Pin D9  = Pin(PORTB, 4); // AD8
    constexpr Pin D10 = Pin(PORTB, 5); // AD9
    constexpr Pin D13 = Pin(PORTB, 6); // AD10
    constexpr Pin D14 = Pin(PORTB, 7); // AD11
    constexpr Pin D11 = Pin(PORTB, 8); // AD12
    constexpr Pin D12 = Pin(PORTB, 9); // AD13
    constexpr Pin D0  = Pin(PORTB, 12); // ALE_H
    constexpr Pin D29 = Pin(PORTB, 14); // AD14
    constexpr Pin D30 = Pin(PORTB, 15); // AD15

    constexpr Pin D15 = Pin(PORTC, 0); // ALE_L
    constexpr Pin D20 = Pin(PORTC, 1); // Read
    constexpr Pin D21 = Pin(PORTC, 4); // S-DATA
    constexpr Pin D4  = Pin(PORTC, 8); // SD card D0
    constexpr Pin D3  = Pin(PORTC, 9); // SD card D1
    constexpr Pin D2  = Pin(PORTC, 10); // SD card D2
    constexpr Pin D1  = Pin(PORTC, 11); // SD card D3
    constexpr Pin D6  = Pin(PORTC, 12); // SD card CLK

    constexpr Pin D5  = Pin(PORTD, 2);  // SD card CMD  // Could be used for DMA
    constexpr Pin D26 = Pin(PORTD, 11); // Reset signal // TIM4_CH1 / LPTIM2_IN2 Could be used for READ_IN and as input to DMAMUX2.

    constexpr Pin D27 = Pin(PORTG, 9);  // CIC_D1
    constexpr Pin D7  = Pin(PORTG, 10); // CIC_D2
    constexpr Pin D8  = Pin(PORTG, 11); // N64_NMI // LPTIM1_IN Could be used for read timer DMA.
    // PORTC, 7 -- LED, NC -- Card Detect
    // PORTG, 14 NC -- S-CLK
    // 

            /**SDMMC1 GPIO Configuration    
            PC12     ------> SDMMC1_CK
            PC11     ------> SDMMC1_D3
            PC10     ------> SDMMC1_D2
            PD2     ------> SDMMC1_CMD
            PC9     ------> SDMMC1_D1
            PC8     ------> SDMMC1_D0 */
#endif

    memset(Sram4Buffer, 0, 64*4);
    SCB_CleanInvalidateDCache();

    // Wait for reset line high.
    while ((GPIOD->IDR & RESET_LINE) == 0) { }
    SysTick->CTRL = TimerCtrl;
    System::Delay(2);
    EXTI->PR1 = RESET_LINE;
    while ((GPIOD->IDR & RESET_LINE) == 0) { }
    SysTick->CTRL = 0;

    InitializeInterrupts();

    uint32_t RomIndex = 1;
    while(1) {
        while ((GPIOD->IDR & RESET_LINE) == 0) {
            GPIOA->ODR = 0;
            GPIOB->ODR = 0;
            GPIOB->MODER = 0x0CF000B3;
            GPIOA->MODER = 0xABFF0000;
            DMACount = 0;
            IntCount = 0;
            ALE_H_Count = 0;
            ADInputAddress = 0;
        }

        Running = true;
        while(Running != false) {
            RunEEPROMEmulator();
        }

        LoadRom(RomName[(RomIndex++) % (sizeof(RomName)/sizeof(RomName[0]))]);
    }
}