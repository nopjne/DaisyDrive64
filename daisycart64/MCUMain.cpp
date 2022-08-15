#include <stdio.h>
#include <string.h>
#include "daisy_seed.h"
#include "fatfs.h"
#include "stm32h7xx_hal.h"
#include "sys/system.h"

#ifndef _FS_EXFAT
#error FAT FS NEEDS TO BE ENABLED
#endif

//#define MENU_ROM_FILE_NAME "menu.z64"
#define MENU_ROM_FILE_NAME "Super Mario 64 (USA).n64"
//#define MENU_ROM_FILE_NAME "Resident Evil 2 (USA).n64"

#define N64_ROM_BASE 0x10000000
using namespace daisy;
BYTE *Sram4Buffer = (BYTE*)0x38000000;
int8_t *ram = (int8_t *)0xC0000000;

static DaisySeed hw;

SdmmcHandler   sd;
FatFSInterface fsi;
FIL            SDFile;

enum AD_INTERFACE_DIRECTION {
    AD_IN,
    AD_OUT
};

int cic_init(void);
int cic_run(void);

void BlinkAndDie(int wait1, int wait2)
{
    while(1) {
        GPIOC->BSRR = (0x1 << 7);
        System::Delay(wait1);
        GPIOC->BSRR = (0x1 << 7) << 16;
        System::Delay(wait2);
    }
}

AD_INTERFACE_DIRECTION AdInterfaceDirection;
#define GP_SPEED GPIO_SPEED_FREQ_VERY_HIGH
inline void SwitchAdInterfaceDirection(AD_INTERFACE_DIRECTION direction)
{
    if (direction == AD_IN) {
        //GPIO_InitTypeDef PortAPins = {0xFF, GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
        //GPIO_InitTypeDef PortBPins = {0xC3F0, GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
        //HAL_GPIO_Init(GPIOA, &PortAPins);
        //HAL_GPIO_Init(GPIOB, &PortBPins);
        //GPIOA->MODER &= 0xFFFF000F;
        GPIOA->MODER &= 0xFFFF0003;
        GPIOB->MODER &= 0x0FF000FF;

    } else {
        //GPIO_InitTypeDef PortAPins = {0xFF, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GP_SPEED, 0};
        //GPIO_InitTypeDef PortBPins = {0xC3F0, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GP_SPEED, 0};
        //HAL_GPIO_Init(GPIOA, &PortAPins);
        //HAL_GPIO_Init(GPIOB, &PortBPins);
        GPIOA->MODER |= 0x00005557;
        GPIOB->MODER |= 0x50055500;
    }

    AdInterfaceDirection = direction;
}

extern "C" void __EXTI0_IRQHandler(void)
{
        __IO uint32_t *regaddr;
    uint32_t maskline;
    uint32_t offset;

    /* compute line register offset and line mask */
    offset = ((EXTI_LINE_1 & EXTI_REG_MASK) >> EXTI_REG_SHIFT);
    maskline = (1UL << (EXTI_LINE_1 & EXTI_PIN_MASK));
    regaddr = (__IO uint32_t *)(&EXTI->PR1 + (0x04U * offset));

    /* Clear Pending bit */
    *regaddr =  maskline;
}

extern "C" void __EXTI1_IRQHandler(void)
{
    //volatile uint32_t PIValue;
    volatile uint32_t PortAValue = GPIOA->IDR;
    volatile uint32_t PortBValue = GPIOB->IDR;

    __IO uint32_t *regaddr;
    uint32_t maskline;
    uint32_t offset;

    /* compute line register offset and line mask */
    offset = ((EXTI_LINE_1 & EXTI_REG_MASK) >> EXTI_REG_SHIFT);
    maskline = (1UL << (EXTI_LINE_1 & EXTI_PIN_MASK));
    regaddr = (__IO uint32_t *)(&EXTI->PR1 + (0x04U * offset));

    /* Clear Pending bit */
    *regaddr =  maskline;

    //if(__HAL_GPIO_EXTI_GET_FLAG(GPIO_PIN_1)) {
    /* USER CODE BEGIN EXTI4_15_IRQn 0 */ 
        //GPIOA->BSRR |= 1;
        //GPIOA->BSRR |= 1 << 16;
    //}
    /* USER CODE END EXTI4_15_IRQn 0 */
    //HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_13); 
    /* USER CODE BEGIN EXTI4_15_IRQn 1 */ 
    /* USER CODE END EXTI4_15_IRQn 1 */ 
}

