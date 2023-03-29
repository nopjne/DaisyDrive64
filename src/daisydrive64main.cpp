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
#include "menu.h"
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
#include "sounds.h"

#if !defined(_FS_EXFAT) && !defined(FF_FS_EXFAT)
#error EXFAT FS NEEDS TO BE ENABLED
#endif

#define N64_MIN_PRELOAD (1024 * 1024 * 64)

extern SD_HandleTypeDef hsd1;
volatile uint32_t CurrentRomSaveType = 0;
uint32_t RomIndex = 0;
volatile bool gUseBootLoader = false;
DTCM_DATA char CurrentRomName[265];
volatile bool gAleHAndResetFlip = false;
volatile uint32_t gCicRegion = 0;
struct RomSetting {
    const char* RomName;
    const BYTE BusSpeedOverride;
    const char EepRomType;
};

volatile daisy::SdmmcHandler::Speed gSDSpeed = daisy::SdmmcHandler::Speed::VERY_FAST;

#define TESTROM 0
#if TESTROM
const RomSetting RomSettings[] = {
    {"testrom.z64", 0x12, EEPROM_16K}
};
#else
const RomSetting RomSettings[] = {
    {"DaisyDrive64_data.bin", 0x12, SAVE_FLASH_1M},
    //{"OS64daisyboot.z64", 0x12, SAVE_FLASH_1M},
    //{"OS64P.z64", 0x12, EEPROM_4K}, // This is the menu rom.
#if 0
    //MENU_ROM_FILE_NAME,
    {"1080 TenEighty Snowboarding (Japan, USA) (En,Ja).n64", 0x20, SAVE_FLASH_1M}, // Runs, Needs flash ram support for saves.
    {"Legend of Zelda, The - Ocarina of Time - Master Quest (USA) (GameCube Edition).n64", 0x20, SAVE_FLASH_1M}, // Runs, Needs flash ram support for saves.
    {"Legend of Zelda, The - Majora's Mask (USA) (GameCube Edition).n64", 0x20, SAVE_FLASH_1M}, // Runs, Needs flash ram support for saves.
    {"Super Smash Bros. (USA).n64", 0x18, SAVE_FLASH_1M}, // Runs, Needs flash ram support for saves.
    {"Paper Mario (USA).n64", 0x40, SAVE_FLASH_1M}, // Runs, Needs flash ram support for saves.
    {"Mario Golf (USA).n64", 0x17, SAVE_FLASH_1M}, // Runs, Needs flash ram support for saves.
    {"Resident Evil 2 (USA).n64", 0x17, SAVE_FLASH_1M},
    {"Mario Kart 64 (USA).n64", 0x12, EEPROM_4K},
    {"1080 TenEighty Snowboarding (Japan, USA) (En,Ja).n64", 0x20, SAVE_SRAM}, // Runs, Needs flash ram support for saves.
    {"Legend of Zelda, The - Ocarina of Time - Master Quest (USA) (GameCube Edition).n64", 0x20, SAVE_SRAM}, // Runs, Needs flash ram support for saves.
    {"Legend of Zelda, The - Majora's Mask (USA) (GameCube Edition).n64", 0x20, SAVE_FLASH_1M}, // Runs, Needs flash ram support for saves.
    {"Super Smash Bros. (USA).n64", 0x18, SAVE_SRAM}, // Runs, Needs flash ram support for saves.
    {"Paper Mario (USA).n64", 0x40, SAVE_FLASH_1M}, // Runs, Needs flash ram support for saves.
    {"Mario Golf (USA).n64", 0x17, SAVE_SRAM}, // Runs, Needs flash ram support for saves.
    {"Resident Evil 2 (USA).n64", 0x17, SAVE_SRAM},
    {"Mario Kart 64 (USA).n64", 0x12, EEPROM_4K},
    {"Donkey Kong 64 (USA).n64", 0x16, EEPROM_16K}, // Boots but very unstable, crashes anywhere.
    {"Star Fox 64 (USA).n64", 0x17, EEPROM_4K},
    {"Star Fox 64 (USA) (Rev A).n64", 0x18, EEPROM_4K},
    {"Harvest Moon 64 (USA).n64", 0x17, EEPROM_4K},
    {"Conker's Bad Fur Day (USA).n64", 0x20, EEPROM_16K},
    {"Yoshi's Story (USA) (En,Ja).n64", 0x17, EEPROM_16K},
    {"Super Mario 64 (USA).n64", 0x17, EEPROM_4K},
    {"Mario Tennis (USA).n64", 0x17, EEPROM_16K},
    {"Mortal Kombat Trilogy (USA) (Rev B).n64", 0x17, EEPROM_4K},
    {"007 - GoldenEye (USA).n64", 0x17, EEPROM_4K},
    {"007 - The World Is Not Enough (USA).n64", 0x17, EEPROM_4K},
    {"Killer Instinct Gold (USA).n64", 0x17, EEPROM_4K},
    {"Mario Tennis 64 (Japan).n64", 0x17, EEPROM_16K},
    {"Mortal Kombat 4 (USA).n64", 0x17, EEPROM_16K},
    {"Mortal Kombat Mythologies - Sub-Zero (USA).n64", 0x17, EEPROM_16K},
    {"Mario Party 2 (USA).n64", 0x17, EEPROM_4K},
    {"Mario Party 3 (USA).n64", 0x17, EEPROM_16K},
    {"Killer Instinct Gold (USA) (Rev B).n64", 0x17, EEPROM_4K},
    {"Wave Race 64 (USA) (Rev A).n64", 0x17, EEPROM_4K},
    {"Perfect Dark (USA) (Rev A).n64", 0x17, EEPROM_16K},
    {"Star Fox 64 (Japan).n64", 0x17, EEPROM_4K},
    {"Pilotwings 64 (USA).n64", 0x17, EEPROM_4K},
    {"Turok - Dinosaur Hunter (USA).n64", 0x17, EEPROM_4K},
    {"Blast Corps (USA).n64", 0x17, EEPROM_4K},
#endif
};
#endif

