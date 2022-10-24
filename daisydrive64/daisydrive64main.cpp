#define DEBUG_DEFAULT_INTERRUPT_HANDLERS 1
#include <stdio.h>
#include <string.h>
#include "daisy_seed.h"
#include "fatfs.h"
#include "stm32h7xx_hal.h"
#include "sys/system.h"
#include "stm32h7xx_hal_dma.h"
#include "n64common.h"
#include "daisydrive64.h"

#ifndef _FS_EXFAT
#error EXFAT FS NEEDS TO BE ENABLED
#endif

#define MENU_ROM_FILE_NAME "menu.n64"

//#define TESTROM 1
#ifdef TESTROM
const char* RomName[] = {
    "testrom.z64",
    
};
#else
const char* RomName[] = {
    //MENU_ROM_FILE_NAME,
    "Harvest Moon 64 (USA).n64",
    "Conker's Bad Fur Day (USA).n64",
    "Star Fox 64 (USA) (Rev A).n64",
    "Legend of Zelda, The - Ocarina of Time - Master Quest (USA) (GameCube Edition).n64", // Need to fix the speed of DOMAIN2 reads.
    "Legend of Zelda, The - Majora's Mask (USA) (GameCube Edition).n64",
    "Yoshi's Story (USA) (En,Ja).n64",
    "Donkey Kong 64 (Japan).n64", // Does not boot.
    "Super Smash Bros. (USA).n64", // Hangs, need to fix the speed of DOMAIN2 reads.
    "Paper Mario (USA).n64",

    "Mario Kart 64 (USA).n64", // Works
    "Super Mario 64 (USA).n64", // Works
    "Mario Tennis (USA).n64", // Works
    "Killer Instinct Gold (USA).n64", // Works
    "Mortal Kombat Trilogy (USA) (Rev B).n64", // Works
    "007 - GoldenEye (USA).n64", // Works
    "Resident Evil 2 (USA).n64",  // Works
    "007 - The World Is Not Enough (USA).n64",  // Works
    
    "Mario Tennis 64 (Japan).n64", // Works
    "Mortal Kombat 4 (USA).n64", // Works
    "Mortal Kombat Mythologies - Sub-Zero (USA).n64", // Works.
    "Mario Party 2 (USA).n64", // Works
    "Mario Party 3 (USA).n64", // Works
    "Killer Instinct Gold (USA) (Rev B).n64", // Works
    "Wave Race 64 (USA) (Rev A).n64", // Works
    "Perfect Dark (USA) (Rev A).n64", // Works
    "Mario Golf (USA).n64", // Works, need saves.
    "Star Fox 64 (Japan).n64", // Works, hangs because of EEPROM interfering with PI address latch.
    "Pilotwings 64 (USA).n64",
    "Turok - Dinosaur Hunter (USA).n64",
    "1080 TenEighty Snowboarding (Japan, USA) (En,Ja).n64",
    "Blast Corps (USA).n64",
};
#endif

const BYTE EEPROMTypeArray[] = {
    EEPROM_4K, // Harvest moon
    EEPROM_16K, // Conker's
    EEPROM_4K, // SF64
    EEPROM_4K, // Zelda
    EEPROM_4K, // Zelda
    EEPROM_16K, // Yoshi
    EEPROM_16K, // DK64
    EEPROM_4K, // SMS64
    EEPROM_4K, // Paper Mario
    EEPROM_4K, // MK64
    EEPROM_4K, // SM64
    EEPROM_16K, // Mario Tennis
    EEPROM_4K, // KI
    EEPROM_4K, // MKT
    EEPROM_4K, // Golden eye
    EEPROM_4K, // RE2
    EEPROM_4K, // World
    EEPROM_16K, // Tennis
    EEPROM_16K, // MK4
    EEPROM_16K, // MK Myth
    EEPROM_4K, // MP2
    EEPROM_16K, // MP3
    EEPROM_4K, // KI
    EEPROM_4K, // Waverace
    EEPROM_16K, // PD
    EEPROM_4K, // MGolf
    EEPROM_4K, // SF4 JP
    EEPROM_4K, // Pilotwings
    EEPROM_4K, // Turok
    EEPROM_4K, // 1080 snow.
    EEPROM_4K, // blast corps.
};

unsigned char *ram = (unsigned char *)0xC0000000;
using namespace daisy;
uint32_t RomMaxSize = 0;

SdmmcHandler   sd;
FatFSInterface fsi;
FIL            SDFile;

static DaisySeed hw;

