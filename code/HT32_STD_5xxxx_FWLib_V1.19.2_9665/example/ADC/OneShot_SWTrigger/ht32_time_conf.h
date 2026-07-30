/*********************************************************************************************************//**
 * @file    ADC/OneShot_SWTrigger/ht32_time_conf.h
 * @brief   The configuration of HT32 Time function (BFTM0, used for 500ms delay).
 ************************************************************************************************************/
#ifndef __HT32_TIME_CONF_H
#define __HT32_TIME_CONF_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Settings ------------------------------------------------------------------------------------------------*/
/* Use BFTM0 as time base for Time_Init() / Time_Delay() / TIME_MS2TICK()                                  */
#if (1)
#define HTCFG_TIME_IPSEL        (0)         /* 0 = BFTM0                                                   */
#define HTCFG_TIME_CLKSEL       (0)         /* 0 = Default Maximum (LIBCFG_MAX_SPEED = 60MHz for F52367)   */
#define HTCFG_TIME_CLK_MANUAL   (20000000)  /* Only used when HTCFG_TIME_CLKSEL = 1                        */
#define HTCFG_TIME_PCLK_DIV     (0)         /* APB prescaler: 0 = /1                                       */
#define HTCFG_TIME_TICKHZ       (1000)      /* Not used for BFTM (BFTM tick = raw clock count)             */
#define HTCFG_TIME_MULTIPLE     (1)         /* Not used for BFTM                                            */
#endif

#ifdef __cplusplus
}
#endif

#endif