uint32_t ADInputAddress = 0;
uint32_t PrefetchRead = 0;
extern "C" void BDMA_Channel0_IRQHandler_(void)
{
    //SCB_CleanDCache_by_Addr( (uint32_t*)Sram4Buffer, 8);
    uint32_t PIValue = (Sram4Buffer[0] & 0xFF) | ((Sram4Buffer[1] & 0x03F0) << 4) | (Sram4Buffer[1] & 0xC000);
    if (0) { // ALE_L
        ADInputAddress = (ADInputAddress & 0xFFFF0000) | ((0x0000FFFF & PIValue));
    } else { // ALE_H
        ADInputAddress = (ADInputAddress & 0x0000FFFF) | ((0x0000FFFF & PIValue) << 16);
    }

    if ((ADInputAddress < N64_ROM_BASE) || (ADInputAddress > (N64_ROM_BASE + (64 * 1024 * 1024)))) {
        PrefetchRead = 0;
    } else {
        PrefetchRead = *((uint32_t*)(ram + (ADInputAddress - N64_ROM_BASE)));
    }
}

__IO   uint32_t DMA_TransferErrorFlag = 0;
DMA_HandleTypeDef DMA_Handle_Channel0;
DMA_HandleTypeDef DMA_Handle_Channel1;
DMA_HandleTypeDef DMA_Handle_Channel2;
LPTIM_HandleTypeDef  LptimHandle;

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void CPU_CACHE_Enable(void);
void LPTIM_Config(void);
static void HAL_TransferError(DMA_HandleTypeDef *hdma);
static void Error_Handler(void);

/* Private functions ---------------------------------------------------------*/

