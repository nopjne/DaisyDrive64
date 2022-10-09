#include "daisy_seed.h"
#include "stm32h7xx_hal.h"
#include "sys/system.h"
#include "stm32h7xx_hal_dma.h"
#include "stm32h7xx_ll_tim.h"
#include "stm32h7xx_ll_bus.h"
#include "n64common.h"
#include "daisydrive64.h"

DTCM_DATA DMA_HandleTypeDef DMA_Handle_Channel0;
DTCM_DATA DMA_HandleTypeDef DMA_Handle_Channel1;
DTCM_DATA HRTIM_HandleTypeDef hhrtim;
DTCM_DATA DMA_HandleTypeDef hdma_hrtim1_m;
DTCM_DATA DMA_HandleTypeDef hdma_dma_generator0;
DTCM_DATA DMA_HandleTypeDef hdma_dma_generator1;

LPTIM_HandleTypeDef LptimHandle = {0};

BYTE *const Sram4Buffer = (BYTE*)0x38000000;
uint32_t *const LogBuffer = (uint32_t*)(ram + (48 * 1024 * 1024));
uint32_t *const PortABuffer = (uint32_t*)Sram4Buffer;
uint32_t *const PortBBuffer = (uint32_t*)(Sram4Buffer + 16);

// TODO: Define storage for 1Mb FRAM (131,072 bytes)


// The DMA out buffer should have space for 512 byte.
// Currently the BBuffer needs to be filled in by the CM7. Rewiring PortA and B would allow it to be entirely DMA.
// Latency because of the cache invalidate, needs to be taken into consideration too.
uint32_t *const DMAOutABuffer = (uint32_t*)(Sram4Buffer + 32);
uint32_t *const DMAOutBBuffer = (uint32_t*)(Sram4Buffer + 32 + 8);


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
    //return 0;

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

    // TODO: Setup the data READ DMA.
    //  The end of the ALE capture should trigger the start of RAM to SRAM copy for a set of 512byte.
    //      Distributed in the following way:
    //          PortA [0] 0x00FF | 0x000000FF > PortA[0]
    //          PortB [0] 0x00F0 | 0x0000FF00 > PortB[0] (upper 4 bit kept)
    //          PortB [1] 0xC300 | 0x0000FF00 > PortB[1] (0xC3 bit kept)
    //  The DMA to SRAM currently happens on the main core, as it requires shifting up values.
    //    A rewire may help here by wiring the 0xF0 bits of Port B to bit [12 .. 15]
    //    A rewire may help here by wiring the 0xF0 and wiring            [12 .. 15]
    //  EXTI0 triggers BDMA SRAM to GPIO for 2 byte on every Hi -> Lo transition.
    //  A decrementing timer is kicked on every Hi->Lo transition which coinsides with RD Pulse Width.
    //    Potentially attempt to use OD instead of PP.
    //  

    return 0;
}


void HAL_HRTIM_MspInit(HRTIM_HandleTypeDef* hhrtim)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    HAL_DMA_MuxSyncConfigTypeDef pSyncConfig;
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
    if (hhrtim->Instance == HRTIM1)
    {
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_HRTIM1;
        PeriphClkInitStruct.Hrtim1ClockSelection = RCC_HRTIM1CLK_TIMCLK;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
        {
            Error_Handler();
        }

        /* Peripheral clock enable */
        __HAL_RCC_HRTIM1_CLK_ENABLE();

        __HAL_RCC_GPIOG_CLK_ENABLE();
        /**HRTIM GPIO Configuration
        PG11     ------> HRTIM_EEV4
        */
        GPIO_InitStruct.Pin = GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF2_HRTIM1;
        HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

        /* HRTIM1 DMA Init */
        /* HRTIM1_M Init */
        hdma_hrtim1_m.Instance = DMA1_Stream0;
        hdma_hrtim1_m.Init.Request = DMA_REQUEST_HRTIM_MASTER;
        hdma_hrtim1_m.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hdma_hrtim1_m.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_hrtim1_m.Init.MemInc = DMA_MINC_ENABLE;
        hdma_hrtim1_m.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_hrtim1_m.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_hrtim1_m.Init.Mode = DMA_CIRCULAR;
        hdma_hrtim1_m.Init.Priority = DMA_PRIORITY_HIGH;
        hdma_hrtim1_m.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
        hdma_hrtim1_m.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_1QUARTERFULL;
        if (HAL_DMA_Init(&hdma_hrtim1_m) != HAL_OK)
        {
            Error_Handler();
        }

        pSyncConfig.SyncSignalID = HAL_DMAMUX1_SYNC_EXTI0;
        pSyncConfig.SyncPolarity = HAL_DMAMUX_SYNC_NO_EVENT;
        pSyncConfig.SyncEnable = DISABLE;
        pSyncConfig.EventEnable = ENABLE;
        pSyncConfig.RequestNumber = 1;
        if (HAL_DMAEx_ConfigMuxSync(&hdma_hrtim1_m, &pSyncConfig) != HAL_OK)
        {
            Error_Handler();
        }

        __HAL_LINKDMA(hhrtim, hdmaMaster, hdma_hrtim1_m);
    }
}

