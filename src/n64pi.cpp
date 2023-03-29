#include "daisy_seed.h"
#include "stm32h7xx_hal.h"
#include "sys/system.h"
#include "stm32h7xx_hal_dma.h"
#include "stm32h7xx_ll_tim.h"
#include "stm32h7xx_ll_bus.h"
#include "n64common.h"
#include "daisydrive64.h"
#include "flashram.h"
#include "menu.h"
#include "bootrom.h"

extern uint32_t CurrentRomSaveType;

//#define HANDLE_ADDRESS_CONSTRUCTION_IN_ISR
DTCM_DATA DMA_HandleTypeDef DMA_Handle_Channel0; // AD capture.
DTCM_DATA DMA_HandleTypeDef DMA_Handle_Channel1; // AD capture.
DTCM_DATA DMA_HandleTypeDef DMA_Handle_Channel2; // MODER switching.
DTCM_DATA DMA_HandleTypeDef DMA_Handle_Channel3; // MODER switching.
DTCM_DATA HRTIM_HandleTypeDef hhrtim;
DTCM_DATA DMA_HandleTypeDef hdma_hrtim1_m;
DTCM_DATA DMA_HandleTypeDef hdma_dma_generator0;
DTCM_DATA DMA_HandleTypeDef hdma_dma_generator1;
//DTCM_DATA uint32_t TestMemory[8];

LPTIM_HandleTypeDef LptimHandle = {0};
DTCM_DATA uint32_t OverflowCounter = 0;
DTCM_DATA uint32_t ADInputAddress = 0;
BYTE *const Sram4Buffer = (BYTE*)0x38000000;
uint32_t *const LogBuffer = (uint32_t*)(ram + (48 * 1024 * 1024));
uint32_t *const PortABuffer = (uint32_t*)Sram4Buffer;
uint32_t *const PortBBuffer = (uint32_t*)(Sram4Buffer + 16);
//uint32_t *const PortABuffer = (uint32_t*)TestMemory;
//uint32_t *const PortBBuffer = (uint32_t*)(TestMemory + 4);
BYTE NullMem[512] = {0};

// TODO: Define storage for 1Mb FRAM (131,072 bytes)


// The DMA out buffer should have space for 512 byte.
// Currently the BBuffer needs to be filled in by the CM7. Rewiring PortA and B would allow it to be entirely DMA.
// Latency because of the cache invalidate, needs to be taken into consideration too.
uint32_t *const DMAOutABuffer = (uint32_t*)(Sram4Buffer + 32);
uint32_t *const DMAOutBBuffer = (uint32_t*)(Sram4Buffer + 32 + 8);
uint32_t *const MODERDMA = (uint32_t*)(DMAOutABuffer + (sizeof(uint32_t) * 512));

// Storage for Flash ram.
BYTE FlashRamStorage[FLASHRAM_SIZE] __attribute__((aligned(16)));

#define PI_PRECALCULATE_OUT_VALUE 1
#if (PI_PRECALCULATE_OUT_VALUE != 0)
DTCM_DATA uint16_t ValueA = 0;
DTCM_DATA uint16_t ValueB = 0;
#else
DTCM_DATA uint16_t PrefetchRead = 0;
#endif
DTCM_DATA uint16_t* ReadPtr = 0;
DTCM_DATA uint32_t ReadOffset = 0;
DTCM_DATA uint32_t DMACount = 0;
DTCM_DATA volatile uint32_t ALE_H_Count = 0;
DTCM_DATA volatile uint32_t IntCount = 0;
DTCM_DATA volatile bool Running = false;
DTCM_DATA uint32_t SpeedTracking[5];
DTCM_DATA uint32_t m_SpeedTracking[5];