unsigned char *ram = (unsigned char *)0xC0000000;
using namespace daisy;
uint32_t RomMaxSize = 0;

SdmmcHandler   sd;
FatFSInterface fsi;
FIL            SDFile;
FIL            SDSaveFile;
FILINFO        gFileInfo;
bool           gByteSwap = false;

drmp3 *MP3Decoder;
DaisySeed hw;
int PlayAudio(const char* name);
int PlayInternalAudio(const BYTE* Memory, size_t Size);
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

void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t                                size)
{
    drmp3_read_pcm_frames_f32(MP3Decoder, 1, out);
}

int PlayInternalAudio(const BYTE* Memory, size_t Size)
{
    if (MP3Decoder == nullptr) {
        MP3Decoder = new drmp3();
    }

    if (drmp3_init_memory(MP3Decoder, (const char*)Memory, Size, nullptr) == false) {
        return 1;
    }

    uint32_t blocksize = 1152;
    hw.SetAudioBlockSize(blocksize);
    hw.StartAudio(AudioCallback);
    return 0;
}

int PlayAudio(const char* FileName)
{
    SdmmcHandler::Config sd_cfg;
    sd_cfg.Defaults();
    sd_cfg.speed = SD_SPEED;
    sd.Init(sd_cfg);
    fsi.Init(FatFSInterface::Config::MEDIA_SD);
    f_mount(&fsi.GetSDFileSystem(), "/", 1);
    MP3Decoder = new drmp3();
    if (drmp3_init_file(MP3Decoder, FileName, nullptr) == false) {
        return 1;
    }

    uint32_t blocksize = 1152;
    hw.SetAudioBlockSize(blocksize);
    hw.StartAudio(AudioCallback);

    //if (!drmp3_init_file(&mp3, "MySong.mp3", NULL)) {
    //    // Failed to open file
    //    return 1;
    //}

    //WavPlayer      *sampler = nullptr;
    //if (sampler == nullptr) {
    //    sampler = new WavPlayer();
    //    // Init hardware
    //    size_t blocksize = 4096;
//
    //    SdmmcHandler::Config sd_cfg;
    //    sd_cfg.Defaults();
    //    sd_cfg.speed = SD_SPEED;
    //    sd.Init(sd_cfg);
    //    fsi.Init(FatFSInterface::Config::MEDIA_SD);
    //    f_mount(&fsi.GetSDFileSystem(), "/", 1);
//
    //    sampler->Init(fsi.GetSDPath());
    //    sampler->SetLooping(true);
//
    //    // Init Audio
    //    hw.SetAudioBlockSize(blocksize);
    //    hw.StartAudio(AudioCallback);
//
    //    sampler->Open(sampler->GetCurrentFile() + 1);
    //}
//
    //// Loop while running.
    //while (Running != false) {
    //    // Prepare buffers for sampler as needed
    //    sampler->Prepare();
    //}
    return 0;
}

