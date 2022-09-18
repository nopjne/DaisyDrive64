#include "daisy_seed.h"
#include "stm32h7xx_hal.h"
#include "sys/system.h"
#include "stm32h7xx_hal_dma.h"
#include "n64common.h"
#include "daisydrive64.h"

DTCM_DATA DMA_HandleTypeDef DMA_Handle_Channel0;
DTCM_DATA DMA_HandleTypeDef DMA_Handle_Channel1;

BYTE *Sram4Buffer = (BYTE*)0x38000000;
uint32_t *LogBuffer = (uint32_t*)(ram + (48*1024*1024));
uint32_t *PortABuffer = (uint32_t*)Sram4Buffer;
uint32_t *PortBBuffer = (uint32_t*)(Sram4Buffer + 16);

DTCM_DATA volatile uint32_t ADInputAddress = 0;
DTCM_DATA volatile uint32_t PrefetchRead = 0;
DTCM_DATA volatile uint32_t ReadOffset = 0;
DTCM_DATA volatile uint32_t DMACount = 0;
DTCM_DATA volatile uint32_t ALE_H_Count = 0;
DTCM_DATA volatile uint32_t IntCount = 0;
DTCM_DATA volatile bool Running = false;

static void HAL_TransferError(DMA_HandleTypeDef *hdma);
static void Error_Handler(void);

typedef struct
{
  __IO uint32_t ISR;   /*!< BDMA interrupt status register */
  __IO uint32_t IFCR;  /*!< BDMA interrupt flag clear register */
} BDMA_Base_Registers;
const uint32_t StreamBaseAddress = (0x58025400UL);

__IO   uint32_t DMA_TransferErrorFlag = 0;
static void HAL_TransferError(DMA_HandleTypeDef *hdma)
{
    DMA_TransferErrorFlag = 1;
}

static void Error_Handler(void)
{
    GPIOC->BSRR = (0x1 << 7);
    while(1) {}
}

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
    // TODO: This code is here because the calculated loop at the end of this function needs these instructions to occur.
    //       Ideally the wait at the end of this function needs to become a property of a timer that sets the mode back to read.
    //       Potentially using open drain instead of push-pull. The speed of OD over PP needs to be investigated in relateion to internal pullups.
    if ((ReadOffset & 2) == 0) {
        LogBuffer[IntCount] = ADInputAddress + (ReadOffset & 511);
    } else {
        LogBuffer[IntCount] = PrefetchRead;
    }
#endif

    if ((ReadOffset & 3) == 0) {
        if ((ADInputAddress >= N64_ROM_BASE) && (ADInputAddress <= (N64_ROM_BASE + RomMaxSize))) {
            PrefetchRead = *((uint32_t*)(ram + (ADInputAddress - N64_ROM_BASE) + (ReadOffset & 511)));
        } else {
            if (ADInputAddress >= CART_DOM2_ADDR1_START && ADInputAddress <= CART_DOM2_ADDR1_END) {
                PrefetchRead = 0;
            } else if (ADInputAddress >= CART_DOM1_ADDR1_START && ADInputAddress <= CART_DOM1_ADDR1_END) {
                PrefetchRead = 0;
            } else if (ADInputAddress >= CART_DOM2_ADDR2_START && ADInputAddress <= CART_DOM2_ADDR2_END) {
                PrefetchRead = 0;
            } else if ( (ADInputAddress <= RomMaxSize) ) { // HACK: Getting addresses that are missing the high byte, happens often.
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
    if ((PortBBuffer[0] & ALE_H) == 0) {
        // Construct ADInputAddress
        ADInputAddress = (PortABuffer[1] & 0xFF) | ((PortBBuffer[1] & 0x03F0) << 4) | (PortBBuffer[1] & 0xC000) |
                        (ADInputAddress & 0xFFFF0000);
    } else {
        ADInputAddress = (PortABuffer[1] & 0xFF) | ((PortBBuffer[1] & 0x03F0) << 4) | (PortBBuffer[1] & 0xC000) |
                        (((PortABuffer[0] & 0xFF) | ((PortBBuffer[0] & 0x03F0) << 4) | (PortBBuffer[0] & 0xC000)) << 16);
    }

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