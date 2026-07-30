/*********************************************************************************************************//**
 * @file    main.c
 * @version $Rev:: 6            $
 * @date    $Date:: 2026-04-16  $
 * @brief   Main program.
 ************************************************************************************************************/

/* Includes ------------------------------------------------------------------------------------------------*/
#include "ht32.h"
#include "ht32_board.h"
#include "ht32_board_config.h"

/* Private variables ---------------------------------------------------------------------------------------*/
vu32 gADC_Result[3];
float gVoltage[3];

/* Private function prototypes -----------------------------------------------------------------------------*/
void ADC_Configuration(void);

/*********************************************************************************************************//**
 * @brief  Main program.
 * @retval None
 ************************************************************************************************************/
int main(void)
{
  /* Initialize the Retarget (UART) for printf */
  RETARGET_Configuration();
  
  printf("\r\n======= HT32 ADC Sequence Polling Example =======\r\n");
  printf("Reading ADC values from PA2(CH2), PA3(CH3), and PD4(CH8)\r\n");

  /* Initialize ADC and associate GPIO pins */
  ADC_Configuration();

  /* Infinite loop */
  while (1)
  {
    /* Trigger ADC Conversion for the entire sequence (Channels 2, 3, 8) */
    ADC_SoftwareStartConvCmd(HTCFG_ADC_PORT, ENABLE);

    /* Wait until all 3 conversions are fully complete (CYCLE EOC flag) */
    while (ADC_GetFlagStatus(HTCFG_ADC_PORT, ADC_FLAG_CYCLE_EOC) == RESET);
    
    /* Clear flags to be ready for the next iteration */
    ADC_ClearIntPendingBit(HTCFG_ADC_PORT, ADC_FLAG_CYCLE_EOC | ADC_FLAG_SINGLE_EOC);

    /* Read out the values from the sequence data registers */
    /* Rank 0 -> PA2 (CH2) */
    gADC_Result[0] = ADC_GetConversionData(HTCFG_ADC_PORT, ADC_REGULAR_DATA0);
    /* Rank 1 -> PA3 (CH3) */
    gADC_Result[1] = ADC_GetConversionData(HTCFG_ADC_PORT, ADC_REGULAR_DATA1);
    /* Rank 2 -> PD4 (CH8) */
    gADC_Result[2] = ADC_GetConversionData(HTCFG_ADC_PORT, ADC_REGULAR_DATA2);

    /* Calculate voltages */
    gVoltage[0] = ((float)gADC_Result[0] * 3.3f) / 4095.0f;
    gVoltage[1] = ((float)gADC_Result[1] * 3.3f) / 4095.0f;
    gVoltage[2] = ((float)gADC_Result[2] * 3.3f) / 4095.0f;

    /* Print results to terminal */
    printf("PA2 (CH2): %4u (%.2fV) | PA3 (CH3): %4u (%.2fV) | PD4 (CH8): %4u (%.2fV)\r\n", 
           (unsigned int)gADC_Result[0], gVoltage[0],
           (unsigned int)gADC_Result[1], gVoltage[1],
           (unsigned int)gADC_Result[2], gVoltage[2]);

    /* Delay */
    {
      vu32 i;
      for (i = 0; i < 6000000; i++); 
    }
  }
}

/*********************************************************************************************************//**
 * @brief  ADC_Configuration.
 *         Configure the GPIO ports, Clocks and ADC mode.
 * @retval None
 ************************************************************************************************************/
void ADC_Configuration(void)
{
  { /* Enable peripheral clocks */
    CKCU_PeripClockConfig_TypeDef CKCUClock = {{ 0 }};
    CKCUClock.Bit.AFIO = 1;
    CKCUClock.Bit.PA = 1; /* Enable Port A clock for PA2, PA3 */
    CKCUClock.Bit.PD = 1; /* Enable Port D clock for PD4 */
    CKCUClock.Bit.HTCFG_ADC_IPN = 1; 
    CKCU_PeripClockConfig(CKCUClock, ENABLE);
  }

  /* Configure AFIO mode as ADC function (AF2) for PA2 and PA3 */
  AFIO_GPxConfig(GPIO_PA, AFIO_PIN_2 | AFIO_PIN_3, AFIO_FUN_ADC);
  
  /* Configure AFIO mode as ADC function (AF2) for PD4 */
  AFIO_GPxConfig(GPIO_PD, AFIO_PIN_4, AFIO_FUN_ADC);

  /* Disable GPIO Input buffer to avoid leakage (important for analog pins) */
  GPIO_InputConfig(HT_GPIOA, GPIO_PIN_2 | GPIO_PIN_3, DISABLE);
  GPIO_InputConfig(HT_GPIOD, GPIO_PIN_4, DISABLE);

  /* Set ADC Prescaler to DIV64 to ensure stable clock */
  CKCU_SetADCnPrescaler(HTCFG_ADC_CKCU_ADCPRE, CKCU_ADCPRE_DIV64);

  /* Configure ADC: One Shot Mode, sequence length 3, sub-sequence length 0 */
  ADC_RegularGroupConfig(HTCFG_ADC_PORT, ONE_SHOT_MODE, 3, 0);

  /* Setup the conversion sequence. */
  ADC_RegularChannelConfig(HTCFG_ADC_PORT, ADC_CH_2, 0, 0); /* Rank 0 -> PA2 */
  ADC_RegularChannelConfig(HTCFG_ADC_PORT, ADC_CH_3, 1, 0); /* Rank 1 -> PA3 */
  ADC_RegularChannelConfig(HTCFG_ADC_PORT, ADC_CH_8, 2, 0); /* Rank 2 -> PD4 */

  /* Configure ADC trigger source as Software Trigger */
  ADC_RegularTrigConfig(HTCFG_ADC_PORT, ADC_TRIG_SOFTWARE);

  /* Enable the ADC! */
  ADC_Cmd(HTCFG_ADC_PORT, ENABLE);
}