static void MX_HRTIM_Init(void)
{
    HRTIM_EventCfgTypeDef pEventCfg = {0};
    HRTIM_TimeBaseCfgTypeDef pTimeBaseCfg = {0};
    HRTIM_TimerCfgTypeDef pTimerCfg = {0};

    /* USER CODE BEGIN HRTIM_Init 1 */

    /* USER CODE END HRTIM_Init 1 */
    hhrtim.Instance = HRTIM1;
    hhrtim.Init.HRTIMInterruptResquests = HRTIM_IT_NONE;
    hhrtim.Init.SyncOptions = HRTIM_SYNCOPTION_NONE;
    if (HAL_HRTIM_Init(&hhrtim) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_HRTIM_EventPrescalerConfig(&hhrtim, HRTIM_EVENTPRESCALER_DIV1) != HAL_OK)
    {
        Error_Handler();
    }

    pEventCfg.Source = HRTIM_EVENTSRC_1;
    pEventCfg.Polarity = HRTIM_EVENTPOLARITY_HIGH;
    pEventCfg.Sensitivity = HRTIM_EVENTSENSITIVITY_BOTHEDGES;
    pEventCfg.FastMode = HRTIM_EVENTFASTMODE_ENABLE;
    if (HAL_HRTIM_EventConfig(&hhrtim, HRTIM_EVENT_4, &pEventCfg) != HAL_OK)
    {
        Error_Handler();
    }

    pTimeBaseCfg.Period = 0x0003;
    pTimeBaseCfg.RepetitionCounter = 0x00;
    pTimeBaseCfg.PrescalerRatio = HRTIM_PRESCALERRATIO_DIV1;
    pTimeBaseCfg.Mode = HRTIM_MODE_SINGLESHOT_RETRIGGERABLE;
    if (HAL_HRTIM_TimeBaseConfig(&hhrtim, HRTIM_TIMERINDEX_MASTER, &pTimeBaseCfg) != HAL_OK)
    {
        Error_Handler();
    }

    pTimerCfg.InterruptRequests = HRTIM_MASTER_IT_NONE;
    pTimerCfg.DMARequests = HRTIM_MASTER_DMA_MREP;
    pTimerCfg.DMASrcAddress = (uint32_t)&(GPIOB->IDR);
    pTimerCfg.DMADstAddress = (uint32_t)(PortBBuffer);
    pTimerCfg.DMASize = 0x4;
    pTimerCfg.HalfModeEnable = HRTIM_HALFMODE_DISABLED;
    pTimerCfg.StartOnSync = HRTIM_SYNCSTART_DISABLED;
    pTimerCfg.ResetOnSync = HRTIM_SYNCRESET_DISABLED;
    pTimerCfg.DACSynchro = HRTIM_DACSYNC_NONE;
    pTimerCfg.PreloadEnable = HRTIM_PRELOAD_DISABLED;
    pTimerCfg.UpdateGating = HRTIM_UPDATEGATING_INDEPENDENT;
    pTimerCfg.BurstMode = HRTIM_TIMERBURSTMODE_MAINTAINCLOCK;
    pTimerCfg.RepetitionUpdate = HRTIM_UPDATEONREPETITION_DISABLED;
    if (HAL_HRTIM_WaveformTimerConfig(&hhrtim, HRTIM_TIMERINDEX_MASTER, &pTimerCfg) != HAL_OK)
    {
        Error_Handler();
    }

    // New code.. from HRTIM external event example.
    HRTIM_OutputCfgTypeDef sConfig_output_config;
    sConfig_output_config.Polarity = HRTIM_OUTPUTPOLARITY_HIGH;
    sConfig_output_config.SetSource = HRTIM_OUTPUTSET_EEV_4;
    sConfig_output_config.ResetSource = HRTIM_OUTPUTRESET_EEV_4;
    sConfig_output_config.IdleMode = HRTIM_OUTPUTIDLEMODE_NONE;
    sConfig_output_config.IdleLevel = HRTIM_OUTPUTIDLELEVEL_INACTIVE;
    sConfig_output_config.FaultLevel = HRTIM_OUTPUTFAULTLEVEL_NONE;
    sConfig_output_config.ChopperModeEnable = HRTIM_OUTPUTCHOPPERMODE_DISABLED;
    sConfig_output_config.BurstModeEntryDelayed = HRTIM_OUTPUTBURSTMODEENTRY_REGULAR;

    HAL_HRTIM_WaveformOutputConfig(&hhrtim, HRTIM_TIMERINDEX_TIMER_A, HRTIM_OUTPUT_TA1, &sConfig_output_config);

    HAL_HRTIM_WaveformOutputConfig(&hhrtim, HRTIM_TIMERINDEX_TIMER_A, HRTIM_OUTPUT_TA2, &sConfig_output_config);

    /*##-3- External Event configuration ########################################################*/
    HRTIM_EventCfgTypeDef sConfig_event;
    sConfig_event.Source = HRTIM_EVENTSRC_1;
    sConfig_event.Polarity = HRTIM_EVENTPOLARITY_HIGH;
    sConfig_event.Sensitivity = HRTIM_EVENTSENSITIVITY_BOTHEDGES;
    sConfig_event.Filter = HRTIM_EVENTFILTER_NONE;
    sConfig_event.FastMode = HRTIM_EVENTFASTMODE_DISABLE;
                
    HAL_HRTIM_EventConfig(&hhrtim, HRTIM_EVENT_8, &sConfig_event);
}

