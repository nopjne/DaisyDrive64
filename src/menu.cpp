#include "daisy_seed.h"
#include "stm32h7xx_hal.h"
#include "sys/system.h"
#include "stm32h7xx_hal_dma.h"
#include "stm32h7xx_ll_tim.h"
#include "stm32h7xx_ll_bus.h"
#include "n64common.h"
#include "daisydrive64.h"
#include "flashram.h"
#include "diskio.h"
#include "menu.h"
#include "menurom.h"

void LoadRom(const char* Name);
uint32_t *MenuBase = (uint32_t*)FlashRamStorage;

using namespace daisy;
SdmmcHandler::Config sd_cfg;
extern SdmmcHandler   sd;

void InitMenuFunctions(void)
{
    // Setup interrupt priority.
    HAL_NVIC_SetPriority(EXTI3_IRQn, 11, 0);
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);

    // Enable software interrupts.
    EXTI->IMR1 |= DAISY_MENU_INTERRUPT;
    EXTI->EMR1 |= DAISY_MENU_INTERRUPT;
    //memcpy(ram, menurom, sizeof(menurom));
}

void EnableMenu(void) {

    sd_cfg.Defaults();
    sd_cfg.speed = SdmmcHandler::Speed::VERY_FAST;
    sd.Init(sd_cfg);

    // Mount SD Card
    DSTATUS Status = disk_initialize(0);
    if (RES_OK != Status) {
        BlinkAndDie(500, 100);
    }
}

inline void HandleExecute(void)
{
    switch(MenuBase[REG_EXECUTE_FUNCTION]) {
        case GET_FW_VERSION:
            MenuBase[REG_STATUS] |= DAISY_STATUS_BIT_DMA_BUSY;
            MenuBase[REG_FUNCTION_PARAMETER] = 0x666;
            MenuBase[REG_STATUS] &= ~DAISY_STATUS_BIT_DMA_BUSY;
        break;
        case ENABLE_MENU_FUNCTIONS:
            MenuBase[REG_STATUS] |= DAISY_STATUS_BIT_SD_BUSY;
            EnableMenu();
            MenuBase[REG_STATUS] &= ~DAISY_STATUS_BIT_SD_BUSY;
        break;
        case DISABLE_MENU_FUNCTIONS:
            MenuBase[REG_STATUS] &= ~DAISY_STATUS_BIT_SD_BUSY;
        break;
        case SD_CARD_READ_SECTOR:
        {
            // Decide what the length of this transfer is REG_DMA_LEN
            // Read from SD to &MenuBase[REG_DMA_RAM_ADDR]

            /**
             * @brief  Reads Sector(s)
             * @param  pdrv: Physical drive number (0..)
             * @param  *buff: Data buffer to store read data
             * @param  sector: Sector address (LBA)
             * @param  count: Number of sectors to read (1..128)
             * @retval DRESULT: Operation result*/
            MenuBase[REG_STATUS] |= DAISY_STATUS_BIT_SD_BUSY;
            MenuBase[REG_STATUS] &= ~DAISY_STATUS_BIT_SD_ERROR;
            DRESULT result = disk_read(0, (BYTE*)&MenuBase[REG_DMA_DATA], MenuBase[REG_DMA_RAM_ADDR], 1);
            if (result != F_OK) {
                MenuBase[REG_STATUS] |= DAISY_STATUS_BIT_SD_ERROR;
            }
            MenuBase[REG_STATUS] &= ~DAISY_STATUS_BIT_SD_BUSY;
        }
        break;
        case SD_CARD_WRITE_SECTOR:
        {
            MenuBase[REG_STATUS] |= DAISY_STATUS_BIT_SD_BUSY;
            MenuBase[REG_STATUS] &= ~DAISY_STATUS_BIT_SD_ERROR;
            DRESULT result = disk_write(0, (BYTE*)&MenuBase[REG_DMA_DATA], MenuBase[REG_DMA_RAM_ADDR], 1);
            if (result != F_OK) {
                MenuBase[REG_STATUS] |= DAISY_STATUS_BIT_SD_ERROR;
            }

            MenuBase[REG_STATUS] &= ~DAISY_STATUS_BIT_SD_BUSY;
        }
        break;
        case UPLOAD_ROM:
            MenuBase[REG_STATUS] |= DAISY_STATUS_BIT_SD_BUSY;
            LoadRom((char*)&MenuBase[REG_DMA_DATA]);
            MenuBase[REG_STATUS] &= ~DAISY_STATUS_BIT_SD_BUSY;
            // TODO: cause the reset here, notify n64 that load is done.

        break;
        case SET_SAVE_TYPE:

        break;
    }
}

extern "C"
ITCM_FUNCTION
void EXTI3_IRQHandler(void)
{
    EXTI->PR1 = DAISY_MENU_INTERRUPT;
    HandleExecute();
}

