#include "daisy_seed.h"
#include "stm32h7xx_hal.h"
#include "sys/system.h"
#include "stm32h7xx_hal_dma.h"
#include "n64common.h"
#include "daisydrive64.h"
#include "stm32h7xx_hal_tim.h"

#define SI_RESET 0xFF // Tx:1 Rx:3
#define SI_INFO  0x00 // Tx:1 Rx:3
#define EEPROM_READ 0x04 // Tx:2 Rx:8
#define EEPROM_STORE  0x05 // Tx:10 Rx:1 
//#define LOG_EEPROM_BYTES 1
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

BYTE *const EepromInputLog = (BYTE*)(LogBuffer - 1024 * 1024);
DTCM_DATA uint32_t EepLogIdx = 0;
BYTE EEPROMStore[2048]; // 16KiBit
BYTE EEPROMType = 0x80;

#if (SI_USE_DMA != 0)
DTCM_DATA TIM_HandleTypeDef htim3;
DTCM_DATA DMA_HandleTypeDef hdma_tim3_ch4;
DTCM_DATA DMA_HandleTypeDef hdma_tim3_ch4_out;
DTCM_DATA DMA_HandleTypeDef hdma_tim3_input_capture_configure;
SRAM1_DATA uint16_t SI_DMAOutputBuffer[SI_RINGBUFFER_LENGTH];
DTCM_DATA uint32_t SI_DMAOutputLength;
SRAM1_DATA uint16_t SDataBuffer[SI_RINGBUFFER_LENGTH];
volatile DTCM_DATA uint32_t LastTransferCount;
volatile uint32_t Start;
SRAM1_DATA uint16_t Tim3InputSetting[] = {
    0x1, // CR1
    0x0, // CR2
    0x0, // SMCR
    0x1000, // DIER
    0xf, // SR
    0x0, // EGR
    0x0, // CCMR1
    0x2100, // CCMR2
    0xb000, // CCER
    0x0, // CNT
    0x86, // PSC
    0xffff, // ARR
    0x0, // RCR
    0x0, // CCR1
    0x0, // CCR2
    0x0, // CCR3
    0x0, // CCR4
    0x0, // BDTR
    0x0, // DCR
    0x1, // DMAR
    0x0, // Reserved
    0x0, // CCMR3
    0x0, // CCR5
    0x0, // CCR6
    0x0, // AF1
    0x0, // AF2
    0x0  // TISEL
};

typedef struct
{
  __IO uint32_t ISR;   /*!< DMA interrupt status register */
  __IO uint32_t Reserved0;
  __IO uint32_t IFCR;  /*!< DMA interrupt flag clear register */
} DMA_Base_Registers;
#else
volatile DTCM_DATA uint32_t OldTimeStamp;

#endif

#define ITCM_FUNCTION __attribute__((long_call, section(".itcm_text")))

void Error_Handler()
{
    GPIOC->BSRR = (1 << 7);
    while (1) {}
}

void ITCM_FUNCTION SI_Enable(void)
{
    if ((htim3.Instance->CR1 & 1) == 0) {
        (((DMA_Stream_TypeDef *)(hdma_tim3_ch4.Instance))->CR) |= DMA_SxCR_EN;
        __HAL_TIM_ENABLE_DMA(&htim3, TIM_DMA_CC4);
        htim3.Instance->CCER = 0xb000;
        htim3.Instance->CR1 = 1;
    }
}