volatile uint32_t TimerCtrl;
void BlinkAndDie(int wait1, int wait2)
{
    SysTick->CTRL = TimerCtrl;
    while(1) {
        GPIOC->BSRR = USER_LED_PORTC;
        System::Delay(wait1);
        GPIOC->BSRR = USER_LED_PORTC << 16;
        System::Delay(wait2);
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
    HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 2);
    NVIC_SetVector(EXTI1_IRQn, (uint32_t)&EXTI1_IRQHandler);
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);

    // Reset line setup
    GPIO_InitStruct = {RESET_LINE, GPIO_MODE_IT_FALLING, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
}

void SaveEEPRom(const char* Name)
{
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
        if (f_open(&SDFile, SaveName, (FA_WRITE)) == FR_OK) {
            UINT byteswritten;
            f_write(&SDFile, EEPROMStore, sizeof(EEPROMStore), &byteswritten);
            f_close(&SDFile);

            if (byteswritten != sizeof(EEPROMStore)) {
                // Let the user know something went wrong.
                BlinkAndDie(1000, 500);
            }
        } else {
            BlinkAndDie(2000, 500);
        }
    }
}

void SaveFlashRam(const char* Name)
{
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
        sprintf(SaveName, "%s.fla", Name);
        if (f_open(&SDFile, SaveName, (FA_WRITE)) == FR_OK) {
            UINT byteswritten;
            f_write(&SDFile, FlashRamStorage, sizeof(FlashRamStorage), &byteswritten);
            f_close(&SDFile);

            if (byteswritten != sizeof(FlashRamStorage)) {
                // Let the user know something went wrong.
                BlinkAndDie(1000, 500);
            }
        } else {
            BlinkAndDie(2000, 500);
        }
    }
}


void LoadRom(const char* Name)
{
    GPIOC->BSRR = USER_LED_PORTC;

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
            volatile FRESULT result = f_stat(SaveName, &FileInfo);
            if (FileInfo.fsize != sizeof(EEPROMStore)) {
                BlinkAndDie(200, 300);
            }

            result = f_read(&SDFile, EEPROMStore, FileInfo.fsize, &bytesread);
            if (result != FR_OK) {
                BlinkAndDie(200, 300);
            }

            f_close(&SDFile);
        }

#if 0
        // Open the FLASH ram file for the requested rom. (Sram and Flash ram can live in RAM_D2 as those require only 128KB)
        sprintf(SaveName, "%s.fla", Name);
        if(f_open(&SDFile, SaveName, FA_READ) != FR_OK) {
            if (f_open(&SDFile, SaveName, (FA_CREATE_ALWAYS) | (FA_WRITE)) == FR_OK) {
                memset(FlashRamStorage, 0, sizeof(FlashRamStorage));
                UINT byteswritten;
                f_write(&SDFile, FlashRamStorage, sizeof(FlashRamStorage), &byteswritten);
                f_close(&SDFile);

                if (byteswritten != sizeof(FlashRamStorage)) {
                    // Let the user know something went wrong.
                    BlinkAndDie(1000, 500);
                }
            }

        } else {
            FILINFO FileInfo;
            FRESULT result = f_stat(SaveName, &FileInfo);
            if (FileInfo.fsize != sizeof(FlashRamStorage)) {
                BlinkAndDie(200, 300);
            }

            result = f_read(&SDFile, FlashRamStorage, FileInfo.fsize, &bytesread);
            if (result != FR_OK) {
                BlinkAndDie(200, 300);
            }

            f_close(&SDFile);
        }
#endif

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

            RomMaxSize = bytesread;

            // If extension is z64, byte swap.
            // TODO: Setup DMA to do this, could use FIFO to speed things up.
            uint32_t NameLength = strlen(Name);
            if (Name[NameLength - 3] == 'z') {
                for (uint32_t i = 0; i < RomMaxSize; i += 2) {
                    *((uint16_t*)(ram + i)) = __bswap16(*((uint16_t*)(ram + i)));
                }
            }

        } else {
            BlinkAndDie(100, 100);
        }
    }
    // No led on on success.
    GPIOC->BSRR = USER_LED_PORTC << 16;

    // Patch speed.
#if (READ_DELAY_NS == 4000)
    *(ram + 3) =  0xFF;
#elif (READ_DELAY_NS == 2000)
    *(ram + 3) =  0x80;
#elif (READ_DELAY_NS == 1000)
    *(ram + 3) =  0x40;
#elif (READ_DELAY_NS == 750)
    *(ram + 3) =  0x39;
#elif (READ_DELAY_NS == 500)
    *(ram + 3) =  0x20;
#endif
}