#if 0
DMA_HandleTypeDef DMA_Handle;
/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int BDMA_Config(void)
{
    GPIOC->BSRR = (0x1 << 7) << 16;
    const uint32_t BDMA_BUFFER_SIZE = 512;
    BYTE* abDmaDebug = (BYTE*)0x38000000; 
    for( int i = 0; i < BDMA_BUFFER_SIZE; i += 8) {
        abDmaDebug[i] = 0xF1;
        abDmaDebug[i + 1] = 0x00;
        abDmaDebug[i + 2] = 0x00;
        abDmaDebug[i + 3] = 0x00;

        abDmaDebug[i + 4] = 0x00;
        abDmaDebug[i + 5] = 0x00;
        abDmaDebug[i + 6] = 0x00;
        abDmaDebug[i + 7] = 0x00;
    }

  HAL_DMA_MuxRequestGeneratorConfigTypeDef dmamux_ReqGenParams  = {0};

  /*##-2- Configure the DMA ##################################################*/
/* Enable BDMA clock */
  __HAL_RCC_BDMA_CLK_ENABLE();

  /* Configure the DMA handler for Transmission process     */
  /* DMA mode is set to circular for an infinite DMA transfer */
  DMA_Handle.Instance                 = BDMA_Channel0;

  DMA_Handle.Init.Request             = BDMA_REQUEST_GENERATOR0;
  DMA_Handle.Init.Direction           = DMA_MEMORY_TO_PERIPH;
  DMA_Handle.Init.PeriphInc           = DMA_PINC_DISABLE;
  DMA_Handle.Init.MemInc              = DMA_MINC_ENABLE;
  DMA_Handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  DMA_Handle.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
  DMA_Handle.Init.Mode                = DMA_CIRCULAR;
  DMA_Handle.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
  DMA_Handle.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
  DMA_Handle.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
  DMA_Handle.Init.MemBurst            = DMA_PBURST_SINGLE;
  DMA_Handle.Init.PeriphBurst         = DMA_PBURST_SINGLE;

  /* Initialize the DMA with for Transmission process */
  HAL_StatusTypeDef dmares = HAL_OK;
  dmares = HAL_DMA_Init(&DMA_Handle);
    if (dmares != HAL_OK) {
        Error_Handler();
    }

  /* Select Callbacks functions called after Transfer complete and Transfer error */
  dmares = HAL_DMA_RegisterCallback(&DMA_Handle, HAL_DMA_XFER_CPLT_CB_ID, NULL);
  if (dmares != HAL_OK) {
        Error_Handler();
    }
  dmares = HAL_DMA_RegisterCallback(&DMA_Handle, HAL_DMA_XFER_ERROR_CB_ID, HAL_TransferError);
if (dmares != HAL_OK) {
        Error_Handler();
    }
  /* NVIC configuration for DMA transfer complete interrupt*/
  HAL_NVIC_SetPriority(BDMA_Channel0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(BDMA_Channel0_IRQn);

  /*##-3- Configure and enable the DMAMUX Request generator  ####################*/
  dmamux_ReqGenParams.SignalID  = HAL_DMAMUX2_REQ_GEN_LPTIM3_OUT; /* External request signal is LPTIM2 signal */
  dmamux_ReqGenParams.Polarity  = HAL_DMAMUX_REQ_GEN_FALLING;      /* External request signal edge is Rising  */
  dmamux_ReqGenParams.RequestNumber = 1;                          /* 1 requests on each edge of the external request signal  */

  dmares = HAL_DMAEx_ConfigMuxRequestGenerator(&DMA_Handle, &dmamux_ReqGenParams);
  if (dmares != HAL_OK) {
        Error_Handler();
        }
  /* NVIC configuration for DMAMUX request generator overrun errors*/
  HAL_NVIC_SetPriority(DMAMUX2_OVR_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMAMUX2_OVR_IRQn);

  dmares = HAL_DMAEx_EnableMuxRequestGenerator (&DMA_Handle);
  if (dmares != HAL_OK) {
    Error_Handler();
  }
  /*##-4- Configure and enable the LPTIM2 used as DMA external request generator signal #####*/
  LPTIM_Config();
  

  /*##-5- Start the DMA transfer ################################################*/
  /* DMA source buffer is  SRC_BUFFER_LED1_TOGGLE containing values to be written
  to GPIOB ODR register in order to turn LED1 On/Off each time comes a request from the DMAMUX request generator */

  SCB_CleanDCache_by_Addr( (uint32_t*)abDmaDebug, BDMA_BUFFER_SIZE);
  uint32_t DstAddr = (uint32_t)&(GPIOA->ODR);
  dmares = HAL_DMA_Start_IT(&DMA_Handle, (uint32_t)abDmaDebug, DstAddr, 2);
  if (dmares != HAL_OK) {
      Error_Handler();
  }

  /* Infinite loop */
  while (1)
  {
    //dmares = HAL_DMA_Start_IT(&DMA_Handle, (uint32_t)abDmaDebug, (uint32_t)&GPIOA->ODR, BDMA_BUFFER_SIZE);
    //if (dmares != HAL_OK) {
    //    Error_Handler();
    //}

    if(DMA_TransferErrorFlag != 0)
    {
      Error_Handler();
    }
  }
}
#endif // Known good BDMA timer toggle GPIO pin.


int InitializeDmaChannels(void)
{
    GPIOC->BSRR = (0x1 << 7) << 16;
    const uint32_t BDMA_BUFFER_SIZE = 512;
    Sram4Buffer[0] = 0;
    Sram4Buffer[1] = 0;
    SCB_CleanDCache_by_Addr( (uint32_t*)Sram4Buffer, 8);

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
        DMA_Handle_Channel0.Init.MemInc              = DMA_MINC_DISABLE;
        DMA_Handle_Channel0.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
        DMA_Handle_Channel0.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
        DMA_Handle_Channel0.Init.Mode                = DMA_CIRCULAR;
        DMA_Handle_Channel0.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
        DMA_Handle_Channel0.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
        DMA_Handle_Channel0.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
        DMA_Handle_Channel0.Init.MemBurst            = DMA_PBURST_SINGLE;
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
        dmamux_ReqGenParams.SignalID  = HAL_DMAMUX2_REQ_GEN_EXTI0; /* External request signal is LPTIM2 signal */
        dmamux_ReqGenParams.Polarity  = HAL_DMAMUX_REQ_GEN_RISING;      /* External request signal edge is Rising  */
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

        dmares = HAL_DMA_Start_IT(&DMA_Handle_Channel0, (uint32_t)&(GPIOA->IDR), (uint32_t)(Sram4Buffer), 1);
        if (dmares != HAL_OK) {
            Error_Handler();
        }
    }

    { // Channel 0 init.
        /* Configure the DMA handler for Transmission process     */
        /* DMA mode is set to circular for an infinite DMA transfer */
        DMA_Handle_Channel1.Instance                 = BDMA_Channel1;
        DMA_Handle_Channel1.Init.Request             = BDMA_REQUEST_GENERATOR0;
        DMA_Handle_Channel1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        DMA_Handle_Channel1.Init.PeriphInc           = DMA_PINC_DISABLE;
        DMA_Handle_Channel1.Init.MemInc              = DMA_MINC_DISABLE;
        DMA_Handle_Channel1.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
        DMA_Handle_Channel1.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
        DMA_Handle_Channel1.Init.Mode                = DMA_CIRCULAR;
        DMA_Handle_Channel1.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
        DMA_Handle_Channel1.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
        DMA_Handle_Channel1.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
        DMA_Handle_Channel1.Init.MemBurst            = DMA_PBURST_SINGLE;
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
        dmamux_ReqGenParams.Polarity  = HAL_DMAMUX_REQ_GEN_RISING;
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

        dmares = HAL_DMA_Start_IT(&DMA_Handle_Channel1, (uint32_t)&(GPIOB->IDR), (uint32_t)(Sram4Buffer + 4), 4);
        if (dmares != HAL_OK) {
            Error_Handler();
        }
    }

    /* Infinite loop */
    while (1)
    {
        
        //dmares = HAL_DMA_Start_IT(&DMA_Handle, (uint32_t)abDmaDebug, (uint32_t)&GPIOA->ODR, BDMA_BUFFER_SIZE);
        //if (dmares != HAL_OK) {
        //    Error_Handler();
        //}

        if(DMA_TransferErrorFlag != 0)
        {
            Error_Handler();
        }
    }
}
#if 0
/**
  * @brief  Configure and start the LPTIM2 with 2sec period and 50% duty cycle.
  * @param  None
  * @retval None
  */
