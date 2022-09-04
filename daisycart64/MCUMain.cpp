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

__IO   uint32_t DMA_TransferErrorFlag = 0;

DMA_HandleTypeDef DMA_Handle_Channel0;
DMA_HandleTypeDef DMA_Handle_Channel1;

//  Source File: C:\test\Super Mario 64 (USA).n64
//         Time: 8/16/2022 10:26 PM
// Orig. Offset: 0 / 0x00000000
//       Length: 1281 / 0x00000501 (bytes)
unsigned char rawData[16] =
{
    //0x37, 0x80, 0x40, 0x12, 0x00, 0x00, 0x0F, 0x00, 0x24, 0x80, 0x00, 0x60, 0x00, 0x00, 0x44, 0x14, 
    //0x37, 0x80, 0x40, 0x20, 0x00, 0x00, 0x0F, 0x00, 0x24, 0x80, 0x00, 0x60, 0x00, 0x00, 0x44, 0x14, 
    0x37, 0x80, 0x40, 0xFF, 0x00, 0x00, 0x0F, 0x00, 0x24, 0x80, 0x00, 0x60, 0x00, 0x00, 0x44, 0x14, 
    //1 0-7 8-16  2 0-7 8-16
    //0xFF, 0x01, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x00, 
} ;


//#define MENU_ROM_FILE_NAME "menu.z64"
#define MENU_ROM_FILE_NAME "Super Mario 64 (USA).n64"
//#define MENU_ROM_FILE_NAME "Resident Evil 2 (USA).n64"

#define N64_ROM_BASE 0x10000000
using namespace daisy;
BYTE *Sram4Buffer = (BYTE*)0x38000000;
int8_t *ram = (int8_t *)0xC0000000;
uint32_t *LogBuffer = (uint32_t*)(ram + (8*1024*1024));
uint32_t *PortABuffer = (uint32_t*)Sram4Buffer;
uint32_t *PortBBuffer = (uint32_t*)(Sram4Buffer + 8);

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

#define GP_SPEED GPIO_SPEED_FREQ_VERY_HIGH
volatile uint32_t ADInputAddress = 0;
volatile uint32_t PrefetchRead = 0;
volatile uint32_t ReadOffset = 0;
uint32_t ReadCount = 0;
//static void HalfTransferComplete(DMA_HandleTypeDef *hdma);
static void HAL_TransferError(DMA_HandleTypeDef *hdma);
static void Error_Handler(void);