static void HAL_TransferError(DMA_HandleTypeDef *hdma);
static void Error_Handler(void);
extern "C" ITCM_FUNCTION void EXTI4_IRQHandler(void);

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
    memset(NullMem, 0, sizeof(NullMem));
    HAL_DMA_MuxRequestGeneratorConfigTypeDef dmamux_ReqGenParams  = {0};
    __HAL_RCC_BDMA_CLK_ENABLE();

    // Setup the MODE switching DMA that should execute before the other channels start.
    {
        DMA_Handle_Channel2.Instance                 = BDMA_Channel2;
        DMA_Handle_Channel2.Init.Request             = BDMA_REQUEST_GENERATOR0;
        DMA_Handle_Channel2.Init.Direction           = DMA_MEMORY_TO_PERIPH;
        DMA_Handle_Channel2.Init.PeriphInc           = DMA_PINC_DISABLE;
        DMA_Handle_Channel2.Init.MemInc              = DMA_MINC_DISABLE;
        DMA_Handle_Channel2.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
        DMA_Handle_Channel2.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
        DMA_Handle_Channel2.Init.Mode                = DMA_CIRCULAR;
        DMA_Handle_Channel2.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
        DMA_Handle_Channel2.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
        DMA_Handle_Channel2.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_1QUARTERFULL;
        DMA_Handle_Channel2.Init.MemBurst            = DMA_MBURST_SINGLE;
        DMA_Handle_Channel2.Init.PeriphBurst         = DMA_PBURST_SINGLE;

        // Initialize the DMA for Transmission.
        HAL_StatusTypeDef dmares = HAL_OK;
        dmares = HAL_DMA_Init(&DMA_Handle_Channel2);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        // Select Callbacks functions called after Transfer complete and Transfer error.
        dmares = HAL_DMA_RegisterCallback(&DMA_Handle_Channel2, HAL_DMA_XFER_CPLT_CB_ID, NULL);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        dmares = HAL_DMA_RegisterCallback(&DMA_Handle_Channel2, HAL_DMA_XFER_ERROR_CB_ID, HAL_TransferError);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        // NVIC configuration for DMA transfer complete interrupt.
        HAL_NVIC_SetPriority(BDMA_Channel2_IRQn, 1, 0);
        HAL_NVIC_DisableIRQ(BDMA_Channel2_IRQn);

        // Configure and enable the DMAMUX Request generator.
        dmamux_ReqGenParams.SignalID  = HAL_DMAMUX2_REQ_GEN_EXTI0;
        dmamux_ReqGenParams.Polarity  = HAL_DMAMUX_REQ_GEN_RISING_FALLING;
        dmamux_ReqGenParams.RequestNumber = 1;

        dmares = HAL_DMAEx_ConfigMuxRequestGenerator(&DMA_Handle_Channel2, &dmamux_ReqGenParams);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        HAL_DMA_MuxSyncConfigTypeDef Sync;
        Sync.SyncSignalID = HAL_DMAMUX2_SYNC_EXTI0;
        Sync.SyncPolarity = HAL_DMAMUX_SYNC_NO_EVENT;
        Sync.SyncEnable = FunctionalState::DISABLE;
        Sync.EventEnable = FunctionalState::ENABLE;
        Sync.RequestNumber = 1;
        dmares = HAL_DMAEx_ConfigMuxSync(&DMA_Handle_Channel2, &Sync);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        // NVIC configuration for DMAMUX request generator overrun errors
        HAL_NVIC_SetPriority(DMAMUX2_OVR_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(DMAMUX2_OVR_IRQn);

        dmares = HAL_DMAEx_EnableMuxRequestGenerator(&DMA_Handle_Channel2);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        dmares = HAL_DMA_Start_IT(&DMA_Handle_Channel2, (uint32_t)&(MODERDMA[1]), (uint32_t)&(GPIOB->MODER), 1);
        if (dmares != HAL_OK) {
            Error_Handler();
        }
    }
    // Setup the MODE switching DMA that should execute before the other channels start.
    {
        DMA_Handle_Channel3.Instance                 = BDMA_Channel3;
        DMA_Handle_Channel3.Init.Request             = BDMA_REQUEST_GENERATOR0;
        DMA_Handle_Channel3.Init.Direction           = DMA_MEMORY_TO_PERIPH;
        DMA_Handle_Channel3.Init.PeriphInc           = DMA_PINC_DISABLE;
        DMA_Handle_Channel3.Init.MemInc              = DMA_MINC_DISABLE;
        DMA_Handle_Channel3.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
        DMA_Handle_Channel3.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
        DMA_Handle_Channel3.Init.Mode                = DMA_CIRCULAR;
        DMA_Handle_Channel3.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
        DMA_Handle_Channel3.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
        DMA_Handle_Channel3.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_1QUARTERFULL;
        DMA_Handle_Channel3.Init.MemBurst            = DMA_MBURST_SINGLE;
        DMA_Handle_Channel3.Init.PeriphBurst         = DMA_PBURST_SINGLE;

        // Initialize the DMA for Transmission.
        HAL_StatusTypeDef dmares = HAL_OK;
        dmares = HAL_DMA_Init(&DMA_Handle_Channel3);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        // Select Callbacks functions called after Transfer complete and Transfer error.
        dmares = HAL_DMA_RegisterCallback(&DMA_Handle_Channel3, HAL_DMA_XFER_CPLT_CB_ID, NULL);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        dmares = HAL_DMA_RegisterCallback(&DMA_Handle_Channel3, HAL_DMA_XFER_ERROR_CB_ID, HAL_TransferError);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        // NVIC configuration for DMA transfer complete interrupt.
        HAL_NVIC_SetPriority(BDMA_Channel3_IRQn, 1, 0);
        HAL_NVIC_DisableIRQ(BDMA_Channel3_IRQn);

        // Configure and enable the DMAMUX Request generator.
        dmamux_ReqGenParams.SignalID  = HAL_DMAMUX2_REQ_GEN_EXTI0;
        dmamux_ReqGenParams.Polarity  = HAL_DMAMUX_REQ_GEN_RISING_FALLING;
        dmamux_ReqGenParams.RequestNumber = 1;

        dmares = HAL_DMAEx_ConfigMuxRequestGenerator(&DMA_Handle_Channel3, &dmamux_ReqGenParams);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        HAL_DMA_MuxSyncConfigTypeDef Sync;
        Sync.SyncSignalID = HAL_DMAMUX2_SYNC_EXTI0;
        Sync.SyncPolarity = HAL_DMAMUX_SYNC_NO_EVENT;
        Sync.SyncEnable = FunctionalState::DISABLE;
        Sync.EventEnable = FunctionalState::ENABLE;
        Sync.RequestNumber = 1;
        dmares = HAL_DMAEx_ConfigMuxSync(&DMA_Handle_Channel3, &Sync);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        // NVIC configuration for DMAMUX request generator overrun errors
        HAL_NVIC_SetPriority(DMAMUX2_OVR_IRQn, 1, 0);
        HAL_NVIC_EnableIRQ(DMAMUX2_OVR_IRQn);

        dmares = HAL_DMAEx_EnableMuxRequestGenerator(&DMA_Handle_Channel3);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        dmares = HAL_DMA_Start_IT(&DMA_Handle_Channel3, (uint32_t)&(MODERDMA[0]), (uint32_t)&(GPIOA->MODER), 1);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        MODERDMA[0] = 0xABFF0000;
        MODERDMA[1] = 0x0CF000BB;
        SCB_CleanDCache_by_Addr(MODERDMA, 8);
    }

    // Setup the Address capture DMA.
    {
        DMA_Handle_Channel0.Instance                 = BDMA_Channel0;
        DMA_Handle_Channel0.Init.Request             = BDMA_REQUEST_GENERATOR1;
        DMA_Handle_Channel0.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        DMA_Handle_Channel0.Init.PeriphInc           = DMA_PINC_DISABLE;
        DMA_Handle_Channel0.Init.MemInc              = DMA_MINC_ENABLE;
        DMA_Handle_Channel0.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
        DMA_Handle_Channel0.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
        DMA_Handle_Channel0.Init.Mode                = DMA_CIRCULAR;
        DMA_Handle_Channel0.Init.Priority            = DMA_PRIORITY_HIGH;
        DMA_Handle_Channel0.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
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
#ifndef HANDLE_ADDRESS_CONSTRUCTION_IN_ISR
        HAL_NVIC_EnableIRQ(BDMA_Channel0_IRQn);
#endif

        // Configure and enable the DMAMUX Request generator.
        dmamux_ReqGenParams.SignalID  = HAL_DMAMUX2_REQ_GEN_DMAMUX2_CH2_EVT;
        dmamux_ReqGenParams.Polarity  = HAL_DMAMUX_REQ_GEN_RISING;
        dmamux_ReqGenParams.RequestNumber = 1;

        dmares = HAL_DMAEx_ConfigMuxRequestGenerator(&DMA_Handle_Channel0, &dmamux_ReqGenParams);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        // NVIC configuration for DMAMUX request generator overrun errors
        //HAL_NVIC_SetPriority(DMAMUX2_OVR_IRQn, 0, 0);
        //HAL_NVIC_EnableIRQ(DMAMUX2_OVR_IRQn);

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
        DMA_Handle_Channel1.Init.Request             = BDMA_REQUEST_GENERATOR1;
        DMA_Handle_Channel1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        DMA_Handle_Channel1.Init.PeriphInc           = DMA_PINC_DISABLE;
        DMA_Handle_Channel1.Init.MemInc              = DMA_MINC_ENABLE;
        DMA_Handle_Channel1.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        DMA_Handle_Channel1.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
        DMA_Handle_Channel1.Init.Mode                = DMA_CIRCULAR;
        DMA_Handle_Channel1.Init.Priority            = DMA_PRIORITY_HIGH;
        DMA_Handle_Channel1.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
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
#ifndef HANDLE_ADDRESS_CONSTRUCTION_IN_ISR
        //HAL_NVIC_EnableIRQ(BDMA_Channel1_IRQn);
#endif

        // Configure and enable the DMAMUX Request generator.
        dmamux_ReqGenParams.SignalID  = HAL_DMAMUX2_SYNC_DMAMUX2_CH3_EVT;
        dmamux_ReqGenParams.Polarity  = HAL_DMAMUX_REQ_GEN_RISING;
        dmamux_ReqGenParams.RequestNumber = 1;

        dmares = HAL_DMAEx_ConfigMuxRequestGenerator(&DMA_Handle_Channel1, &dmamux_ReqGenParams);
        if (dmares != HAL_OK) {
            Error_Handler();
        }

        // NVIC configuration for DMAMUX request generator overrun errors.
        HAL_NVIC_SetPriority(DMAMUX2_OVR_IRQn, 0, 0);
        HAL_NVIC_DisableIRQ(DMAMUX2_OVR_IRQn);

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

#if 1
    // TODO: Putting this here for now, but should not be in the DMA init function.
    GPIO_InitTypeDef WritePin = {WRITE_LINE, GPIO_MODE_IT_FALLING, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOC, &WritePin);
    NVIC_SetVector(EXTI4_IRQn, (uint32_t)&EXTI4_IRQHandler);
    if (CurrentRomSaveType == SAVE_FLASH_1M) {
        //NVIC_SetVector(EXTI4_IRQn, (uint32_t)&FlashRAMWrite0);
    }

    HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI4_IRQn);
#endif

    return 0;
}

