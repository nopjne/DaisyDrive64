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

const char* RomName[] = {
    //MENU_ROM_FILE_NAME,
    "Mario Kart 64 (USA).n64", // Works
    "Super Mario 64 (USA).n64", // Works
    "Mario Tennis (USA).n64", // Works - lots of audio glitches.
    "Killer Instinct Gold (USA).n64", // Works - lots of audio glitches
    //"Mortal Kombat Trilogy (USA).n64", // Gets in menu freezes on player selection screen.
    //"Resident Evil 2 (USA).n64",  // Does not boot.
    //"007 - GoldenEye (USA).n64", // Boots, shows logo and hangs.
    //"007 - The World Is Not Enough (USA).n64",  // Does not boot.
    //"Mario Golf (USA).n64", // Does not boot.
    //"Mario Tennis 64 (Japan).n64", // Works - audio glitches.
    "Mortal Kombat 4 (USA).n64", // Works - audio glitches.
    "Mortal Kombat Mythologies - Sub-Zero (USA).n64", // Works.
    //"Perfect Dark (USA) (Rev A).n64", // Needs different CIC chip. (CIC select not implemented)
    //"Star Fox 64 (Japan).n64", // Needs different CIC chip. (CIC select not implemented)
    //"Star Fox 64 (USA).n64", // Needs different CIC chip. (CIC select not implemented)
};

const BYTE EEPROMTypeArray[] = {
    EEPROM_4K,
    EEPROM_4K,
    EEPROM_4K,
    EEPROM_16K,
    EEPROM_16K,
    EEPROM_16K,
    //EEPROM_16K,
    //EEPROM_16K,
    //EEPROM_16K,
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
    HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
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

            RomMaxSize = bytesread;

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
#elif (READ_DELAY_NS == 500)
    *(ram + 3) =  0x20;
#endif
}

inline void DaisyDriveN64Reset(void)
{
    GPIOA->ODR = 0;
    GPIOB->ODR = 0;
    GPIOB->MODER = 0x0CF000B3;
    GPIOA->MODER = 0xABFF0000;
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
    GPIO_InitTypeDef PortAPins = {0xFF, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GP_SPEED, 0};
    GPIO_InitTypeDef PortBPins = {0xC3F0, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOA, &PortAPins);
    HAL_GPIO_Init(GPIOB, &PortBPins);

    PortAPins = {0xFF, GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    PortBPins = {0xC3F0, GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOA, &PortAPins);
    HAL_GPIO_Init(GPIOB, &PortBPins);

    PortBPins = {WRITE_LINE, GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOB, &PortBPins);

    GPIO_InitTypeDef PortCPins = {(ALE_L | READ_LINE), GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOC, &PortCPins);

    PortCPins = {S_DAT_LINE, GPIO_MODE_OUTPUT_OD, GPIO_PULLUP, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOC, &PortCPins);
    GPIOC->BSRR = S_DAT_LINE;

    PortCPins = {USER_LED_PORTC, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOC, &PortCPins);

    GPIO_InitTypeDef PortDPins = {RESET_LINE, GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOD, &PortDPins);

    GPIO_InitTypeDef PortGPins = {(N64_NMI | CIC_CLK), GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    PortGPins.Pin |= CIC_D1; // For now make this an input pin. It actually needs to be an OUT_OD pin.
    HAL_GPIO_Init(GPIOG, &PortGPins);

    LoadRom(RomName[0]);
    EEPROMType = EEPROMTypeArray[0];

    // TODO: "Output complete" timer setup. This timer needs to be setup to hit at the end of second read interrupt.
    // This timer allows the interrupt to complete instead of hogging up the CM7, the completion time needs to 
    // be calculated based on the speed value set in the rom.
    //NVIC_SetVector(EXTI1_IRQn, (uint32_t)&__EXTI0_IRQHandler);
    //LPTIM_Config();
    // Disable LP3 timer.
    //DISABLE_LP3_TIMER();

    memset(Sram4Buffer, 0, 64 * 4);
    SCB_CleanInvalidateDCache();

    // Wait for reset line high.
    while ((GPIOD->IDR & RESET_LINE) == 0) { }
    SysTick->CTRL = TimerCtrl;
    System::Delay(2);
    EXTI->PR1 = RESET_LINE;
    while ((GPIOD->IDR & RESET_LINE) == 0) { }
    SysTick->CTRL = 0;

    InitializeInterrupts();

    uint32_t RomIndex = 0;
    while(1) {
        while ((GPIOD->IDR & RESET_LINE) == 0) {
            DaisyDriveN64Reset();
        }

        Running = true;
        while(Running != false) {
            RunEEPROMEmulator();
        }

        SaveEEPRom(RomName[RomIndex % (sizeof(RomName)/sizeof(RomName[0]))]);
        RomIndex += 1;
        LoadRom(RomName[RomIndex % (sizeof(RomName)/sizeof(RomName[0]))]);
        EEPROMType = EEPROMTypeArray[RomIndex % (sizeof(RomName)/sizeof(RomName[0]))];
    }
}