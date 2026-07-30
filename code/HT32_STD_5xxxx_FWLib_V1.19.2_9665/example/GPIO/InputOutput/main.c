/*********************************************************************************************************//**
 * @file    GPIO/InputOutput/main.c
 * @version $Rev:: 5675         $
 * @date    $Date:: 2021-12-23 #$
 * @brief   Main program.
 *************************************************************************************************************
 * @attention
 *
 * Firmware Disclaimer Information
 *
 * 1. The customer hereby acknowledges and agrees that the program technical documentation, including the
 *    code, which is supplied by Holtek Semiconductor Inc., (hereinafter referred to as "HOLTEK") is the
 *    proprietary and confidential intellectual property of HOLTEK, and is protected by copyright law and
 *    other intellectual property laws.
 *
 * 2. The customer hereby acknowledges and agrees that the program technical documentation, including the
 *    code, is confidential information belonging to HOLTEK, and must not be disclosed to any third parties
 *    other than HOLTEK and the customer.
 *
 * 3. The program technical documentation, including the code, is provided "as is" and for customer reference
 *    only. After delivery by HOLTEK, the customer shall use the program technical documentation, including
 *    the code, at their own risk. HOLTEK disclaims any expressed, implied or statutory warranties, including
 *    the warranties of merchantability, satisfactory quality and fitness for a particular purpose.
 *
 * <h2><center>Copyright (C) Holtek Semiconductor Inc. All rights reserved</center></h2>
 ************************************************************************************************************/

/* Includes ------------------------------------------------------------------------------------------------*/
#include "ht32.h"
#include "ht32_board.h"
#include "ht32_board_config.h"

/* Private function prototypes -----------------------------------------------------------------------------*/
void CKCU_Configuration(void);
void GPIO_OUT_Configuration(void);

/* Global functions ----------------------------------------------------------------------------------------*/
int main(void)
{
  CKCU_Configuration();

  GPIO_OUT_Configuration();

  while (1)
  {
    /* D0 (PA10) */
    GPIO_WriteOutBits(HT_GPIOA, GPIO_PIN_10, SET);
    GPIO_WriteOutBits(HT_GPIOA, GPIO_PIN_10, RESET);

    /* D1 (PA8) */
    GPIO_WriteOutBits(HT_GPIOA, GPIO_PIN_8, SET);
    GPIO_WriteOutBits(HT_GPIOA, GPIO_PIN_8, RESET);

    /* D2 (PB0) */
    GPIO_WriteOutBits(HT_GPIOB, GPIO_PIN_0, SET);
    GPIO_WriteOutBits(HT_GPIOB, GPIO_PIN_0, RESET);

    /* D3 (PB1) */
    GPIO_WriteOutBits(HT_GPIOB, GPIO_PIN_1, SET);
    GPIO_WriteOutBits(HT_GPIOB, GPIO_PIN_1, RESET);

    /* D4 (PB2) */
    GPIO_WriteOutBits(HT_GPIOB, GPIO_PIN_2, SET);
    GPIO_WriteOutBits(HT_GPIOB, GPIO_PIN_2, RESET);

    /* D5 (PB3) */
    GPIO_WriteOutBits(HT_GPIOB, GPIO_PIN_3, SET);
    GPIO_WriteOutBits(HT_GPIOB, GPIO_PIN_3, RESET);

    /* D6 (PB4) */
    GPIO_WriteOutBits(HT_GPIOB, GPIO_PIN_4, SET);
    GPIO_WriteOutBits(HT_GPIOB, GPIO_PIN_4, RESET);

    /* D7 (PB5) */
    GPIO_WriteOutBits(HT_GPIOB, GPIO_PIN_5, SET);
    GPIO_WriteOutBits(HT_GPIOB, GPIO_PIN_5, RESET);

    /* D8 (PA14) */
    GPIO_WriteOutBits(HT_GPIOA, GPIO_PIN_14, SET);
    GPIO_WriteOutBits(HT_GPIOA, GPIO_PIN_14, RESET);

    /* D9 (PA15) */
    GPIO_WriteOutBits(HT_GPIOA, GPIO_PIN_15, SET);
    GPIO_WriteOutBits(HT_GPIOA, GPIO_PIN_15, RESET);

    /* D10 (PC10) */
    GPIO_WriteOutBits(HT_GPIOC, GPIO_PIN_10, SET);
    GPIO_WriteOutBits(HT_GPIOC, GPIO_PIN_10, RESET);

    /* D11 (PC8) */
    GPIO_WriteOutBits(HT_GPIOC, GPIO_PIN_8, SET);
    GPIO_WriteOutBits(HT_GPIOC, GPIO_PIN_8, RESET);

    /* D12 (PC9) */
    GPIO_WriteOutBits(HT_GPIOC, GPIO_PIN_9, SET);
    GPIO_WriteOutBits(HT_GPIOC, GPIO_PIN_9, RESET);

    /* D13 (PC11) */
    GPIO_WriteOutBits(HT_GPIOC, GPIO_PIN_11, SET);
    GPIO_WriteOutBits(HT_GPIOC, GPIO_PIN_11, RESET);

    /* A0 / D14 (PC1) */
    GPIO_WriteOutBits(HT_GPIOC, GPIO_PIN_1, SET);
    GPIO_WriteOutBits(HT_GPIOC, GPIO_PIN_1, RESET);

    /* A1 / D15 (PC3) */
    GPIO_WriteOutBits(HT_GPIOC, GPIO_PIN_3, SET);
    GPIO_WriteOutBits(HT_GPIOC, GPIO_PIN_3, RESET);

    /* A2 / D16 (PD4) */
    GPIO_WriteOutBits(HT_GPIOD, GPIO_PIN_4, SET);
    GPIO_WriteOutBits(HT_GPIOD, GPIO_PIN_4, RESET);

    /* A3 / D17 (PC2) */
    GPIO_WriteOutBits(HT_GPIOC, GPIO_PIN_2, SET);
    GPIO_WriteOutBits(HT_GPIOC, GPIO_PIN_2, RESET);

    /* A4 / D18 (PC5) */
    GPIO_WriteOutBits(HT_GPIOC, GPIO_PIN_5, SET);
    GPIO_WriteOutBits(HT_GPIOC, GPIO_PIN_5, RESET);

    /* A5 / D19 (PC4) */
    GPIO_WriteOutBits(HT_GPIOC, GPIO_PIN_4, SET);
    GPIO_WriteOutBits(HT_GPIOC, GPIO_PIN_4, RESET);
  }
}