void ITCM_FUNCTION SI_Reset(void) 
{
    __HAL_TIM_DISABLE_DMA(&htim3, TIM_DMA_CC4);
    htim3.Instance->CR1 = 0;
    (((DMA_Stream_TypeDef *)(hdma_tim3_ch4.Instance))->CR) &= ~DMA_SxCR_EN;
    htim3.Instance->DIER = 0x1000; // DIER
    htim3.Instance->SR = 0xf, // SR
    htim3.Instance->ARR = 65535;
    htim3.Instance->CCMR2 = 0x2100;
#if 1
    TIM_IC_InitTypeDef sConfigIC = {0};
    sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_BOTHEDGE;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    sConfigIC.ICFilter = 2;
    if (HAL_TIM_IC_ConfigChannel(&htim3, &sConfigIC, TIM_CHANNEL_4) != HAL_OK)
    {
        Error_Handler();
    }

    // /* Disable the Channel 4: Reset the CC4E Bit */
    // TIMx->CCER &= ~TIM_CCER_CC4E;
    // tmpccmr2 = TIMx->CCMR2;
    // tmpccer = TIMx->CCER;
// 
    // /* Select the Input */
    // tmpccmr2 &= ~TIM_CCMR2_CC4S;
    // tmpccmr2 |= (TIM_ICSelection << 8U);
// 
    // /* Set the filter */
    // tmpccmr2 &= ~TIM_CCMR2_IC4F;
    // tmpccmr2 |= ((TIM_ICFilter << 12U) & TIM_CCMR2_IC4F);
// 
    // /* Select the Polarity and set the CC4E Bit */
    // tmpccer &= ~(TIM_CCER_CC4P | TIM_CCER_CC4NP);
    // tmpccer |= ((TIM_ICPolarity << 12U) & (TIM_CCER_CC4P | TIM_CCER_CC4NP));
// 
    // /* Write to TIMx CCMR2 and CCER registers */
    // TIMx->CCMR2 = tmpccmr2;
    // TIMx->CCER = tmpccer ;
// 
    // htim->Instance->CCMR2 &= ~TIM_CCMR2_IC4PSC;
// 
    // /* Set the IC4PSC value */
    // htim->Instance->CCMR2 |= (sConfig->ICPrescaler << 8U);
#endif 
    ((DMA_Base_Registers *)(hdma_tim3_ch4.StreamBaseAddress))->IFCR = 0x3FUL << (hdma_tim3_ch4.StreamIndex & 0x1FU);
    LastTransferCount = SI_RINGBUFFER_LENGTH;
    (((DMA_Stream_TypeDef *)hdma_tim3_ch4.Instance)->NDTR) = SI_RINGBUFFER_LENGTH;
}

volatile int intcount = 0;
extern "C" void ITCM_FUNCTION DMA1_Stream7_IRQHandler(void)
{
    // Clear the interrupt bit.
    (((DMA_Stream_TypeDef *)(hdma_tim3_input_capture_configure.Instance))->CR) &= ~DMA_SxCR_EN;
    ((DMA_Base_Registers *)(hdma_tim3_input_capture_configure.StreamBaseAddress))->IFCR = 0xC300000;
    (((DMA_Stream_TypeDef *)(hdma_tim3_ch4_out.Instance))->CR) &= ~DMA_SxCR_EN;
    
    __HAL_TIM_DISABLE_DMA(&htim3, TIM_DMA_CC4);
    htim3.Instance->CR1 = 0;

    htim3.Instance->DIER = 0x1000; // DIER
    htim3.Instance->SR = 0xf, // SR
    htim3.Instance->ARR = 65535;
    htim3.Instance->CCMR2 = 0x2100;
#if 1
    TIM_IC_InitTypeDef sConfigIC = {0};
    sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_BOTHEDGE;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    sConfigIC.ICFilter = 2;
    if (HAL_TIM_IC_ConfigChannel(&htim3, &sConfigIC, TIM_CHANNEL_4) != HAL_OK)
    {
        Error_Handler();
    }

    // /* Disable the Channel 4: Reset the CC4E Bit */
    // TIMx->CCER &= ~TIM_CCER_CC4E;
    // tmpccmr2 = TIMx->CCMR2;
    // tmpccer = TIMx->CCER;
// 
    // /* Select the Input */
    // tmpccmr2 &= ~TIM_CCMR2_CC4S;
    // tmpccmr2 |= (TIM_ICSelection << 8U);
// 
    // /* Set the filter */
    // tmpccmr2 &= ~TIM_CCMR2_IC4F;
    // tmpccmr2 |= ((TIM_ICFilter << 12U) & TIM_CCMR2_IC4F);
// 
    // /* Select the Polarity and set the CC4E Bit */
    // tmpccer &= ~(TIM_CCER_CC4P | TIM_CCER_CC4NP);
    // tmpccer |= ((TIM_ICPolarity << 12U) & (TIM_CCER_CC4P | TIM_CCER_CC4NP));
// 
    // /* Write to TIMx CCMR2 and CCER registers */
    // TIMx->CCMR2 = tmpccmr2;
    // TIMx->CCER = tmpccer ;
// 
    // htim->Instance->CCMR2 &= ~TIM_CCMR2_IC4PSC;
// 
    // /* Set the IC4PSC value */
    // htim->Instance->CCMR2 |= (sConfig->ICPrescaler << 8U);
#endif 
    ((DMA_Base_Registers *)(hdma_tim3_ch4.StreamBaseAddress))->IFCR = 0x3FUL << (hdma_tim3_ch4.StreamIndex & 0x1FU);
    LastTransferCount = SI_RINGBUFFER_LENGTH;
    (((DMA_Stream_TypeDef *)hdma_tim3_ch4.Instance)->NDTR) = SI_RINGBUFFER_LENGTH;
    (((DMA_Stream_TypeDef *)(hdma_tim3_ch4.Instance))->CR) |= DMA_SxCR_EN;
    __HAL_TIM_ENABLE_DMA(&htim3, TIM_DMA_CC4);
    htim3.Instance->CCER = 0xb000;
    htim3.Instance->CR1 = 1;
    intcount += 1;
}