void LPTIM_Config(void)
{

  uint32_t periodValue;
  uint32_t pulseValue ;

  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_PeriphCLKInitTypeDef  PeriphClkInitStruct = {0};

  /* Enable the LSE clock source */
#if 0
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  //RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  HAL_StatusTypeDef status;
  status = HAL_RCC_OscConfig(&RCC_OscInitStruct);
  if (status != HAL_OK) {
    Error_Handler();
  }
#endif

    //HAL_TIM_Base_Start  disney_dma_base;
    //HAL_TIM_Base_Start());

  /* LPTIM2 clock source set to LSE*/
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LPTIM3;
  PeriphClkInitStruct.Lptim345ClockSelection = RCC_LPTIM345CLKSOURCE_D3PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
    Error_Handler();
  }

  periodValue = 1;    /* Calculate the Timer  Autoreload value for 2sec period */
  pulseValue  = 0;        /* Set the Timer  pulse value for 50% duty cycle         */

  /* TIM1 Peripheral clock enable */
  __HAL_RCC_LPTIM3_CLK_ENABLE();

  LptimHandle.Instance                           = LPTIM3;
  LptimHandle.Init.CounterSource                 = LPTIM_COUNTERSOURCE_INTERNAL;
  LptimHandle.Init.UpdateMode                    = LPTIM_UPDATE_IMMEDIATE;
  LptimHandle.Init.OutputPolarity                = LPTIM_OUTPUTPOLARITY_LOW;
  LptimHandle.Init.Clock.Source                  = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC;
  LptimHandle.Init.Clock.Prescaler               = LPTIM_PRESCALER_DIV1;
  LptimHandle.Init.UltraLowPowerClock.Polarity   = LPTIM_ACTIVEEDGE_RISING;
  LptimHandle.Init.UltraLowPowerClock.SampleTime = LPTIM_CLOCKSAMPLETIME_DIRECTTRANSITION;
  LptimHandle.Init.Trigger.Source                = LPTIM_TRIGSOURCE_SOFTWARE;
  LptimHandle.Init.Trigger.ActiveEdge            = LPTIM_ACTIVEEDGE_RISING;
  LptimHandle.Init.Trigger.SampleTime            = LPTIM_CLOCKSAMPLETIME_DIRECTTRANSITION;

  if(HAL_LPTIM_Init(&LptimHandle) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }

  /* Start the timer */
  if (HAL_LPTIM_PWM_Start(&LptimHandle, periodValue, pulseValue) != HAL_OK)
  {
    Error_Handler();
  }

}
#endif