void InitializeInterrupts(void)
{
    // Disable systick interrupts. (Needs to be re-enabled for SDCard reads)
    InitializeDmaChannels();

    GPIO_InitTypeDef GPIO_InitStruct;
    // ALEH interrupt setup. -- 
    // This interrupt is used to indicate that ALE_H high, and the mode needs to switch to input.
    GPIO_InitStruct = {ALE_H, GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // ALEL interrupt setup. Needs to cause a DMA transaction. From Perih to Memory.
    GPIO_InitStruct = {ALE_L, GPIO_MODE_IT_RISING, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
    NVIC_SetVector(EXTI0_IRQn, (uint32_t)&EXTI0_IRQHandler);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);

    GPIO_InitStruct = {READ_LINE, GPIO_MODE_IT_FALLING, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    HAL_NVIC_SetPriority(EXTI1_IRQn, 1, 0);
    NVIC_SetVector(EXTI1_IRQn, (uint32_t)&EXTI1_IRQHandler);
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);

    // NMI Reset line setup
    GPIO_InitStruct = {N64_NMI, GPIO_MODE_IT_FALLING, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    //GPIO_InitStruct = {RESET_LINE, GPIO_MODE_IT_FALLING, GPIO_NOPULL, GP_SPEED, 0};
    //HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    NVIC_SetVector(EXTI15_10_IRQn, (uint32_t)&EXTI15_10_IRQHandler);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 10, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void SaveEEPRom(const char* Name)
{
    SdmmcHandler::Config sd_cfg;
    sd_cfg.Defaults();
    sd_cfg.speed = SD_SPEED;

    int retry = 5;
    while (retry != 0) {
        retry -= 1;
        sd.Init(sd_cfg);

        // Links libdaisy i/o to fatfs driver.
        fsi.Init(FatFSInterface::Config::MEDIA_SD);

        // Mount SD Card
        if (f_mount(&fsi.GetSDFileSystem(), "/", 1) != FR_OK) {
            fsi.DeInit();
            HAL_SD_DeInit(&hsd1);
            continue;
        }

        // Open the eeprom save file for the requested rom.
        char SaveName[265 + 4];
        sprintf(SaveName, "%s.eep", Name);
        if (f_open(&SDFile, SaveName, (FA_WRITE)) == FR_OK) {
            UINT byteswritten;
            f_write(&SDFile, EEPROMStore, sizeof(EEPROMStore), &byteswritten);
            f_close(&SDFile);

            if (byteswritten != sizeof(EEPROMStore)) {
                continue;
            }

            return;
        }
    }

    // Let the user know something went wrong.
    PlayInternalAudio(sdfailedmp3, sizeof(sdfailedmp3));
    BlinkAndDie(1000, 500);
}

void SaveFlashRam(const char* Name)
{
    SdmmcHandler::Config sd_cfg;
    sd_cfg.Defaults();
    sd_cfg.speed = SD_SPEED;

    int retry = 5;
    while (retry != 0) {
        retry -= 1;
        sd.Init(sd_cfg);

        // Links libdaisy i/o to fatfs driver.
        fsi.Init(FatFSInterface::Config::MEDIA_SD);

        // Mount SD Card
        if (f_mount(&fsi.GetSDFileSystem(), "/", 1) != FR_OK) {
            fsi.DeInit();
            HAL_SD_DeInit(&hsd1);
            continue;
        }

        // Open the flash/sram save file for the requested rom.
        char SaveName[265 + 4];
        sprintf(SaveName, "%s.fla", Name);
        if (f_open(&SDFile, SaveName, (FA_WRITE)) == FR_OK) {
            UINT byteswritten;
            f_write(&SDFile, FlashRamStorage, sizeof(FlashRamStorage), &byteswritten);
            f_close(&SDFile);

            if (byteswritten != sizeof(FlashRamStorage)) {
                continue;
            }

            return;
        }
    }

    // Let the user know something went wrong.
    PlayInternalAudio(flashloadfailedmp3, sizeof(flashloadfailedmp3));
    BlinkAndDie(2000, 500);
}

void LoadRom(const char* Name)
{
    GPIOC->BSRR = USER_LED_PORTC;

    size_t bytesread = 0;
    SdmmcHandler::Config sd_cfg;
    sd_cfg.Defaults();
    uint32_t retry = 6;
    while (retry--)
    {
        sd_cfg.speed = SD_SPEED;
        sd.Init(sd_cfg);

        // Links libdaisy i/o to fatfs driver.
        fsi.Init(FatFSInterface::Config::MEDIA_SD);

        // Mount SD Card
        if (f_mount(&fsi.GetSDFileSystem(), "/", 1) != FR_OK) {
            // Read the CMD, if the line is not pulled low, the card is not connected.
            if ((GPIOD->IDR & SD_CARD_CMD) != 0) {
                PlayInternalAudio(nosdcardmp3, sizeof(nosdcardmp3));
                BlinkAndDie(100, 200);
            }

            if (SD_SPEED == SdmmcHandler::Speed::VERY_FAST) {
                SD_SPEED = SdmmcHandler::Speed::FAST;
            } else if (SD_SPEED == SdmmcHandler::Speed::FAST) {
                SD_SPEED = SdmmcHandler::Speed::STANDARD;
            } else if (SD_SPEED == SdmmcHandler::Speed::STANDARD) {
                SD_SPEED = SdmmcHandler::Speed::MEDIUM_SLOW;
            } else if (SD_SPEED == SdmmcHandler::Speed::MEDIUM_SLOW) {
                SD_SPEED = SdmmcHandler::Speed::SLOW;
            } else {
                PlayInternalAudio(sdfailedmp3, sizeof(sdfailedmp3));
                BlinkAndDie(500, 100);
            }

            fsi.DeInit();
            HAL_SD_DeInit(&hsd1);
            continue;
        }

        // Read requested rom from the SD Card.
        if(f_open(&SDFile, Name, FA_READ) == FR_OK) {
            FRESULT result = f_stat(Name, &gFileInfo);
            if (result != FR_OK) {
                PlayInternalAudio(romfstatfailedmp3, sizeof(romfstatfailedmp3));
                BlinkAndDie(100, 500);
            }

            uint32_t IntroReadSize = std::min<size_t>(N64_MIN_PRELOAD, gFileInfo.fsize);
            result = f_read(&SDFile, ram, IntroReadSize, &bytesread);
            if (result != FR_OK) {
                PlayInternalAudio(romreadfailedmp3, sizeof(romreadfailedmp3));
                BlinkAndDie(200, 200);
            }

            // Blink led on error.
            if (bytesread != IntroReadSize) {
                PlayInternalAudio(romsizemismatchmp3, sizeof(romsizemismatchmp3));
                BlinkAndDie(500, 500);
            }

            RomMaxSize = bytesread;

            // If extension is z64, byte swap.
            // TODO: Setup DMA to do this, could use FIFO to speed things up.
            uint32_t NameLength = strlen(Name);
            gByteSwap = false;
            if (Name[NameLength - 3] == 'z') {
                gByteSwap = true;
                for (uint32_t i = 0; i < RomMaxSize; i += 2) {
                    *((uint16_t*)(ram + i)) = __bswap16(*((uint16_t*)(ram + i)));
                }
            }

            strcpy(CurrentRomName, Name);

        } else {
            PlayInternalAudio(romfopenfailedmp3, sizeof(romfopenfailedmp3));
            BlinkAndDie(100, 100);
        }

        break;
    }

    // No led on on success.
    GPIOC->BSRR = USER_LED_PORTC << 16;

    // Use the preset patch speed.
#ifdef ALLOW_BUS_SPEED_OVERRIDE
    *(ram + 3) = RomSettings[WRAP_ROM_INDEX(RomIndex)].BusSpeedOverride;
#endif

    // Patch speed.
#if (READ_DELAY_NS == 4000)
    *(ram + 3) =  0xFF;
#elif (READ_DELAY_NS == 2000)
    *(ram + 3) =  0x80;
#elif (READ_DELAY_NS == 1000)
    *(ram + 3) =  0x40;
#elif (READ_DELAY_NS == 750)
    *(ram + 3) =  0x30;
#elif (READ_DELAY_NS == 500)
    *(ram + 3) =  0x20;
#elif (READ_DELAY_NS == 400)
    *(ram + 3) =  0x19;
#endif

    gUseBootLoader = false;
}

void ContinueRomLoad(void)
{
    size_t bytesread = 0;
    size_t ReadSize =  (gFileInfo.fsize - N64_MIN_PRELOAD);
    if ((gFileInfo.fsize > N64_MIN_PRELOAD) && (ReadSize < gFileInfo.fsize) && (ReadSize != 0))
    {
        FRESULT result = f_read(&SDFile, ram + N64_MIN_PRELOAD, ReadSize, &bytesread);
        if ((bytesread != ReadSize) || (result != F_OK)) {
            PlayInternalAudio(romloadsizemismatchmp3, sizeof(romloadsizemismatchmp3));
            BlinkAndDie(500, 500);
        }

        RomMaxSize += ReadSize;
    }

    f_close(&SDFile);

    // If extension is z64, byte swap.
    if (gByteSwap != false) {
        for (uint32_t i = N64_MIN_PRELOAD; i < RomMaxSize; i += 2) {
            *((uint16_t*)(ram + i)) = __bswap16(*((uint16_t*)(ram + i)));
        }
    }

    // Open the eeprom save file for the requested rom.
    if (CurrentRomSaveType == EEPROM_16K || CurrentRomSaveType == EEPROM_4K) {
        char SaveName[265 + 4];
        sprintf(SaveName, "%s.eep", CurrentRomName);
        if(f_open(&SDSaveFile, SaveName, FA_READ) != FR_OK) {
            if (f_open(&SDSaveFile, SaveName, (FA_CREATE_ALWAYS) | (FA_WRITE)) == FR_OK) {
                memset(EEPROMStore, 0, sizeof(EEPROMStore));
                UINT byteswritten;
                f_write(&SDSaveFile, EEPROMStore, sizeof(EEPROMStore), &byteswritten);
                f_close(&SDSaveFile);

                if (byteswritten != sizeof(EEPROMStore)) {
                    // Let the user know something went wrong.
                    PlayInternalAudio(eepromcreatesizemismatchmp3, sizeof(eepromcreatesizemismatchmp3));
                    BlinkAndDie(1000, 500);
                }
            }

        } else {
            FILINFO FileInfo;
            volatile FRESULT result = f_stat(SaveName, &FileInfo);
            if (FileInfo.fsize != sizeof(EEPROMStore)) {
                PlayInternalAudio(eepromstatsizemismatchmp3, sizeof(eepromstatsizemismatchmp3));
                BlinkAndDie(200, 300);
            }

            result = f_read(&SDSaveFile, EEPROMStore, FileInfo.fsize, &bytesread);
            if (result != FR_OK) {
                PlayInternalAudio(eepromloadfailedmp3, sizeof(eepromloadfailedmp3));
                BlinkAndDie(200, 300);
            }

            f_close(&SDSaveFile);
        }
    } else {
        // Open the FLASH ram file for the requested rom. (Sram and Flash ram can live in RAM_D2 as those require only 128KB)
        char SaveName[265 + 4];
        sprintf(SaveName, "%s.fla", CurrentRomName);
        if(f_open(&SDSaveFile, SaveName, FA_READ) != FR_OK) {
            if (f_open(&SDSaveFile, SaveName, (FA_CREATE_ALWAYS) | (FA_WRITE)) == FR_OK) {
                memset(FlashRamStorage, 0xFF, sizeof(FlashRamStorage));
                UINT byteswritten;
                f_write(&SDSaveFile, FlashRamStorage, sizeof(FlashRamStorage), &byteswritten);
                f_close(&SDSaveFile);

                if (byteswritten != sizeof(FlashRamStorage)) {
                    f_unlink(SaveName);
                    // Let the user know something went wrong.
                    PlayInternalAudio(flashcreatesizemismatchmp3, sizeof(flashcreatesizemismatchmp3));
                    BlinkAndDie(1000, 500);
                }
            }

        } else {
            FILINFO FileInfo;
            FRESULT result = f_stat(SaveName, &FileInfo);
            if (FileInfo.fsize != sizeof(FlashRamStorage)) {
                f_close(&SDSaveFile);
                f_unlink(SaveName);
                PlayInternalAudio(flashfstatsizemismatchmp3, sizeof(flashfstatsizemismatchmp3));
                BlinkAndDie(200, 300);
            }

            result = f_read(&SDSaveFile, FlashRamStorage, FileInfo.fsize, &bytesread);
            if (result != FR_OK) {
                PlayInternalAudio(flashloadfailedmp3, sizeof(flashloadfailedmp3));
                BlinkAndDie(200, 300);
            }

            f_close(&SDSaveFile);
        }
    }
}

void SetupBootloader(void)
{
    RomMaxSize = 1064960;
    gUseBootLoader = true;
    strcpy(CurrentRomName, "OS64daisyboot.z64\0");
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
extern void * g_pfnVectors;

#include "diskio.h"

int main(void)
{
    __HAL_RCC_GPIOG_CLK_ENABLE();
    GPIO_InitTypeDef PortGPins = {CIC_DAT, GPIO_MODE_OUTPUT_OD, GPIO_PULLUP, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOG, &PortGPins);
    GPIOG->BSRR = CIC_DAT << 16;

    // Init hardware
#if OVERCLOCK
    hw.Init(true);
#else
    hw.Init(false);
#endif

    // Override interrupt priority for SAI transfer.
    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 15, 1);
    HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 15, 1);
    HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 15, 1);
    HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 15, 1);
    //hw.ConfigureAudio();

    // Relocate vector table to RAM as well as all interrupt functions and high usage variables.
    memcpy((void*)&itcm_text_start, &itcm_data, (int) (&itcm_text_end - &itcm_text_start));
    memcpy((void*)&__dtcmram_bss_start__, &dtcm_data, (int) (&__dtcmram_bss_end__ - &__dtcmram_bss_start__));

    PortGPins = {CIC_CLK, GPIO_MODE_IT_RISING_FALLING, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOG, &PortGPins);
    NVIC_SetVector(EXTI9_5_IRQn, (uint32_t)&EXTI9_5_IRQHandler);
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

    GPIO_InitTypeDef PortCPins = {USER_LED_PORTC, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOC, &PortCPins);

    CICEmulatorInitFast();
    StartCICEmulator();
    //SCB_DisableDCache();

    // Hack to flash the menu rom to the qspi.
//#define FLASH_THE_MENU_ROM 1
#if FLASH_THE_MENU_ROM
    LoadRom(RomSettings[WRAP_ROM_INDEX(RomIndex)].RomName);
    GPIOC->BSRR = USER_LED_PORTC;
    hw.qspi.Erase((uint32_t)hw.qspi.GetData(), (uint32_t)(hw.qspi.GetData()) + RomMaxSize);
    hw.qspi.Write((uint32_t)hw.qspi.GetData(), RomMaxSize, (uint8_t*)ram);
    GPIOC->BSRR = USER_LED_PORTC << 16;
#else
    SetupBootloader();
#endif

    TimerCtrl = SysTick->CTRL;
    //SysTick->CTRL = 0;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    
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

    PortCPins = {READ_LINE, GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOC, &PortCPins);

    PortCPins = {S_DAT_LINE, GPIO_MODE_OUTPUT_OD, GPIO_PULLUP, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOC, &PortCPins);
    GPIOC->BSRR = S_DAT_LINE;

    GPIO_InitTypeDef ResetLinePin = {RESET_LINE, GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOD, &ResetLinePin);

    GPIO_InitTypeDef ALE_L_Pin = {ALE_L, GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOC, &ALE_L_Pin);

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GP_SPEED;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    memset(Sram4Buffer, 0, 64 * 4);
    memset(SDataBuffer, 0, sizeof(SDataBuffer));
    SCB_CleanInvalidateDCache();

    EEPROMType = RomSettings[RomIndex].EepRomType;
    CurrentRomSaveType = EEPROMType;

    // Read the ALE_H line and if it is high, it needs to switch with the RESET_LINE.
    // When both the lines are low, the DaisyDrive64 is in a tethered boot scenario and the detection will need to happen later.
    // First line to go Hi is RESET from now on.
    while (RESET_IS_LOW && ALE_H_IS_LOW) { }
    if (ALE_H_IS_HIGH) {
        gAleHAndResetFlip = true;
    }

    // Wait for reset line high.
    while (RESET_IS_LOW) { }
    EXTI->PR1 = (gAleHAndResetFlip != false) ? ALE_H : RESET_LINE;
    while (RESET_IS_LOW) { }

    InitMenuFunctions();
    InitializeInterrupts();
    InitializeTimersPI();
    InitializeTimersSI();
    while(1) {
        while (RESET_IS_LOW) {
            DaisyDriveN64Reset();
        }

        if (CurrentRomSaveType == EEPROM_16K || CurrentRomSaveType == EEPROM_4K) {
            SI_Enable();
        }

        Running = true;
        OverflowCounter = 0;
        if (RomIndex != 0) {
            StartCICEmulator();
            ContinueRomLoad();
        }

        while(Running != false) {
        }

        while (SaveFileDirty != false) {
        }

        RomIndex += 1;
        LoadRom(RomSettings[WRAP_ROM_INDEX(RomIndex)].RomName);
        CICEmulatorInit();
        EEPROMType = RomSettings[WRAP_ROM_INDEX(RomIndex)].EepRomType;
        CurrentRomSaveType = EEPROMType;
        if (CurrentRomSaveType == EEPROM_16K || CurrentRomSaveType == EEPROM_4K) {
            SI_Reset();
        }

        // Write the boot rom to flash.
        if ((RomIndex == 2) && CURRENT_ROMNAME_STARTS_WITH_OS64) {
            ContinueRomLoad();
            GPIOC->BSRR = USER_LED_PORTC;
            hw.qspi.Erase((uint32_t)hw.qspi.GetData(0x40000), (uint32_t)(hw.qspi.GetData()) + RomMaxSize);
            hw.qspi.Write((uint32_t)hw.qspi.GetData(0x40000), RomMaxSize, (uint8_t*)ram);
            GPIOC->BSRR = USER_LED_PORTC << 16;
        }
    }
}