void InitializeTimersSI(void)
{
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_IC_InitTypeDef sConfigIC = {0};

    htim3.Instance = TIM3;
#if OVERCLOCK
    htim3.Init.Prescaler = 135 - 1; // 2 ticks per 1us
#else
    htim3.Init.Prescaler = 120; // 2 ticks per 1us
#endif

    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 65535;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
    {
        Error_Handler();
    }
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_TIM_IC_Init(&htim3) != HAL_OK)
    {
        Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_BOTHEDGE;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    sConfigIC.ICFilter = 2;
    if (HAL_TIM_IC_ConfigChannel(&htim3, &sConfigIC, TIM_CHANNEL_4) != HAL_OK)
    {
        Error_Handler();
    }

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_TIM3_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    hdma_tim3_ch4.Instance = DMA1_Stream5;
    hdma_tim3_ch4.Init.Request = DMA_REQUEST_TIM3_CH4;
    hdma_tim3_ch4.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_tim3_ch4.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_tim3_ch4.Init.MemInc = DMA_MINC_ENABLE;
    hdma_tim3_ch4.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_tim3_ch4.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_tim3_ch4.Init.Mode = DMA_CIRCULAR;
    hdma_tim3_ch4.Init.Priority = DMA_PRIORITY_LOW;
    hdma_tim3_ch4.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    hdma_tim3_ch4.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    hdma_tim3_ch4.Init.MemBurst = DMA_MBURST_SINGLE;
    hdma_tim3_ch4.Init.PeriphBurst = DMA_PBURST_SINGLE;
    if (HAL_DMA_Init(&hdma_tim3_ch4) != HAL_OK)
    {
        Error_Handler();
    }
    NVIC_SetVector(DMA1_Stream5_IRQn, (uint32_t)&DMA1_Stream7_IRQHandler);

    hdma_tim3_ch4_out.Instance = DMA1_Stream6;
    hdma_tim3_ch4_out.Init.Request = DMA_REQUEST_TIM3_CH4;
    hdma_tim3_ch4_out.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_tim3_ch4_out.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_tim3_ch4_out.Init.MemInc = DMA_MINC_ENABLE;
    hdma_tim3_ch4_out.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_tim3_ch4_out.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_tim3_ch4_out.Init.Mode = DMA_NORMAL;
    hdma_tim3_ch4_out.Init.Priority = DMA_PRIORITY_LOW;
    hdma_tim3_ch4_out.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    hdma_tim3_ch4_out.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    hdma_tim3_ch4_out.Init.MemBurst = DMA_MBURST_SINGLE;
    hdma_tim3_ch4_out.Init.PeriphBurst = DMA_PBURST_SINGLE;
    if (HAL_DMA_Init(&hdma_tim3_ch4_out) != HAL_OK)
    {
        Error_Handler();
    }

    // Configure the DMA for output.
    NVIC_SetVector(DMA1_Stream6_IRQn, (uint32_t)&DMA1_Stream7_IRQHandler);
    HAL_NVIC_DisableIRQ(DMA1_Stream6_IRQn);
    (((DMA_Stream_TypeDef *)(hdma_tim3_ch4_out.Instance))->PAR) = (uint32_t)&(htim3.Instance->CCR4);
    (((DMA_Stream_TypeDef *)(hdma_tim3_ch4_out.Instance))->M0AR) = (uint32_t)&(SI_DMAOutputBuffer[0]);
    (((DMA_Stream_TypeDef *)(hdma_tim3_ch4_out.Instance))->CR) &= ~DMA_SxCR_EN;

    hdma_tim3_input_capture_configure.Instance = DMA1_Stream7;
    hdma_tim3_input_capture_configure.Init.Request = DMA_REQUEST_TIM3_CH3;
    hdma_tim3_input_capture_configure.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_tim3_input_capture_configure.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_tim3_input_capture_configure.Init.MemInc = DMA_MINC_ENABLE;
    hdma_tim3_input_capture_configure.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_tim3_input_capture_configure.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_tim3_input_capture_configure.Init.Mode = DMA_NORMAL;
    hdma_tim3_input_capture_configure.Init.Priority = DMA_PRIORITY_LOW;
    hdma_tim3_input_capture_configure.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    hdma_tim3_input_capture_configure.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    hdma_tim3_input_capture_configure.Init.MemBurst = DMA_MBURST_SINGLE;
    hdma_tim3_input_capture_configure.Init.PeriphBurst = DMA_PBURST_SINGLE;
    if (HAL_DMA_Init(&hdma_tim3_input_capture_configure) != HAL_OK)
    {
        Error_Handler();
    }

    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = TIM_OCMODE_TIMING;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_SET;
    sConfigOC.OCFastMode = TIM_OCFAST_ENABLE;
    sConfigOC.Pulse = 0xFFFF;
    if (HAL_TIM_OC_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(DMA1_Stream7_IRQn, 15, 0);
    NVIC_SetVector(DMA1_Stream7_IRQn, (uint32_t)&DMA1_Stream7_IRQHandler);
    HAL_NVIC_EnableIRQ(DMA1_Stream7_IRQn);
    (((DMA_Stream_TypeDef *)(hdma_tim3_input_capture_configure.Instance))->PAR) = (uint32_t)&(htim3.Instance->DMAR);
    (((DMA_Stream_TypeDef *)(hdma_tim3_input_capture_configure.Instance))->M0AR) = (uint32_t)&(Tim3InputSetting[0]);
    (((DMA_Stream_TypeDef *)(hdma_tim3_input_capture_configure.Instance))->CR) &= ~DMA_SxCR_EN;

    // Enable the timer and DMA.
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_LINKDMA(&htim3, hdma[TIM_DMA_ID_CC4], hdma_tim3_ch4);
    HAL_TIM_IC_Start_DMA(&htim3, TIM_CHANNEL_4, (uint32_t*)&(SDataBuffer[0]), SI_RINGBUFFER_LENGTH);

    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 15, 15);
    HAL_NVIC_DisableIRQ(DMA1_Stream5_IRQn);
    LastTransferCount = SI_RINGBUFFER_LENGTH;

    // Configure a timer to timeout on SI output completion and put TIM3 back in input mode.
    const uint16_t Tim3InputSettingLocal[] = {
        0x0, // CR1
        0x0, // CR2
        0x0, // SMCR
        0x1800, // DIER
        0x0, // SR
        0x0, // EGR
        0x0, // CCMR1
        0x2100, // CCMR2
        0xb000, // CCER
        0x0, // CNT
        0x86, // PSC6
        0xffff, // ARR
        0x0, // RCR
        0x0, // CCR1
        0x0, // CCR2
        0x0, // CCR3
        0x0, // CCR4
        0x0, // BDTR
        0x1, // DCR
        0x1, // DMAR
        0x0, // Reserved
        0x0, // CCMR3
        0x0, // CCR5
        0x0, // CCR6
        0x0, // AF1
        0x0, // AF2
        0x0  // TISEL
    };

    memcpy(Tim3InputSetting, Tim3InputSettingLocal, sizeof(Tim3InputSettingLocal));
    SCB_CleanInvalidateDCache_by_Addr((uint32_t*)&(Tim3InputSetting[0]), sizeof(Tim3InputSetting));
}