#if 0
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
        GPIO_InitStruct.Speed = GP_SPEED;
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
    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 4, 0);
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
#endif

void InitializeTimersPI(void)
{
    //MX_HRTIM_Init();
    //MX_DMA_Init();
}

//volatile uint32_t xCount = 0;
//volatile uint16_t xValueSave[40];
//volatile uint32_t xAddrSave[40];

extern "C"
ITCM_FUNCTION
void EXTI4_IRQHandler(void)
{
    // SRAM/FRAM handler
    EXTI->PR1 = WRITE_LINE;
    const uint32_t ValueInB = GPIOB->IDR;
    const uint32_t ValueInA = GPIOA->IDR;

    //*((uint32_t*)FlashRamStorage) = 0x80011111;
    *ReadPtr = ((ValueInB & 0x03F0) << 4) | (ValueInB & 0xC000) | (ValueInA & 0xFF);
    //xValueSave[xCount] = ((ValueInB & 0x03F0) << 4) | (ValueInB & 0xC000) | (ValueInA & 0xFF);
    //xAddrSave[xCount] = (uint32_t)ReadPtr;
    ReadPtr += 1;

    if (CURRENT_ROMNAME_STARTS_WITH_OS64) {
        if ((uint32_t)ReadPtr == ((uint32_t)(&(MenuBase[REG_EXECUTE_FUNCTION]))) + 4) {
            MenuBase[REG_STATUS] |= (DAISY_STATUS_BIT_DMA_BUSY | DAISY_STATUS_BIT_SD_BUSY);
            NVIC->STIR = 9;
        }

    } else {
        m_SpeedTracking[0] = SpeedTracking[0];
        m_SpeedTracking[1] = SpeedTracking[1];
        m_SpeedTracking[2] = SpeedTracking[2];
        m_SpeedTracking[3] = SpeedTracking[3];
        m_SpeedTracking[4] = SpeedTracking[4];

        gSaveFence += 1;
        SaveFileDirty = true;
        NVIC->STIR = 9;
    }
}