static void MX_DMA_Init(void)
{

    /* Local variables */
    HAL_DMA_MuxRequestGeneratorConfigTypeDef pRequestGeneratorConfig = {0};

    /* DMA controller clock enable */
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* Configure DMA request hdma_dma_generator0 on DMA1_Stream1 */
    hdma_dma_generator0.Instance = DMA1_Stream1;
    hdma_dma_generator0.Init.Request = DMA_REQUEST_GENERATOR0;
    hdma_dma_generator0.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_dma_generator0.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_dma_generator0.Init.MemInc = DMA_MINC_ENABLE;
    hdma_dma_generator0.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_dma_generator0.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_dma_generator0.Init.Mode = DMA_CIRCULAR;
    hdma_dma_generator0.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_dma_generator0.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    hdma_dma_generator0.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_1QUARTERFULL;
    if (HAL_DMA_Init(&hdma_dma_generator0) != HAL_OK)
    {
        Error_Handler( );
    }

    /* Configure the DMAMUX request generator for the selected DMA stream */
    pRequestGeneratorConfig.SignalID = HAL_DMAMUX1_REQ_GEN_DMAMUX1_CH0_EVT;
    pRequestGeneratorConfig.Polarity = HAL_DMAMUX_REQ_GEN_RISING;
    pRequestGeneratorConfig.RequestNumber = 1;
    if (HAL_DMAEx_ConfigMuxRequestGenerator(&hdma_dma_generator0, &pRequestGeneratorConfig) != HAL_OK)
    {
        Error_Handler( );
    }

    /* Configure DMA request hdma_dma_generator1 on DMA1_Stream2 */
    hdma_dma_generator1.Instance = DMA1_Stream2;
    hdma_dma_generator1.Init.Request = DMA_REQUEST_GENERATOR1;
    hdma_dma_generator1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_dma_generator1.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_dma_generator1.Init.MemInc = DMA_MINC_ENABLE;
    hdma_dma_generator1.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_dma_generator1.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_dma_generator1.Init.Mode = DMA_CIRCULAR;
    hdma_dma_generator1.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_dma_generator1.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    hdma_dma_generator1.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_1QUARTERFULL;
    if (HAL_DMA_Init(&hdma_dma_generator1) != HAL_OK)
    {
        Error_Handler( );
    }

    /* Configure the DMAMUX request generator for the selected DMA stream */
    pRequestGeneratorConfig.SignalID = HAL_DMAMUX1_REQ_GEN_DMAMUX1_CH0_EVT;
    pRequestGeneratorConfig.Polarity = HAL_DMAMUX_REQ_GEN_RISING;
    pRequestGeneratorConfig.RequestNumber = 1;
    if (HAL_DMAEx_ConfigMuxRequestGenerator(&hdma_dma_generator1, &pRequestGeneratorConfig) != HAL_OK)
    {
        Error_Handler( );
    }

    /* DMA interrupt init */
    /* DMA1_Stream0_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);

    /* DMAMUX1_OVR_IRQn interrupt configuration */
    //HAL_NVIC_SetPriority(DMAMUX1_OVR_IRQn, 0, 0);
    //HAL_NVIC_EnableIRQ(DMAMUX1_OVR_IRQn);

    HAL_StatusTypeDef dmares = HAL_OK;
#if 0
    // Select Callbacks functions called after Transfer complete and Transfer error
    dmares = HAL_DMA_RegisterCallback(&hdma_dma_generator1, HAL_DMA_XFER_CPLT_CB_ID, NULL);
    if (dmares != HAL_OK) {
        Error_Handler();
    }

    dmares = HAL_DMA_RegisterCallback(&hdma_dma_generator1, HAL_DMA_XFER_ERROR_CB_ID, HAL_TransferError);
    if (dmares != HAL_OK) {
        Error_Handler();
    }
#endif

    dmares = HAL_DMAEx_EnableMuxRequestGenerator(&hdma_dma_generator1);
    if (dmares != HAL_OK) {
        Error_Handler();
    }

    dmares = HAL_DMAEx_EnableMuxRequestGenerator(&hdma_dma_generator0);
    if (dmares != HAL_OK) {
        Error_Handler();
    }

    dmares = HAL_DMA_Start_IT(&hdma_dma_generator1, (uint32_t)&(GPIOB->IDR), (uint32_t)(PortBBuffer), 2);
    if (dmares != HAL_OK) {
        Error_Handler();
    }

    dmares = HAL_DMA_Start_IT(&hdma_dma_generator0, (uint32_t)&(GPIOA->IDR), (uint32_t)(PortABuffer), 2);
    if (dmares != HAL_OK) {
        Error_Handler();
    }
}