#if (SI_USE_DMA == 0)
// 0  = 0,0,0,1
// 1  = 0,1,1,1
// DS = 0,0,1,1
// CS = 0,1,1 
inline bool SIGetBytes(BYTE *Out, uint32_t ExpectedBytes, bool Block)
{
    volatile uint32_t TimeStamp;
    uint32_t TransferTime = 540; // 540 * 1000 * 1000 / 1000 * 1000 = 1us.
    volatile BYTE BitCount = 0;
    *Out = 0;

    // wait for sdat low
    while ((Block != false) && ((GPIOC->IDR & (S_DAT_LINE)) != 0)) {}

    TimeStamp = DWT->CYCCNT;
    OldTimeStamp = TimeStamp;
    while ((Running != false) && (BitCount < (8 * ExpectedBytes))) {
        // Wait for 2us.
        while (((TimeStamp - OldTimeStamp) < (TransferTime * 2))) {
            TimeStamp = DWT->CYCCNT;
        }

        *Out <<= 1;
        *Out |= ((GPIOC->IDR & (S_DAT_LINE)) != 0) ? 1 : 0;
        BitCount += 1;

        if ((BitCount % 8) == 0) {
#ifdef LOG_EEPROM_BYTES
            EepromInputLog[EepLogIdx++ % (1024 * 1024)] = *Out;
#endif
            if (BitCount < (8 * ExpectedBytes)) {
                Out += 1;
                *Out = 0;
            }
        }

        // Wait for next bit.
        volatile uint32_t timeout = 900;
        // Wait while line low. (wait for high)
        while ((timeout--) && ((GPIOC->IDR & (S_DAT_LINE)) == 0)) {}

        if (timeout != 0) {
            timeout = 900;
            // Wait while high. (Wait for low)
            while ((timeout--) && ((GPIOC->IDR & (S_DAT_LINE)) != 0)) {}
        }

        if (timeout == 0) {
            return false;
        }

        OldTimeStamp = TimeStamp = DWT->CYCCNT;
    }

    return true;
}