static void HAL_TransferError(DMA_HandleTypeDef *hdma)
{
  DMA_TransferErrorFlag = 1;
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
static void Error_Handler(void)
{
    GPIOC->BSRR = (0x1 << 7);
  while(1)
  {
  }
}

#if 0
// DMA-Memory: Global Array in SRAM4, 32byte-aligned
//__attribute__((section(".RAM_D3"))) BYTE abDmaDebug[2048];
#define BDMA_BUFFER_SIZE 2048
void Init_BDMA(){
    __HAL_RCC_BDMA_CLK_ENABLE();
    BYTE* abDmaDebug = (BYTE*)0x38000000;
  RCC->APB4ENR |= RCC_APB4ENR_LPTIM3EN;
  RCC->AHB4ENR |= RCC_AHB4ENR_BDMAEN;
 
  for( int i = 0; i < BDMA_BUFFER_SIZE; i += 2) {
    abDmaDebug[i]= (i);
    abDmaDebug[i + 1]= (i + 1);
  }
 
  SCB_CleanDCache_by_Addr( (uint32_t*)abDmaDebug, BDMA_BUFFER_SIZE);
 
#define PIN_DCLK      "PA1 TL3"
  LPTIM3->CR= LPTIM_CR_ENABLE;
  #define MHZ_LT1_LT2_LT3_LT4_LT5  100    //MHZ_APB3
  #define FREQU_LT3   1000000   //1MHz
  #define LT3_ARR 100  // MHZ_LT1_LT2_LT3_LT4_LT5*1000000 / FREQU_LT3
  LPTIM3->ARR= LT3_ARR-1;
  LPTIM3->CMP= LT3_ARR/2;

  //1=Requestgenerator 0
  DMAMUX2_Channel0->CCR=  (1 << DMAMUX_CxCR_DMAREQ_ID_Pos);
  //Request-Generator:
  //12=LPTIM3_OUT (Trigger-Input)
  DMAMUX2_RequestGenerator0->RGCR=  (12 << DMAMUX_RGxCR_SIG_ID_Pos) |
                                    (1 << DMAMUX_RGxCR_GPOL_Pos);
 
  DMAMUX2_Channel0->CCR|= DMAMUX_CxCR_EGE;
  DMAMUX2_RequestGenerator0->RGCR|= DMAMUX_RGxCR_GE;
 
  BDMA_Channel0->CCR= (2 << BDMA_CCR_PL_Pos) |
                      // SIZE for GPIOG->ODR: 1 or 2 (NOT 0)
                      (0 << BDMA_CCR_MSIZE_Pos) |   
                      (1 << BDMA_CCR_PSIZE_Pos) |
                      (1 << BDMA_CCR_MINC_Pos) |
                      (0 << BDMA_CCR_PINC_Pos) |
                      (0 << BDMA_CCR_CIRC_Pos) |
                      (1 << BDMA_CCR_DIR_Pos);
  //ATTENTION! Memory in SRAM4!!                      
  BDMA_Channel0->CM0AR=(DWORD)&abDmaDebug;
  BDMA_Channel0->CPAR= (DWORD)&GPIOA->ODR;
}
 
void Fire_BDMA(){
  LPTIM3->CR= 0;
  BDMA_Channel0->CCR &=~(1 << BDMA_CCR_EN_Pos);
  BDMA->IFCR= (DWORD)-1;
  BDMA_Channel0->CNDTR = BDMA_BUFFER_SIZE;
  BDMA_Channel0->CCR |=(1 << BDMA_CCR_EN_Pos);
  LPTIM3->CR= LPTIM_CR_ENABLE;
  LPTIM3->CR= LPTIM_CR_ENABLE | LPTIM_CR_CNTSTRT;
}
#endif

int main(void)
{
    size_t bytesread = 0;

    // Init hardware
    //SCB_EnableDCache();
    //SCB_EnableICache();
    hw.Init(true);

#if 0
    typedef struct
    {
    uint32_t Pin;       /*!< Specifies the GPIO pins to be configured.
                            This parameter can be any value of @ref GPIO_pins_define */

    uint32_t Mode;      /*!< Specifies the operating mode for the selected pins.
                            This parameter can be a value of @ref GPIO_mode_define */

    uint32_t Pull;      /*!< Specifies the Pull-up or Pull-Down activation for the selected pins.
                            This parameter can be a value of @ref GPIO_pull_define */

    uint32_t Speed;     /*!< Specifies the speed for the selected pins.
                            This parameter can be a value of @ref GPIO_speed_define */

    uint32_t Alternate;  /*!< Peripheral to be connected to the selected pins.
                                This parameter can be a value of @ref GPIO_Alternate_function_selection */
    } GPIO_InitTypeDef;
#endif

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    //__HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    // Preconfigure GPIO PortA and PortB, so the following directional changes can be faster.
    GPIO_InitTypeDef PortAPins = {0xFF, GPIO_MODE_OUTPUT_OD, GPIO_NOPULL, GP_SPEED, 0};
    GPIO_InitTypeDef PortBPins = {0xC3F0, GPIO_MODE_OUTPUT_OD, GPIO_NOPULL, GP_SPEED, 0};
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

    GPIO_InitTypeDef PortDPins = {(1 << 11), GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    HAL_GPIO_Init(GPIOD, &PortDPins);

    GPIO_InitTypeDef PortGPins = {(1 << 10) | (1 << 11), GPIO_MODE_INPUT, GPIO_NOPULL, GP_SPEED, 0};
    PortGPins.Pin |= (1 << 9);
    HAL_GPIO_Init(GPIOG, &PortGPins);

    GPIOC->BSRR = (0x1 << 7);

    // test
    //HAL_EnableCompensationCell();
#if 0
    HAL_EnableCompensationCell();
    //PortAPins = {0x02, GPIO_MODE_AF_PP, GPIO_PULLDOWN, GP_SPEED, GPIO_AF3_LPTIM3};
    //HAL_GPIO_Init(GPIOA, &PortAPins);
    PortAPins = {0x01, GPIO_MODE_OUTPUT_OD, GPIO_PULLDOWN, GP_SPEED, 0};
    GPIOA->ODR = 0xFFFFFFFF;
    BDMA_Config();
#endif
    // !test

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

        // Open and write the test file to the SD Card.
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
    volatile uint32_t PortA = GPIOA->IDR;
    volatile uint32_t PortB = GPIOB->IDR;
    volatile uint32_t PortC = GPIOC->IDR;


    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = 1 << 1;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
    //NVIC_SetVector(EXTI0_IRQn, (uint32_t)&EXTI0_IRQHandler);
    InitializeDmaChannels();

#if 0
    // Setup GPIO
    (uint32_t *)GPIOA;
    (uint32_t *)GPIOB;
    (uint32_t *)GPIOC;
    (uint32_t *)GPIOD;
    (uint32_t *)GPIOE;
    (uint32_t *)GPIOF;
    (uint32_t *)GPIOG;

    constexpr Pin D25 = Pin(PORTA, 0); // AD0
    constexpr Pin D24 = Pin(PORTA, 1); // AD1
    constexpr Pin D28 = Pin(PORTA, 2); // AD2
    constexpr Pin D16 = Pin(PORTA, 3); // AD3
    constexpr Pin D23 = Pin(PORTA, 4); // AD4
    constexpr Pin D22 = Pin(PORTA, 5); // AD5
    constexpr Pin D19 = Pin(PORTA, 6); // AD6
    constexpr Pin D18 = Pin(PORTA, 7); // AD7

    constexpr Pin D17 = Pin(PORTB, 1); // Read.
    constexpr Pin D9  = Pin(PORTB, 4); // AD8
    constexpr Pin D10 = Pin(PORTB, 5); // AD9
    constexpr Pin D13 = Pin(PORTB, 6); // AD10
    constexpr Pin D14 = Pin(PORTB, 7); // AD11
    constexpr Pin D11 = Pin(PORTB, 8); // AD12
    constexpr Pin D12 = Pin(PORTB, 9); // AD13
    constexpr Pin D0  = Pin(PORTB, 12); // ALE_L
    constexpr Pin D29 = Pin(PORTB, 14); // AD14
    constexpr Pin D30 = Pin(PORTB, 15); // AD15

    constexpr Pin D15 = Pin(PORTC, 0); // Write.
    constexpr Pin D20 = Pin(PORTC, 1); // ALE_H
    constexpr Pin D21 = Pin(PORTC, 4); // S-DATA
    constexpr Pin D4  = Pin(PORTC, 8); // SD card D0
    constexpr Pin D3  = Pin(PORTC, 9); // SD card D1
    constexpr Pin D2  = Pin(PORTC, 10); // SD card D2
    constexpr Pin D1  = Pin(PORTC, 11); // SD card D3
    constexpr Pin D6  = Pin(PORTC, 12); // SD card CLK

    constexpr Pin D5  = Pin(PORTD, 2); // SD card CMD
    constexpr Pin D26 = Pin(PORTD, 11); // N64_Cold_reset

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

#if 0
    typedef struct
    {
      __IO uint32_t MODER;    /*!< GPIO port mode register,               Address offset: 0x00      */
      __IO uint32_t OTYPER;   /*!< GPIO port output type register,        Address offset: 0x04      */
      __IO uint32_t OSPEEDR;  /*!< GPIO port output speed register,       Address offset: 0x08      */
      __IO uint32_t PUPDR;    /*!< GPIO port pull-up/pull-down register,  Address offset: 0x0C      */
      __IO uint32_t IDR;      /*!< GPIO port input data register,         Address offset: 0x10      */
      __IO uint32_t ODR;      /*!< GPIO port output data register,        Address offset: 0x14      */
      __IO uint32_t BSRR;     /*!< GPIO port bit set/reset,               Address offset: 0x18      */
      __IO uint32_t LCKR;     /*!< GPIO port configuration lock register, Address offset: 0x1C      */
      __IO uint32_t AFR[2];   /*!< GPIO alternate function registers,     Address offset: 0x20-0x24 */
    } GPIO_TypeDef;
    
    /** @defgroup GPIO_speed_define  GPIO speed define
      * @brief GPIO Output Maximum frequency
      * @{
      */
    #define  GPIO_SPEED_FREQ_LOW         (0x00000000U)  /*!< Low speed     */
    #define  GPIO_SPEED_FREQ_MEDIUM      (0x00000001U)  /*!< Medium speed  */
    #define  GPIO_SPEED_FREQ_HIGH        (0x00000002U)  /*!< Fast speed    */
    #define  GPIO_SPEED_FREQ_VERY_HIGH   (0x00000003U)  /*!< High speed    */

    /** @defgroup GPIO_pull_define  GPIO pull define
      * @brief GPIO Pull-Up or Pull-Down Activation
      * @{
      */
    #define  GPIO_NOPULL        (0x00000000U)   /*!< No Pull-up or Pull-down activation  */
    #define  GPIO_PULLUP        (0x00000001U)   /*!< Pull-up activation                  */
    #define  GPIO_PULLDOWN      (0x00000002U)   /*!< Pull-down activation                */

    typedef struct {
      uint32_t Pin;       /*!< Specifies the GPIO pins to be configured.
                               This parameter can be any value of @ref GPIO_pins_define */
      uint32_t Mode;      /*!< Specifies the operating mode for the selected pins.
                               This parameter can be a value of @ref GPIO_mode_define */
      uint32_t Pull;      /*!< Specifies the Pull-up or Pull-Down activation for the selected pins.
                               This parameter can be a value of @ref GPIO_pull_define */
      uint32_t Speed;     /*!< Specifies the speed for the selected pins.
                               This parameter can be a value of @ref GPIO_speed_define */
      uint32_t Alternate;  /*!< Peripheral to be connected to the selected pins.
                                This parameter can be a value of @ref GPIO_Alternate_function_selection */
    } GPIO_InitTypeDef;
#endif

    cic_init();

    volatile uint32_t Address = 0;
    volatile uint32_t ReadOffset = 0;
    volatile uint32_t ALE_L_prev = 0;
    volatile uint32_t ALE_H_prev = 0;
    volatile uint32_t READ_INT_prev = 0;
    SwitchAdInterfaceDirection(AD_IN);

    //while(1) {
    //    uint32_t PortCValue = GPIOC->IDR;
    //    uint32_t ALE_H = (PortCValue & (1 << 1));
    //    uint32_t ALE_H_prev;
    //    if ((ALE_H != ALE_H_prev) && (ALE_H == 0)) {
    //        GPIOA->BSRR |= 1;
    //        GPIOA->BSRR |= 1 << 16;
    //    }
//
    //    ALE_H_prev = ALE_H;
    //}
    //SwitchAdInterfaceDirection(AD_OUT);
    //while (1) {
    //    //uint32_t Value = 0;
    //    GPIOA->ODR = 0xFF;
    //    //GPIOB->ODR = (((Value >> 4) & 0x03F0) | (Value & 0xC000));
    //    //Value = 0xFFFF;
    //    GPIOA->ODR = 0;
    //    //GPIOB->ODR = (((Value >> 4) & 0x03F0) | (Value & 0xC000));
    //}

    uint32_t ReadTemp = 0;
    volatile uint32_t PortAPrev = 0;
    volatile uint32_t PortBPrev = 0;
    volatile uint32_t PortAPrev2 = 0;
    volatile uint32_t PortBPrev2 = 0;
    // Main loop for N64 interface
    while (1) {
        // Read GPIO
        uint32_t PIValue;
        uint32_t PortAValue = GPIOA->IDR;
        uint32_t PortBValue = GPIOB->IDR;
        uint32_t PortCValue = GPIOC->IDR;

        // Handle CIC -- disable for now. Should be handled by the pico.
        //cic_run();

        // Check RD WR ALE_L ALE_H -- 
        uint32_t READ_INT = (PortBValue & (1 << 1));
        uint32_t ALE_L = (PortBValue & (1 << 12));
        uint32_t ALE_H = (PortCValue & (1 << 1));

        // DMA1, DMA2 DMAMUX, MDMA, BDMA
#if 0
        if (((ALE_L != ALE_L_prev) && (ALE_L != 0)) || 
            ((ALE_H != ALE_H_prev) && (ALE_H != 0))) {

            // Set AD ports to input.
            if (AdInterfaceDirection != AD_IN) {
                SwitchAdInterfaceDirection(AD_IN);
            }
        }
#endif

        // ALE_L set 
        if ((ALE_L != ALE_L_prev) && (ALE_L == 0)) {
            PIValue = (PortAPrev2 & 0xFF) | ((PortBValue & 0x03F0) << 4) | (PortBValue & 0xC000);
            Address = (Address & 0xFFFF0000) | (0x0000FFFF & PIValue);
            ReadOffset = 0;

            // Prefetch.
            if ((Address < N64_ROM_BASE) || (Address > (N64_ROM_BASE + (64 * 1024 * 1024)))) {
                ReadTemp = 0;
                if (Address == 0) {
                    GPIOC->BSRR = (0x1 << 7);
                }
            } else {
                ReadTemp = *((uint32_t*)(ram + (Address - N64_ROM_BASE) + ReadOffset));
            }

            if (Address == N64_ROM_BASE) {
                ReadTemp = 0x20408037;
            }
        }

        if ((ALE_H != ALE_H_prev) && (ALE_H == 0)) {
            PIValue = (PortAPrev2 & 0xFF) | ((PortBPrev2 & 0x03F0) << 4) | (PortBPrev2 & 0xC000);
            Address = (Address & 0x0000FFFF) | ((0x0000FFFF & PIValue) << 16);
            ReadOffset = 0;

            // Prefetch.
            if ((Address < N64_ROM_BASE) || (Address > (N64_ROM_BASE + (64 * 1024 * 1024)))) {
                ReadTemp = 0;
                if (Address == 0) {
                    GPIOC->BSRR = (0x1 << 7);
                }
            } else {
                ReadTemp = *((uint32_t*)(ram + (Address - N64_ROM_BASE) + ReadOffset));
            }
        }

        // Prefetch on rising edge of READ line.
        if ((READ_INT != READ_INT_prev) && (READ_INT != 0)) {
            if ((ReadOffset % 4) == 0) {
                if ((Address < N64_ROM_BASE) || (Address > (N64_ROM_BASE + (64 * 1024 * 1024)))) {
                    ReadTemp = 0;
                } else {
                    ReadTemp = *((uint32_t*)(ram + (Address - N64_ROM_BASE) + ReadOffset));
                }
            }
        }

        if ((READ_INT != READ_INT_prev) && (READ_INT == 0)) {
            // Check if AD direction needs to switch.
            if (AdInterfaceDirection != AD_OUT) {
                SwitchAdInterfaceDirection(AD_OUT);
            }

            // Output AD.
            const uint32_t Value = (((ReadOffset & 2) != 0) ? ReadTemp : (ReadTemp >> 16));
            GPIOA->ODR = (Value & 0xFF);
            GPIOB->ODR = (((Value >> 4) & 0x03F0) | (Value & 0xC000));
            ReadOffset += 2;
        }

        // EXTI0_IRQn; // Read interrupt.
        // EXTI1_IRQn; // ALE_L interrupt.
        // EXTI2_IRQn; // ALE_H interrupt.
        // EXTI3_IRQn; // Write interrupt.
        // EXTI4_IRQn; // CiC interrupt.
        // EXTI9_5_IRQn;

        ALE_L_prev = ALE_L;
        ALE_H_prev = ALE_H;
        READ_INT_prev = READ_INT;
        PortAPrev2 = PortAPrev;
        PortBPrev2 = PortBPrev; 
        PortAPrev = PortAValue;
        PortBPrev = PortBValue;
    }
}