extern "C"
ITCM_FUNCTION
void EXTI15_10_IRQHandler(void) // Reset interrupt.
{
    EXTI->PR1 = N64_NMI;
    if (RESET_IS_HIGH) {
        gUseBootLoader = true;
        // Hack, this is faster than strcpy.
        *((uint32_t*)CurrentRomName) = *((uint32_t*)"OS64");

    } else {
        Running = false;
    }

    CICProcessRegionSwap();
    // If a whole DaisyDrive64 system reset is necessary call: HAL_NVIC_SystemReset();
    //HAL_NVIC_SystemReset();
}

uint32_t Temp;
extern "C"
ITCM_FUNCTION
void EXTI1_IRQHandler(void)
{
    EXTI->PR1 = READ_LINE;

    // TODO: Value can be DMA-ed directly from ram.
    // PortB upper bits can be used as 0xF0 and port B lower 2 bits can be used as 0x300
    // Then PortA needs to hold the lower-upper 2bits on 10 and 11. Needs 3 DMA channels to realize.
    // And need soldering to USB_OTG_FS_ID and USB_OTG_FS_D_-
#if (PI_PRECALCULATE_OUT_VALUE == 0)
    const uint32_t Value = (((ReadOffset & 2) == 0) ? PrefetchRead : (PrefetchRead >> 16));
    const uint32_t OutB = (((Value >> 4) & 0x03F0) | (Value & 0xC000));
    GPIOA->ODR = Value;
    GPIOB->ODR = OutB;
#else
    // if ((ReadPtr >= FlashRamStorage) && (ReadPtr <= (FlashRamStorage + sizeof(FlashRamStorage)))) {
    //     const uint16_t PrefetchRead = *ReadPtr;
    //     const uint8_t ValueA = PrefetchRead;
    //     const uint16_t ValueB = (((PrefetchRead >> 4) & 0x03F0) | (PrefetchRead & 0xC000));
    //     GPIOA->ODR = ValueA;
    //     GPIOB->ODR = ValueB;
    //     ReadPtr += 1;
    //     return;
    // }

    GPIOA->ODR = ValueA;
    GPIOB->ODR = ValueB;
#endif

#if (USE_OPEN_DRAIN_OUTPUT == 0)
    // Switch to output.
    // TODO: Can this be done through DMA entirely?
    if (ReadOffset == 0) {
        SET_PI_OUTPUT_MODE
        ReadOffset += 2;
    }

#if PI_ENABLE_LOGGING
    if ((ReadOffset & 2) == 0) {
        LogBuffer[IntCount] = (uint32_t)(ADInputAddress + ReadOffset);
        Temp = (ValueA | (ValueB & 0xC000) | ((ValueB << 4) & 0x3F00)) << 16;
    } else {
        LogBuffer[IntCount] = Temp | (*ReadPtr);
    }
    ReadOffset += 2;
#endif

#endif
#if (PI_PRECALCULATE_OUT_VALUE == 0)
    if ((ReadOffset & 3) == 0) {
        ReadPtr += 1;
        PrefetchRead = *ReadPtr;
    }
#else

    ReadPtr += 1;
    __DMB();
    const uint16_t PrefetchRead = *ReadPtr;
    ValueA = PrefetchRead;
    ValueB = (((PrefetchRead >> 4) & 0x03F0) | (PrefetchRead & 0xC000));
#endif

#if PI_ENABLE_LOGGING
    IntCount += 1;  // TODO: Understand why this increment is important for DK booting...
    if (IntCount >= (((64 - 48) * 1024 * 1024) / 4)) {
        IntCount = 3;
    }
#endif

#if 0 // Debugging
    if (IntCount == 565966) {
         LogBuffer[0] = (((BDMA_Base_Registers *)(DMA_Handle_Channel1.StreamBaseAddress))->ISR);
         volatile uint32_t x = *((uint32_t*)0xD1ED1ED1);
         x += 1;
    }
#endif
}