void CKCU_Configuration(void)
{
  CKCU_PeripClockConfig_TypeDef CKCUClock = {{0}};

  CKCUClock.Bit.PA         = 1;
  CKCUClock.Bit.PB         = 1;
  CKCUClock.Bit.PC         = 1;
  CKCUClock.Bit.PD         = 1;
  CKCUClock.Bit.AFIO       = 1;
  CKCU_PeripClockConfig(CKCUClock, ENABLE);
}

void GPIO_OUT_Configuration(void)
{
  /* Port A: PA8(D1), PA10(D0), PA14(D8), PA15(D9) */
  AFIO_GPxConfig(GPIO_PA, AFIO_PIN_8 | AFIO_PIN_10 | AFIO_PIN_14 | AFIO_PIN_15, AFIO_FUN_GPIO);
  GPIO_DirectionConfig(HT_GPIOA, GPIO_PIN_8 | GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15, GPIO_DIR_OUT);

  /* Port B: PB0(D2), PB1(D3), PB2(D4), PB3(D5), PB4(D6), PB5(D7) */
  AFIO_GPxConfig(GPIO_PB, AFIO_PIN_0 | AFIO_PIN_1 | AFIO_PIN_2 | AFIO_PIN_3 | AFIO_PIN_4 | AFIO_PIN_5, AFIO_FUN_GPIO);
  GPIO_DirectionConfig(HT_GPIOB, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5, GPIO_DIR_OUT);

  /* Port C: PC1(A0), PC2(A3), PC3(A1), PC4(A5), PC5(A4), PC8(D11), PC9(D12), PC10(D10), PC11(D13) */
  AFIO_GPxConfig(GPIO_PC, AFIO_PIN_1 | AFIO_PIN_2 | AFIO_PIN_3 | AFIO_PIN_4 | AFIO_PIN_5 | AFIO_PIN_8 | AFIO_PIN_9 | AFIO_PIN_10 | AFIO_PIN_11, AFIO_FUN_GPIO);
  GPIO_DirectionConfig(HT_GPIOC, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11, GPIO_DIR_OUT);

  /* Port D: PD4(A2) */
  AFIO_GPxConfig(GPIO_PD, AFIO_PIN_4, AFIO_FUN_GPIO);
  GPIO_DirectionConfig(HT_GPIOD, GPIO_PIN_4, GPIO_DIR_OUT);
}

#if (HT32_LIB_DEBUG == 1)
void assert_error(u8* filename, u32 uline)
{
  while (1)
  {
  }
}
#endif
