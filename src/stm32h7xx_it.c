/**
  ******************************************************************************
  * @file    DMA/DMAMUX_RequestGen/Src/stm32h7xx_it.c
  * @author  MCD Application Team
  * @brief   Main Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
//#include "main.h"
//#include "stm32h7xx_it.h"

/** @addtogroup STM32H7xx_HAL_Examples
  * @{
  */

/** @addtogroup DMAMUX_RequestGen
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
extern DMA_HandleTypeDef DMA_Handle_Channel0;
extern DMA_HandleTypeDef DMA_Handle_Channel1;
extern DMA_HandleTypeDef DMA_Handle_Channel2;
extern DMA_HandleTypeDef DMA_Handle_Channel3; // MODER switching.
extern uint32_t OverflowCounter;
void BDMA_Channel0_IRQHandler_(void);
void BDMA_Channel1_IRQHandler_(void);
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M7 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void)
{
}

/******************************************************************************/
/*                 STM32H7xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32h7xx.s).                                               */
/******************************************************************************/

/**
  * @brief  This function handles BDMA channel0 interrupt request.
  * @param  None
  * @retval None
  */
#if 0
void BDMA_Channel0_IRQHandler(void)
{
  // TODO: remove this and simplify the common handler.
  HAL_DMA_IRQHandler(&DMA_Handle_Channel0);
  BDMA_Channel0_IRQHandler_();
}

void BDMA_Channel1_IRQHandler(void)
{
  // TODO: remove this and simplify the common handler.
  HAL_DMA_IRQHandler(&DMA_Handle_Channel1);
  BDMA_Channel1_IRQHandler_();
}
#endif
#define ITCM_FUNCTION __attribute__((long_call, section(".itcm_text")))
/**
  * @brief  This function handles DMAMUX2  interrupt request.
  * @param  None
  * @retval None
  */
ITCM_FUNCTION void DMAMUX2_OVR_IRQHandler(void)
{
  //while(1) {
  //  int a = 0;
  //  a += 1;
  //} 
  HAL_DMAEx_MUX_IRQHandler(&DMA_Handle_Channel0);
  HAL_DMAEx_MUX_IRQHandler(&DMA_Handle_Channel1);
  HAL_DMAEx_MUX_IRQHandler(&DMA_Handle_Channel2);
  HAL_DMAEx_MUX_IRQHandler(&DMA_Handle_Channel3);
  OverflowCounter += 1;
}

/**
  * @}
  */

/**
  * @}
  */