extern "C"
ITCM_FUNCTION
void ReadISRNoPrefetch(void)
{
    EXTI->PR1 = READ_LINE;
    const uint16_t PrefetchRead = *ReadPtr;
    __DMB();
    GPIOA->ODR = PrefetchRead;
    GPIOB->ODR = (((PrefetchRead >> 4) & 0x03F0) | (PrefetchRead & 0xC000));
    ReadPtr += 1;
}

extern "C"
ITCM_FUNCTION
void ReadISRNoPrefetchFirst(void)
{
    EXTI->PR1 = READ_LINE;
    const uint16_t PrefetchRead = *ReadPtr;
    __DMB();
    GPIOA->ODR = PrefetchRead;
    GPIOB->ODR = (((PrefetchRead >> 4) & 0x03F0) | (PrefetchRead & 0xC000));
    SET_PI_OUTPUT_MODE
    ReadPtr += 1;
    NVIC_SetVector(EXTI1_IRQn, (uint32_t)&ReadISRNoPrefetch);
}

#if 0
inline void ConstructAddress(void)
{
    if ((ADInputAddress >= CART_DOM2_ADDR2_START) && (ADInputAddress <= CART_DOM2_ADDR2_END)) {
            //lat=0x05 pwd=0x0c pgs=0xd rls=0x2
            if (ADInputAddress < (CART_DOM2_ADDR2_START | (1 << 18))) {
                ReadPtr = (uint16_t*)(FlashRamStorage + (ADInputAddress - CART_DOM2_ADDR2_START));
            } else if (ADInputAddress >= (CART_DOM2_ADDR2_START | (3 << 18))) {
                ReadPtr = (uint16_t*)(FlashRamStorage + (ADInputAddress - (CART_DOM2_ADDR2_START + (3 << 18)) + (sizeof(FlashRamStorage) / 4) * 3));
            } else if (ADInputAddress >= (CART_DOM2_ADDR2_START | (2 << 18))) {
                ReadPtr = (uint16_t*)(FlashRamStorage + (ADInputAddress - (CART_DOM2_ADDR2_START + (2 << 18)) + (sizeof(FlashRamStorage) / 4) * 2));
            } else if (ADInputAddress >= (CART_DOM2_ADDR2_START | (1 << 18))) {
                ReadPtr = (uint16_t*)(FlashRamStorage + (ADInputAddress - (CART_DOM2_ADDR2_START + (1 << 18)) + (sizeof(FlashRamStorage) / 4) * 1));
            }
    } else if ((ADInputAddress >= N64_ROM_BASE) && (ADInputAddress <= (N64_ROM_BASE + RomMaxSize))) {
        //NVIC_SetVector(EXTI1_IRQn, (uint32_t)&EXTI1_IRQHandler);
        ReadPtr = ((uint16_t*)(ram + (ADInputAddress - N64_ROM_BASE)));
    } else if (ADInputAddress >= CART_DOM2_ADDR1_START && ADInputAddress <= CART_DOM2_ADDR1_END) {
        //NVIC_SetVector(EXTI1_IRQn, (uint32_t)&EXTI1_IRQHandler);
        ReadPtr = (uint16_t*)&NullMem;
    } else if (ADInputAddress >= CART_DOM1_ADDR1_START && ADInputAddress <= CART_DOM1_ADDR1_END) {
        //NVIC_SetVector(EXTI1_IRQn, (uint32_t)&EXTI1_IRQHandler);
        ReadPtr = (uint16_t*)&NullMem;
    } else if (ADInputAddress >= CART_MENU_ADDR_START && ADInputAddress <= CART_MENU_ADDR_END) {
        //NVIC_SetVector(EXTI19_IRQn, (uint32_t)&EXTI1_IRQHandler);
        ReadPtr = (uint16_t*)(((unsigned char*)MenuBase) + (ADInputAddress - CART_MENU_ADDR_START));
    } else {
        ReadPtr = (uint16_t*)NullMem;// (uint16_t*)(ADInputAddress);
    }

#if (PI_PRECALCULATE_OUT_VALUE != 0)
    const uint16_t PrefetchRead = *ReadPtr;
    ValueA = PrefetchRead;
    ValueB = (((PrefetchRead >> 4) & 0x03F0) | (PrefetchRead & 0xC000));
#endif
}
#endif

