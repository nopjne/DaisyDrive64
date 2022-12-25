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
uint32_t *MenuBase = (uint32_t*)(FlashRamStorage + 32770);

using namespace daisy;
SdmmcHandler::Config sd_cfg;
extern SdmmcHandler   sd;

void InitMenuFunctions(void)
{
    // Setup interrupt priority, needs to be below the SysTick priority otherwise SD operations it will cause deadlocks.
    HAL_NVIC_SetPriority(EXTI3_IRQn, 15, 1);
    NVIC_SetVector(EXTI3_IRQn, (uint32_t)&EXTI3_IRQHandler);
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

#define SWAP_WORDS(x) ((((x) & 0xFFFF) << 16) | (((x) & 0xFFFF0000) >> 16))
BYTE testTemp[512];
uint32_t xSector[40];
uint32_t xCount;
inline void HandleExecute(void)
{
    uint32_t operation = SWAP_WORDS(MenuBase[REG_EXECUTE_FUNCTION]);
    //uint32_t operation = MenuBase[REG_EXECUTE_FUNCTION];
    switch(operation) {
        case GET_FW_VERSION:
            MenuBase[REG_STATUS] |= DAISY_STATUS_BIT_DMA_BUSY;
            MenuBase[REG_FUNCTION_PARAMETER] = SWAP_WORDS(0x00000666);
        break;
        case ENABLE_MENU_FUNCTIONS:
            MenuBase[REG_STATUS] |= DAISY_STATUS_BIT_SD_BUSY;
            EnableMenu();
        break;
        case DISABLE_MENU_FUNCTIONS:
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
            uint32_t temp;
            MenuBase[REG_DMA_RAM_ADDR] = SWAP_WORDS(MenuBase[REG_DMA_RAM_ADDR]);
            DRESULT result = disk_read(0, testTemp, MenuBase[REG_DMA_RAM_ADDR], 1);
            memcpy(&MenuBase[REG_DMA_DATA], testTemp, 512);
            uint32_t* Ptr = &MenuBase[REG_DMA_DATA];
            for (int i = 0; i < 128; i += 1) {
                 temp = __bswap32(*Ptr);
                 *Ptr = temp;
                 temp = SWAP_WORDS(*Ptr);
                 *Ptr = temp;
                 Ptr += 1;
            }

            if (result != F_OK) {
                MenuBase[REG_STATUS] |= DAISY_STATUS_BIT_SD_ERROR;
            }
        }
        break;
        case SD_CARD_WRITE_SECTOR:
        {
            uint32_t temp;
            MenuBase[REG_STATUS] |= DAISY_STATUS_BIT_SD_BUSY;
            MenuBase[REG_STATUS] &= ~DAISY_STATUS_BIT_SD_ERROR;
            uint32_t* Ptr = &MenuBase[REG_DMA_DATA];
            for (int i = 0; i < 128; i += 1) {
                *Ptr = SWAP_WORDS(*Ptr);
                temp = __bswap32(*Ptr);
                *Ptr = temp;
                Ptr += 1;
            }

            MenuBase[REG_DMA_RAM_ADDR] = SWAP_WORDS(MenuBase[REG_DMA_RAM_ADDR]);
            DRESULT result = disk_write(0, (BYTE*)&MenuBase[REG_DMA_DATA], MenuBase[REG_DMA_RAM_ADDR], 1);
            if (result != F_OK) {
                MenuBase[REG_STATUS] |= DAISY_STATUS_BIT_SD_ERROR;
            }
        }
        break;
        case UPLOAD_ROM:
        {
            MenuBase[REG_STATUS] |= DAISY_STATUS_BIT_SD_BUSY;
            uint32_t* Ptr = &MenuBase[REG_DMA_DATA];
            for (int i = 0; i < 128; i += 1) {
                *Ptr = SWAP_WORDS(*Ptr);
                Ptr += 1;
            }

            LoadRom((char*)&MenuBase[REG_DMA_DATA]);
            // TODO: cause the reset here, notify n64 that load is done.
        }
        break;
        case SET_SAVE_TYPE:

        break;
        default:
        while(1) {}
        break;
    }

    MenuBase[REG_STATUS] &= ~(DAISY_STATUS_BIT_DMA_BUSY | DAISY_STATUS_BIT_SD_BUSY);
}

//volatile uint32_t xHandler = 0;
//uint32_t xSave[20];
extern "C"
ITCM_FUNCTION
void EXTI3_IRQHandler(void)
{
     EXTI->PR1 = DAISY_MENU_INTERRUPT;
     //xSave[xHandler] = MenuBase[REG_EXECUTE_FUNCTION];
     //xHandler += 1;
     HandleExecute();
}