void InitializeTimersPI(void)
{
    //MX_HRTIM_Init();
    //MX_DMA_Init();
}

extern "C"
ITCM_FUNCTION
void EXTI15_10_IRQHandler(void)
{
    if ((EXTI->PR1 & RESET_LINE) != 0) {
        // Unset interrupt.
        EXTI->PR1 = RESET_LINE;
#if (USE_OPEN_DRAIN_OUTPUT == 0)
        // Switch to output.
        SET_PI_OUTPUT_MODE
        GPIOA->ODR = 0;
        GPIOB->ODR = 0;
        // Switch to Input.
        SET_PI_INPUT_MODE
#else
        // Switch to Input.
        GPIOA->ODR = 0xFF;
        GPIOB->ODR = 0xC3F0;
#endif

        DMACount = 0;
        IntCount = 0;
        ALE_H_Count = 0;
        ADInputAddress = 0;
        Running = false;

        // If a whole DaisyDrive64 system reset is necessary call: HAL_NVIC_SystemReset();
        return;
    }

    if ((EXTI->PR1 & ALE_H) == 0) {
        return;
    }

    EXTI->PR1 = ALE_H;
#if (USE_OPEN_DRAIN_OUTPUT == 0)
    // Switch to Input.
    SET_PI_INPUT_MODE;
#else
    // Switch to Input.
    GPIOA->ODR = 0xFF;
    GPIOB->ODR = 0xC3F0;
#endif
    
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
#if (USE_OPEN_DRAIN_OUTPUT == 0)
        // Switch to output.
        // TODO: Can this be done through DMA entirely?
        SET_PI_OUTPUT_MODE
#endif
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
        const uint32_t x = 0;
#endif
#else
        volatile uint32_t x = 196;
#endif

        while (x) { x--; }
#if (USE_OPEN_DRAIN_OUTPUT == 0)
        // GPIOA->ODR = 0xFF;
        // GPIOB->ODR = 0xFFFF;
        GPIOA->ODR = 0x00;
        GPIOB->ODR = 0x0000;
        SET_PI_INPUT_MODE;
#else
        // Switch to Input.
        GPIOA->ODR = 0xFF;
        GPIOB->ODR = 0xC3F0;
#endif
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

inline ITCM_FUNCTION void ConstructAddress(void)
{
    SCB_InvalidateDCache_by_Addr(PortABuffer, 16);
    if ((PortBBuffer[0] & ALE_H) == 0) {
        // Construct ADInputAddress
        ADInputAddress = (PortABuffer[1] & 0xFE) | ((PortBBuffer[1] & 0x03F0) << 4) | (PortBBuffer[1] & 0xC000) |
                         (ADInputAddress & 0xFFFF0000);
    } else {
        ADInputAddress = (PortABuffer[1] & 0xFE) | ((PortBBuffer[1] & 0x03F0) << 4) | (PortBBuffer[1] & 0xC000) |
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