inline bool SIGetConsoleTerminator(void)
{
    volatile uint32_t TimeStamp;
    const uint32_t TransferTime = 500; // 540 * 1000 * 1000 / 1000 * 1000 = 1us.
    // Skip the terminator for now.
    TimeStamp = OldTimeStamp = DWT->CYCCNT;
    while ((Running != false) && ((TimeStamp - OldTimeStamp) < (TransferTime * 2))) {
        TimeStamp = DWT->CYCCNT;
    }

    return true;
}

inline bool SIPutDeviceTerminator(void)
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

inline bool SIPutByte(BYTE In)
{
    const BYTE SIZero = 0x8;
    const BYTE SIOne = 0xE;
    volatile uint32_t TimeStamp;
    
    BYTE Out = 0;

    const uint32_t TransferTime = 553; // 540 * 1000 * 1000 / 1000 * 1000 = 1us.
    uint BitCount = 0;
    //OldTimeStamp = DWT->CYCCNT;
    while ((Running != false) && (BitCount <= 8)) {
        if (Out == 0) {
            Out = ((In & 0x80) != 0) ? SIOne : SIZero;
            In <<= 1;
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
#else
inline BYTE GetSingleByte(uint32_t Offset)
{
    BYTE Result = 0;
    uint32_t Index = 0;
    while (Index < 16) {
        Result <<= 1;
        Result |= (SDataBuffer[(Offset % SI_RINGBUFFER_LENGTH) + Index + 1] - SDataBuffer[(Offset % SI_RINGBUFFER_LENGTH) + Index]) < 4;
        Index += 2;
    }

    return Result;
}
inline bool SIGetBytes(BYTE *Out, uint32_t ExpectedBytes, bool Block)
{
    volatile uint32_t TransferCount = LastTransferCount;
    volatile BYTE ByteCount = 0;
    *Out = 0;

    // Wait for input capture to be enabled.
    while ((Running != false) && (((((DMA_Stream_TypeDef *)(hdma_tim3_ch4.Instance))->CR) & 1) == 0)) {}

    // Read the current capture pointer.
    //TransferCount = ((DMA_Stream_TypeDef*)0x40020088)->NDTR;
    while ((Running != false) && (ByteCount < ExpectedBytes)) {
        // Wait for data in buffer, there need to be 16 captures before a single byte of data is available.
        while ((Block != false) && ((LastTransferCount - TransferCount) < 16) && (Running != false)) {
            //TransferCount = ((DMA_Stream_TypeDef*) 0x40020088)->NDTR;
        }

        if (Running == false) {
            return false;
        }

        // Ignore the ARR ticks.
        if (SDataBuffer[SI_RINGBUFFER_LENGTH - LastTransferCount] == SDataBuffer[SI_RINGBUFFER_LENGTH - TransferCount]) {
            LastTransferCount = TransferCount;
            continue;
        }

        *Out = GetSingleByte(SI_RINGBUFFER_LENGTH - LastTransferCount);
        LastTransferCount -= 16;
        if (LastTransferCount > SI_RINGBUFFER_LENGTH || LastTransferCount == 0) {
            LastTransferCount += SI_RINGBUFFER_LENGTH;
        }

#ifdef LOG_EEPROM_BYTES
        EepromInputLog[EepLogIdx++ % (1024 * 1024)] = *Out;
#endif
        ByteCount += 1;
        if (ByteCount < ExpectedBytes) {
            Out += 1;
            *Out = 0;
        }
    }

    return Running;
}

inline bool SIGetConsoleTerminator(void)
{
    // Wait for data in buffer, 2 edges need to come in before the transfer is considered complete.
    uint32_t TransferCount = ((DMA_Stream_TypeDef*)0x40020088)->NDTR;
    while ((TransferCount - LastTransferCount) < 2) {
        TransferCount = ((DMA_Stream_TypeDef*)0x40020088)->NDTR;
    }

    LastTransferCount -= 2;
    if (LastTransferCount > SI_RINGBUFFER_LENGTH) {
        LastTransferCount += SI_RINGBUFFER_LENGTH;
    }
    return true;
}

inline bool SIPutDeviceTerminator(void)
{
    // Add the device terminator
    const uint32_t HALF_PULSE = 4;
    Start += HALF_PULSE;
    SI_DMAOutputBuffer[SI_DMAOutputLength] = Start;
    Start += HALF_PULSE;
    SI_DMAOutputLength += 1;

    // Stop input mode.
    htim3.Instance->CR1 = 0;
    // Reset input capture
    __HAL_TIM_DISABLE_DMA(&htim3, TIM_DMA_CC4);
    (((DMA_Stream_TypeDef *)(hdma_tim3_ch4.Instance))->CR) &= ~DMA_SxCR_EN;
    ((DMA_Base_Registers *)(hdma_tim3_ch4.StreamBaseAddress))->IFCR = 0x3FUL << (hdma_tim3_ch4.StreamIndex & 0x1FU);
    ((DMA_Stream_TypeDef*)hdma_tim3_ch4.Instance)->NDTR = SI_RINGBUFFER_LENGTH;
    LastTransferCount = SI_RINGBUFFER_LENGTH;

    // Program the TIM3 timer into output compare mode and setup DMA
    //     SI_DMAOutputBuffer, SI_DMAOutputLength
    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = TIM_OCMODE_TOGGLE;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_LOW;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_SET;
    sConfigOC.OCFastMode = TIM_OCFAST_ENABLE;
    sConfigOC.Pulse = 4;
    // TODO: Convert the configuration to a few instructions.
    if (HAL_TIM_OC_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
    {
        Error_Handler();
    }
    //SCB_CleanInvalidateDCache_by_Addr((uint32_t*)&(SI_DMAOutputBuffer[0]), SI_DMAOutputLength * sizeof(uint16_t));

    // Program the output dma to start.
    ((DMA_Base_Registers *)(hdma_tim3_ch4_out.StreamBaseAddress))->IFCR = 0x3FUL << (hdma_tim3_ch4_out.StreamIndex & 0x1FU);
    (((DMA_Stream_TypeDef *)(hdma_tim3_ch4_out.Instance))->NDTR) = SI_DMAOutputLength;
    (((DMA_Stream_TypeDef *)(hdma_tim3_ch4_out.Instance))->CR) |= DMA_SxCR_EN;
    __HAL_TIM_ENABLE_DMA(&htim3, TIM_DMA_CC4);
    htim3.Instance->CCER |= (uint32_t)(TIM_CCx_ENABLE << (TIM_CHANNEL_4 & 0x1FU));

    // Program channel 3 DMA to trigger transmit completion.
    htim3.Instance->CCR3 = Start;
    ((DMA_Base_Registers *)(hdma_tim3_input_capture_configure.StreamBaseAddress))->IFCR = 0x3FUL << (hdma_tim3_input_capture_configure.StreamIndex & 0x1FU);
    (((DMA_Stream_TypeDef *)(hdma_tim3_input_capture_configure.Instance))->NDTR) = 1;
    (((DMA_Stream_TypeDef *)(hdma_tim3_input_capture_configure.Instance))->CR) |= DMA_SxCR_EN | DMA_SxCR_TCIE;
    htim3.Instance->DCR = TIM_DMABURSTLENGTH_1TRANSFER | TIM_DMABASE_CR1;
    __HAL_TIM_ENABLE_DMA(&htim3, TIM_DMA_CC3);

    // Enable the output timer.
    htim3.Instance->CNT = 0;
    htim3.Instance->CCMR2 = 0x3000;
    htim3.Instance->CR1 = 1;

    // Reset output recording.
    SI_DMAOutputLength = 0;
    return true;
}

inline bool SIPutByte(BYTE In)
{
    // Construct the DMA output buffer
    // SI_DMAOutputBuffer, SI_DMAOutputLength;
    if (SI_DMAOutputLength == 0) {
        Start = 4;
    }

    for (uint32_t i = 0; i < 8; i += 1) {
        const uint32_t SHORT_PULSE = 2;
        const uint32_t LONG_PULSE = 6;
        if ((In & 0x80) != 0) {
            Start += SHORT_PULSE;
            SI_DMAOutputBuffer[SI_DMAOutputLength] = Start;
            Start += LONG_PULSE;

        } else {
            Start += LONG_PULSE;
            SI_DMAOutputBuffer[SI_DMAOutputLength] = Start;
            Start += SHORT_PULSE;
        }

        SI_DMAOutputLength += 1;
        SI_DMAOutputBuffer[SI_DMAOutputLength] = Start;
        SI_DMAOutputLength += 1;

        In <<= 1;
    }
    return true;
}
#endif

void ITCM_FUNCTION RunEEPROMEmulator(void)
{
    BYTE Command = 0;

    // Check state.
    bool result = SIGetBytes(&Command, 1, true);
    if ((Running == false) || (result == false)) {
        return;
    }

    if ((Command == SI_RESET) || (Command == SI_INFO)) {
        SIGetConsoleTerminator();
        // Return the info on either 4KiBit or 16KiBit EEPROM.
        // 0x0080	N64	4 Kbit EEPROM	Bitfield: 0x80=Write in progress
        // 0x00C0	N64	16 Kbit EEPROM	Bitfield: 0x80=Write in progress
#if (SI_USE_DMA == 0)
        OldTimeStamp = DWT->CYCCNT;
#endif
        SIPutByte(0x00);
        SIPutByte(EEPROMType);
        SIPutByte(0x00);
        SIPutDeviceTerminator();
    }

    if (Command == EEPROM_READ) {
        BYTE AddressByte;
        SIGetBytes(&AddressByte, 1, true);
        SIGetConsoleTerminator();
        uint32_t Address = AddressByte * 8;
#if (SI_USE_DMA == 0)
        OldTimeStamp = DWT->CYCCNT;
#endif
        for (int i = 0; i < 8; i += 1) {
            SIPutByte(EEPROMStore[Address + i]);
        }

        SIPutDeviceTerminator();
    }

    if (Command == EEPROM_STORE) {
        BYTE AddressByte;
        SIGetBytes(&AddressByte, 1, true);
        if (EEPROMType == 0x80) {
            AddressByte %= 64;
        }

        uint32_t Address = AddressByte * 8;
        SIGetBytes(&(EEPROMStore[Address]), 8, true);
        SIGetConsoleTerminator();

#if (SI_USE_DMA == 0)
        OldTimeStamp = DWT->CYCCNT;
#endif
        SIPutByte(0x00); // Output not busy, coz we fast.
        SIPutDeviceTerminator();
    }
}

void SITest()
{
    InitializeTimersSI();
    while (1) {
        SIPutByte(0x00);
        SIPutByte(0xFF);
        SIPutByte(0x00);
        SIPutDeviceTerminator();

        // wait on completion of DMA
        while (((DMA_Stream_TypeDef*)hdma_tim3_input_capture_configure.Instance)->NDTR) {}

        SIPutByte(0xAB);
        SIPutByte(0xFF);
        SIPutByte(0x00);
        SIPutDeviceTerminator();

        // wait on completion of DMA
        while (((DMA_Stream_TypeDef*)hdma_tim3_input_capture_configure.Instance)->NDTR) {}
    }
}