extern "C"
ITCM_FUNCTION
void BDMA_Channel0_IRQHandler(void)
{
    ((BDMA_Base_Registers *)(DMA_Handle_Channel0.StreamBaseAddress))->IFCR = 1;
    //ADInputAddress = (((PortABuffer[0] & 0xFF) | ((PortBBuffer[0] & 0x03F0) << 4) | (PortBBuffer[0] & 0xC000)) << 16)
    //                 | (PortABuffer[1] & 0xFE) | ((PortBBuffer[1] & 0x03F0) << 4) | (PortBBuffer[1] & 0xC000);

    ADInputAddress = ((PortBBuffer[0] & 0x03F003F0) << 4) |
                     (PortBBuffer[0] & 0xC000C000) |
                     (PortABuffer[0] & 0x00FE00FF);

    ADInputAddress = (ADInputAddress >> 16) | (ADInputAddress << 16);
    SpeedTracking[1] = DWT->CYCCNT;

    //ConstructAddress();
    SpeedTracking[2] = DWT->CYCCNT;

    if ((ADInputAddress >= CART_DOM2_ADDR2_START) && (ADInputAddress <= CART_DOM2_ADDR2_END)) {
        //lat=0x05 pwd=0x0c pgs=0xd rls=0x2
        if (ADInputAddress < (CART_DOM2_ADDR2_START | (1 << 18))) {
            ReadPtr = (uint16_t*)(FlashRamStorage + (ADInputAddress - CART_DOM2_ADDR2_START));
        //} else {
        //    ReadPtr = (uint16_t*)(FlashRamStorage);
        } else if (ADInputAddress >= (CART_DOM2_ADDR2_START | (3 << 18))) {
            ReadPtr = (uint16_t*)(FlashRamStorage + (ADInputAddress - (CART_DOM2_ADDR2_START + (3 << 18)) + (sizeof(FlashRamStorage) / 4) * 3));
        } else if (ADInputAddress >= (CART_DOM2_ADDR2_START | (2 << 18))) {
            ReadPtr = (uint16_t*)(FlashRamStorage + (ADInputAddress - (CART_DOM2_ADDR2_START + (2 << 18)) + (sizeof(FlashRamStorage) / 4) * 2));
        } else if (ADInputAddress >= (CART_DOM2_ADDR2_START | (1 << 18))) {
            ReadPtr = (uint16_t*)(FlashRamStorage + (ADInputAddress - (CART_DOM2_ADDR2_START + (1 << 18)) + (sizeof(FlashRamStorage) / 4) * 1));
        }
    } else if ((ADInputAddress >= N64_ROM_BASE) && (ADInputAddress <= (N64_ROM_BASE + RomMaxSize))) {
        //NVIC_SetVector(EXTI1_IRQn, (uint32_t)&EXTI1_IRQHandler);
        if (gUseBootLoader == false) {
            ReadPtr = ((uint16_t*)(ram + (ADInputAddress - N64_ROM_BASE)));
        } else {
            ReadPtr = (uint16_t*)(bootrom + ((ADInputAddress - N64_ROM_BASE) & 0x0fffffff));
        }
    } else if (ADInputAddress >= CART_DOM2_ADDR1_START && ADInputAddress <= CART_DOM2_ADDR1_END) {
        //NVIC_SetVector(EXTI1_IRQn, (uint32_t)&EXTI1_IRQHandler);
        ReadPtr = (uint16_t*)&NullMem;
    } else if (ADInputAddress >= CART_DOM1_ADDR1_START && ADInputAddress <= CART_DOM1_ADDR1_END) {
        //NVIC_SetVector(EXTI1_IRQn, (uint32_t)&EXTI1_IRQHandler);
        ReadPtr = (uint16_t*)&NullMem;
    } else if (ADInputAddress >= CART_MENU_ADDR_START && ADInputAddress <= CART_MENU_ADDR_END) {
        //NVIC_SetVector(EXTI19_IRQn, (uint32_t)&EXTI1_IRQHandler);
        ReadPtr = (uint16_t*)(((unsigned char*)MenuBase) + (ADInputAddress - CART_MENU_ADDR_START));
    } else {
        ReadPtr = (uint16_t*)NullMem;// (uint16_t*)(ADInputAddress);
    }

    SpeedTracking[3] = DWT->CYCCNT;
#if (PI_PRECALCULATE_OUT_VALUE != 0)
    const uint16_t PrefetchRead = *ReadPtr;
    ValueA = PrefetchRead;
    ValueB = (((PrefetchRead >> 4) & 0x03F0) | (PrefetchRead & 0xC000));
#endif
    SpeedTracking[4] = DWT->CYCCNT;
}

extern "C"
ITCM_FUNCTION
void EXTI0_IRQHandler(void)
{
    EXTI->PR1 = ALE_L;
    // Invalidate the data cache by address.
    SCB->DCIMVAC = (uint32_t)PortABuffer;
#if HANDLE_ADDRESS_CONSTRUCTION_IN_ISR
    while ((((BDMA_Base_Registers *)(DMA_Handle_Channel1.StreamBaseAddress))->ISR & ((1<<4) | 1)) != ((1<<4) | 1)) {
    }

    ((BDMA_Base_Registers *)(DMA_Handle_Channel0.StreamBaseAddress))->IFCR = 1 | 1 << 4;
    ConstructAddress();
#endif
    ReadOffset = 0;
    //SpeedTracking[0] = DWT->CYCCNT;
}

extern "C"
ITCM_FUNCTION
void LPTIM1_PISetInputMode(void)
{
#if (USE_OPEN_DRAIN_OUTPUT == 0)
    GPIOA->ODR = 0x00;
    GPIOB->ODR = 0x0000;
    SET_PI_INPUT_MODE;
#else
    // Switch to Input.
    GPIOA->ODR = 0xFF;
    GPIOB->ODR = 0xC3F0;
#endif
}