inline void DaisyDriveN64Reset(void)
{
#if (USE_OPEN_DRAIN_OUTPUT == 0)
    GPIOA->ODR = 0;
    GPIOB->ODR = 0;
    SET_PI_INPUT_MODE;
#else
    GPIOA->ODR = 0xFF;
    GPIOB->ODR = 0xC3F0;
#endif
    DMACount = 0;
    IntCount = 0;
    ALE_H_Count = 0;
    ADInputAddress = 0;
    EepLogIdx = 0;
}

extern "C" const unsigned char __dtcmram_bss_start__;
extern "C" const unsigned char __dtcmram_bss_end__;
extern "C" const unsigned char dtcm_data;

extern "C" const unsigned char itcm_text_start;
extern "C" const unsigned char itcm_text_end;
extern "C" const unsigned char itcm_data;

void SITest();

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

    TimerCtrl = SysTick->CTRL;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    // Preconfigure GPIO PortA and PortB, so the following directional changes can be faster.
    // Ideally port A or port B should be entirely dedicated to the PI interface.
#if (USE_OPEN_DRAIN_OUTPUT == 0)
    GPIO_InitTypeDef PortAPins = {0xFF, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GP_SPEED, 0};
    GPIO_InitTypeDef PortBPins = {0xC3F0, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOA, &PortAPins);
    HAL_GPIO_Init(GPIOB, &PortBPins);

    PortAPins = {0xFF, GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    PortBPins = {0xC3F0, GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOA, &PortAPins);
    HAL_GPIO_Init(GPIOB, &PortBPins);
#else
    GPIO_InitTypeDef PortAPins = {0xFF, GPIO_MODE_OUTPUT_OD, GPIO_PULLUP, GP_SPEED, 0};
    GPIO_InitTypeDef PortBPins = {0xC3F0, GPIO_MODE_OUTPUT_OD, GPIO_PULLUP, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOA, &PortAPins);
    HAL_GPIO_Init(GPIOB, &PortBPins);
    GPIOA->ODR = 0xFF;
    GPIOB->ODR = 0xC3F0;
#endif

    GPIO_InitTypeDef PortCPins = {READ_LINE, GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOC, &PortCPins);

    PortCPins = {S_DAT_LINE, GPIO_MODE_OUTPUT_OD, GPIO_PULLUP, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOC, &PortCPins);
    GPIOC->BSRR = S_DAT_LINE;

    PortCPins = {USER_LED_PORTC, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOC, &PortCPins);

    GPIO_InitTypeDef ResetLinePin = {RESET_LINE, GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOD, &ResetLinePin);

    GPIO_InitTypeDef ALE_L_Pin = {ALE_L, GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOC, &ALE_L_Pin);

    GPIO_InitTypeDef PortGPins = {CIC_CLK, GPIO_MODE_IT_RISING_FALLING, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOG, &PortGPins);
    NVIC_SetVector(EXTI9_5_IRQn, (uint32_t)&EXTI9_5_IRQHandler);
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

    PortGPins = {CIC_DAT, GPIO_MODE_OUTPUT_OD, GPIO_PULLUP, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOG, &PortGPins);

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    uint32_t RomIndex = 0;
    LoadRom(RomName[RomIndex]);
    CICEmulatorInit();
    EEPROMType = EEPROMTypeArray[RomIndex];

    memset(Sram4Buffer, 0, 64 * 4);
    memset(SDataBuffer, 0, sizeof(SDataBuffer));
    SCB_CleanInvalidateDCache();

    // Wait for reset line high.
    while (RESET_IS_LOW) { }
    GPIOG->BSRR = CIC_DAT;
    SysTick->CTRL = TimerCtrl;
    //System::Delay(2);
    EXTI->PR1 = RESET_LINE;
    while (RESET_IS_LOW) { }
    SysTick->CTRL = 0;

    InitializeInterrupts();
    InitializeTimersPI();
    InitializeTimersSI();

    
    while(1) {
        while (RESET_IS_LOW) {
            DaisyDriveN64Reset();
        }

        SI_Enable();
        Running = true;
        OverflowCounter = 0;
        StartCICEmulator();
        while(Running != false) {
            RunCICEmulator();
        }

        SaveEEPRom(RomName[WRAP_ROM_INDEX(RomIndex)]);
        //SaveFlashRam(RomName[WRAP_ROM_INDEX(RomIndex)]);
        RomIndex += 1;
        LoadRom(RomName[WRAP_ROM_INDEX(RomIndex)]);
        CICEmulatorInit();
        EEPROMType = EEPROMTypeArray[WRAP_ROM_INDEX(RomIndex)];
        SI_Reset();
    }
}