int InitializeDmaChannels(void)
{
    HAL_DMA_MuxRequestGeneratorConfigTypeDef dmamux_ReqGenParams  = {0};

    /*##-2- Configure the DMA ##################################################*/
    /* Enable BDMA clock */
    __HAL_RCC_BDMA_CLK_ENABLE();

    { // Channel 0 init.
        /* Configure the DMA handler for Transmission process     */
        /* DMA mode is set to circular for an infinite DMA transfer */
        DMA_Handle_Channel0.Instance                 = BDMA_Channel0;
        DMA_Handle_Channel0.Init.Request             = BDMA_REQUEST_GENERATOR0;
        DMA_Handle_Channel0.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        DMA_Handle_Channel0.Init.PeriphInc           = DMA_PINC_DISABLE;
        DMA_Handle_Channel0.Init.MemInc              = DMA_MINC_ENABLE;
        DMA_Handle_Channel0.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
        DMA_Handle_Channel0.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
        DMA_Handle_Channel0.Init.Mode                = DMA_CIRCULAR;
        DMA_Handle_Channel0.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
        DMA_Handle_Channel0.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
        DMA_Handle_Channel0.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_HALFFULL;
        DMA_Handle_Channel0.Init.MemBurst            = DMA_MBURST_SINGLE;
        DMA_Handle_Channel0.Init.PeriphBurst         = DMA_PBURST_SINGLE;

        /* Initialize the DMA with for Transmission process */
        HAL_StatusTypeDef dmares = HAL_OK;
        dmares = HAL_DMA_Init(&DMA_Handle_Channel0);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        /* Select Callbacks functions called after Transfer complete and Transfer error */
        dmares = HAL_DMA_RegisterCallback(&DMA_Handle_Channel0, HAL_DMA_XFER_CPLT_CB_ID, NULL);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        dmares = HAL_DMA_RegisterCallback(&DMA_Handle_Channel0, HAL_DMA_XFER_ERROR_CB_ID, HAL_TransferError);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

                /* NVIC configuration for DMA transfer complete interrupt*/
        HAL_NVIC_SetPriority(BDMA_Channel0_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(BDMA_Channel0_IRQn);

        /*##-3- Configure and enable the DMAMUX Request generator  ####################*/
        dmamux_ReqGenParams.SignalID  = HAL_DMAMUX2_REQ_GEN_EXTI0;
        dmamux_ReqGenParams.Polarity  = HAL_DMAMUX_REQ_GEN_RISING_FALLING;
        dmamux_ReqGenParams.RequestNumber = 1;                          /* 1 requests on each edge of the external request signal  */

        dmares = HAL_DMAEx_ConfigMuxRequestGenerator(&DMA_Handle_Channel0, &dmamux_ReqGenParams);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        /* NVIC configuration for DMAMUX request generator overrun errors*/
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

    { // Channel 1 init.
        /* Configure the DMA handler for Transmission process     */
        /* DMA mode is set to circular for an infinite DMA transfer */
        DMA_Handle_Channel1.Instance                 = BDMA_Channel1;
        DMA_Handle_Channel1.Init.Request             = BDMA_REQUEST_GENERATOR0;
        DMA_Handle_Channel1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        DMA_Handle_Channel1.Init.PeriphInc           = DMA_PINC_DISABLE;
        DMA_Handle_Channel1.Init.MemInc              = DMA_MINC_ENABLE;
        DMA_Handle_Channel1.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        DMA_Handle_Channel1.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
        DMA_Handle_Channel1.Init.Mode                = DMA_CIRCULAR;
        DMA_Handle_Channel1.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
        DMA_Handle_Channel1.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
        DMA_Handle_Channel1.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_HALFFULL;
        DMA_Handle_Channel1.Init.MemBurst            = DMA_MBURST_SINGLE;
        DMA_Handle_Channel1.Init.PeriphBurst         = DMA_PBURST_SINGLE;

        /* Initialize the DMA with for Transmission process */
        HAL_StatusTypeDef dmares = HAL_OK;
        dmares = HAL_DMA_Init(&DMA_Handle_Channel1);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        /* Select Callbacks functions called after Transfer complete and Transfer error */
        dmares = HAL_DMA_RegisterCallback(&DMA_Handle_Channel1, HAL_DMA_XFER_CPLT_CB_ID, NULL);
        if (dmares != HAL_OK) {
            Error_Handler();
        }
        dmares = HAL_DMA_RegisterCallback(&DMA_Handle_Channel1, HAL_DMA_XFER_ERROR_CB_ID, HAL_TransferError);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        /* NVIC configuration for DMA transfer complete interrupt*/
        HAL_NVIC_SetPriority(BDMA_Channel1_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(BDMA_Channel1_IRQn);

        /*##-3- Configure and enable the DMAMUX Request generator  ####################*/
        dmamux_ReqGenParams.SignalID  = HAL_DMAMUX2_REQ_GEN_EXTI0;
        dmamux_ReqGenParams.Polarity  = HAL_DMAMUX_REQ_GEN_RISING_FALLING;
        dmamux_ReqGenParams.RequestNumber = 1;

        dmares = HAL_DMAEx_ConfigMuxRequestGenerator(&DMA_Handle_Channel1, &dmamux_ReqGenParams);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        /* NVIC configuration for DMAMUX request generator overrun errors*/
        HAL_NVIC_SetPriority(DMAMUX2_OVR_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(DMAMUX2_OVR_IRQn);

        dmares = HAL_DMAEx_EnableMuxRequestGenerator (&DMA_Handle_Channel1);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        volatile uint32_t dmares1 = HAL_DMA_Start_IT(&DMA_Handle_Channel1, (uint32_t)&(GPIOA->IDR), (uint32_t)(PortABuffer), 2);
        if (dmares1 != HAL_OK) {
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
#define ALE_H (1 << 11)

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

volatile uint32_t DMACount = 0;

extern "C" void EXTI15_10_IRQHandler(void)
{
    EXTI->PR1 = 0x00000800;
    GPIOA->MODER = 0xABFF0000;
    GPIOB->MODER = 0x0CB000B3;
}

volatile uint32_t IntCount = 0;
extern "C" void EXTI1_IRQHandler(void)
{
    EXTI->PR1 = 0x00000002;
    if ((ReadOffset & 2) == 0) {
        LogBuffer[IntCount] = ADInputAddress | (ReadOffset & 511);
    } else {
        LogBuffer[IntCount] = PrefetchRead;
    }

    if ((ReadOffset & 3) == 0) {
        PrefetchRead = *((uint32_t*)(ram + (ADInputAddress - N64_ROM_BASE) + (ReadOffset & 511)));
        // Switch to output.
        GPIOA->MODER = 0xABFF5555;
        GPIOB->MODER = 0x5CB555B3;
    }

    // Value can be DMA-ed directly from ram.
    uint32_t Value = (((ReadOffset & 2) == 0) ? PrefetchRead : (PrefetchRead >> 16));
    uint32_t OutB = (((Value >> 4) & 0x03F0) | (Value & 0xC000));
    GPIOA->ODR = Value;
    GPIOB->ODR = OutB;

    if (ReadOffset == 512) {
        volatile uint32_t x = 33;
        while (x) { x--; }
        GPIOA->MODER = 0xABFF0000;
        GPIOB->MODER = 0x0CB000B3;
    }

    ReadOffset += 2;
    IntCount += 1;
}

extern "C" void BDMA_Channel1_IRQHandler(void)
{
    ((BDMA_Base_Registers *)(DMA_Handle_Channel1.StreamBaseAddress))->IFCR = 1 << 4;
    const uint32_t DoOp = (DMACount += 1);
    if ((DoOp & 1) == 0) {
        SCB_InvalidateDCache_by_Addr(PortABuffer, 16);
        // Construct ADInputAddress
        ADInputAddress = (PortABuffer[1] & 0xFF) | ((PortBBuffer[1] & 0x03F0) << 4) | (PortBBuffer[1] & 0xC000) |
                       (((PortABuffer[0] & 0xFF) | ((PortBBuffer[0] & 0x03F0) << 4) | (PortBBuffer[0] & 0xC000)) << 16);

        ReadOffset = 0;
    }
}

extern "C" void BDMA_Channel0_IRQHandler(void)
{
    ((BDMA_Base_Registers *)(DMA_Handle_Channel0.StreamBaseAddress))->IFCR = 1;
    const uint32_t DoOp = (DMACount += 1);
    if ((DoOp & 1) == 0) {
        SCB_InvalidateDCache_by_Addr(PortABuffer, 16);
        // Construct ADInputAddress
        ADInputAddress = (PortABuffer[1] & 0xFF) | ((PortBBuffer[1] & 0x03F0) << 4) | (PortBBuffer[1] & 0xC000) |
                       (((PortABuffer[0] & 0xFF) | ((PortBBuffer[0] & 0x03F0) << 4) | (PortBBuffer[0] & 0xC000)) << 16);

        ReadOffset = 0;
    }
}

int main(void)
{
    size_t bytesread = 0;

    // Init hardware
    hw.Init(true);

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    // Preconfigure GPIO PortA and PortB, so the following directional changes can be faster.
    GPIO_InitTypeDef PortAPins = {0xFF, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GP_SPEED, 0};
    GPIO_InitTypeDef PortBPins = {0xC3F0, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOA, &PortAPins);
    HAL_GPIO_Init(GPIOB, &PortBPins);
    
    PortAPins = {0xFF, GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    PortBPins = {0xC3F0, GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOA, &PortAPins);
    HAL_GPIO_Init(GPIOB, &PortBPins);

    PortBPins = {(1 << 1) | (1 << 12), GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOB, &PortBPins);

    GPIO_InitTypeDef PortCPins = {(1 << 0) | (1 << 1), GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOC, &PortCPins);

    PortCPins = {(1 << 7), GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOC, &PortCPins);

    GPIO_InitTypeDef PortDPins = {(1 << 11), GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOD, &PortDPins);

    GPIO_InitTypeDef PortGPins = {(1 << 10) | (1 << 11), GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    PortGPins.Pin |= (1 << 9);
    HAL_GPIO_Init(GPIOG, &PortGPins);

    GPIOC->ODR = (0x1 << 7);

    SdmmcHandler::Config sd_cfg;
    sd_cfg.Defaults();
    sd_cfg.speed = SdmmcHandler::Speed::STANDARD;
    {
        sd.Init(sd_cfg);

        // Links libdaisy i/o to fatfs driver.
        fsi.Init(FatFSInterface::Config::MEDIA_SD);

        // Mount SD Card
        if (f_mount(&fsi.GetSDFileSystem(), "/", 1) != FR_OK) {
            while(1) {
                GPIOC->BSRR = (0x1 << 7);
                System::Delay(500);
                GPIOC->BSRR = (0x1 << 7) << 16;
                System::Delay(100);
            }
        }

        // Open and write the test file to the SD Card. Needed when supporting write.
        //if(f_open(&SDFile, MENU_ROM_FILE_NAME, (FA_CREATE_ALWAYS) | (FA_WRITE)) == FR_OK) {
        //    f_write(&SDFile, outbuff, len, &byteswritten);
        //    f_close(&SDFile);
        //}

        // Read the menu rom from the SD Card.
        if(f_open(&SDFile, MENU_ROM_FILE_NAME, FA_READ) == FR_OK) {
            FILINFO FileInfo;
            FRESULT result = f_stat(MENU_ROM_FILE_NAME, &FileInfo);
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
            //memset(ram, 0xFF, FileInfo.fsize);
        } else {
            BlinkAndDie(100, 100);
        }
    }
    // No led on on success.
    GPIOC->BSRR = (0x1 << 7) << 16;

    // Patch speed.
    memcpy(ram, rawData, 16);

    // Read interrupt setup.
    //NVIC_SetVector(EXTI1_IRQn, (uint32_t)&__EXTI0_IRQHandler);
    //LPTIM_Config();
    // Disable LP3 timer.
    //DISABLE_LP3_TIMER();

    // Wait for reset line high.
    while ((GPIOB->IDR & (1 << 12)) == 0) { }
    System::Delay(2);
    while ((GPIOB->IDR & (1 << 12)) == 0) { }

    // Disable systick interrupts. (Needs to be re-enabled for SDCard reads)
    SysTick->CTRL = 0;
    InitializeDmaChannels();

    GPIO_InitTypeDef GPIO_InitStruct;
    // ALEH interrupt setup. -- 
    // This interrupt is used to indicate that ALE_H high, and the mode needs to switch to input.
    GPIO_InitStruct = {ALE_H, GPIO_MODE_IT_RISING, GPIO_NOPULL, GP_SPEED, 0};
    NVIC_SetVector(EXTI15_10_IRQn, (uint32_t)&EXTI15_10_IRQHandler);
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
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
    
#endif

#if 0
    constexpr Pin D25 = Pin(PORTA, 0); // AD0  // Could be used for DMA
    constexpr Pin D24 = Pin(PORTA, 1); // AD1
    constexpr Pin D28 = Pin(PORTA, 2); // AD2  // Could be used for DMA
    constexpr Pin D16 = Pin(PORTA, 3); // AD3
    constexpr Pin D23 = Pin(PORTA, 4); // AD4
    constexpr Pin D22 = Pin(PORTA, 5); // AD5
    constexpr Pin D19 = Pin(PORTA, 6); // AD6
    constexpr Pin D18 = Pin(PORTA, 7); // AD7

    constexpr Pin D17 = Pin(PORTB, 1); // Write.
    constexpr Pin D9  = Pin(PORTB, 4); // AD8
    constexpr Pin D10 = Pin(PORTB, 5); // AD9
    constexpr Pin D13 = Pin(PORTB, 6); // AD10
    constexpr Pin D14 = Pin(PORTB, 7); // AD11
    constexpr Pin D11 = Pin(PORTB, 8); // AD12
    constexpr Pin D12 = Pin(PORTB, 9); // AD13
    constexpr Pin D0  = Pin(PORTB, 12); // N.C
    constexpr Pin D29 = Pin(PORTB, 14); // AD14
    constexpr Pin D30 = Pin(PORTB, 15); // AD15

    constexpr Pin D15 = Pin(PORTC, 0); // ALE_L -- // Read  // Could be used for DMA
    constexpr Pin D20 = Pin(PORTC, 1); // Read -- N.C -- Cold reset needs to be hooked up..
    constexpr Pin D21 = Pin(PORTC, 4); // S-DATA
    constexpr Pin D4  = Pin(PORTC, 8); // SD card D0
    constexpr Pin D3  = Pin(PORTC, 9); // SD card D1
    constexpr Pin D2  = Pin(PORTC, 10); // SD card D2
    constexpr Pin D1  = Pin(PORTC, 11); // SD card D3
    constexpr Pin D6  = Pin(PORTC, 12); // SD card CLK

    constexpr Pin D5  = Pin(PORTD, 2);  // SD card CMD  // Could be used for DMA
    constexpr Pin D26 = Pin(PORTD, 11); // ALE_H

    constexpr Pin D27 = Pin(PORTG, 9);  // CIC_D1
    constexpr Pin D7  = Pin(PORTG, 10); // CIC_D2
    constexpr Pin D8  = Pin(PORTG, 11); // N64_NMI
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
    while(1) {

    }
}