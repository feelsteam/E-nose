#include "ht32.h"
#include "ht32_board.h"
#include <stdio.h>

/* ==========================================================================
 * OLED (SPI) 定義
 * ==========================================================================*/
#define OLED_DC_PORT    HT_GPIOA
#define OLED_DC_PIN     GPIO_PIN_15
#define OLED_RES_PORT   HT_GPIOA
#define OLED_RES_PIN    GPIO_PIN_14
#define OLED_SPI_PORT   HT_SPI1

#define OLED_DC_CMD()   GPIO_WriteOutBits(OLED_DC_PORT,  OLED_DC_PIN,  RESET)
#define OLED_DC_DAT()   GPIO_WriteOutBits(OLED_DC_PORT,  OLED_DC_PIN,  SET)
#define OLED_RES_L()    GPIO_WriteOutBits(OLED_RES_PORT, OLED_RES_PIN, RESET)
#define OLED_RES_H()    GPIO_WriteOutBits(OLED_RES_PORT, OLED_RES_PIN, SET)

#define OLED_WIDTH      128
#define OLED_PAGES      8

static const u8 Font8x8[][8] = {
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 0x20   */
  {0x00,0x00,0x5F,0x00,0x00,0x00,0x00,0x00}, /* 0x21 ! */
  {0x00,0x07,0x00,0x07,0x00,0x00,0x00,0x00}, /* 0x22 " */
  {0x14,0x7F,0x14,0x7F,0x14,0x00,0x00,0x00}, /* 0x23 # */
  {0x24,0x2A,0x7F,0x2A,0x12,0x00,0x00,0x00}, /* 0x24 $ */
  {0x23,0x13,0x08,0x64,0x62,0x00,0x00,0x00}, /* 0x25 % */
  {0x36,0x49,0x55,0x22,0x50,0x00,0x00,0x00}, /* 0x26 & */
  {0x00,0x05,0x03,0x00,0x00,0x00,0x00,0x00}, /* 0x27 ' */
  {0x00,0x1C,0x22,0x41,0x00,0x00,0x00,0x00}, /* 0x28 ( */
  {0x00,0x41,0x22,0x1C,0x00,0x00,0x00,0x00}, /* 0x29 ) */
  {0x08,0x2A,0x1C,0x2A,0x08,0x00,0x00,0x00}, /* 0x2A * */
  {0x08,0x08,0x3E,0x08,0x08,0x00,0x00,0x00}, /* 0x2B + */
  {0x00,0x50,0x30,0x00,0x00,0x00,0x00,0x00}, /* 0x2C , */
  {0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x00}, /* 0x2D - */
  {0x00,0x60,0x60,0x00,0x00,0x00,0x00,0x00}, /* 0x2E . */
  {0x20,0x10,0x08,0x04,0x02,0x00,0x00,0x00}, /* 0x2F / */
  {0x3E,0x51,0x49,0x45,0x3E,0x00,0x00,0x00}, /* 0x30 0 */
  {0x00,0x42,0x7F,0x40,0x00,0x00,0x00,0x00}, /* 0x31 1 */
  {0x42,0x61,0x51,0x49,0x46,0x00,0x00,0x00}, /* 0x32 2 */
  {0x21,0x41,0x45,0x4B,0x31,0x00,0x00,0x00}, /* 0x33 3 */
  {0x18,0x14,0x12,0x7F,0x10,0x00,0x00,0x00}, /* 0x34 4 */
  {0x27,0x45,0x45,0x45,0x39,0x00,0x00,0x00}, /* 0x35 5 */
  {0x3C,0x4A,0x49,0x49,0x30,0x00,0x00,0x00}, /* 0x36 6 */
  {0x01,0x71,0x09,0x05,0x03,0x00,0x00,0x00}, /* 0x37 7 */
  {0x36,0x49,0x49,0x49,0x36,0x00,0x00,0x00}, /* 0x38 8 */
  {0x06,0x49,0x49,0x29,0x1E,0x00,0x00,0x00}, /* 0x39 9 */
  {0x00,0x36,0x36,0x00,0x00,0x00,0x00,0x00}, /* 0x3A : */
  {0x00,0x56,0x36,0x00,0x00,0x00,0x00,0x00}, /* 0x3B ; */
  {0x00,0x08,0x14,0x22,0x41,0x00,0x00,0x00}, /* 0x3C < */
  {0x14,0x14,0x14,0x14,0x14,0x00,0x00,0x00}, /* 0x3D = */
  {0x41,0x22,0x14,0x08,0x00,0x00,0x00,0x00}, /* 0x3E > */
  {0x02,0x01,0x51,0x09,0x06,0x00,0x00,0x00}, /* 0x3F ? */
  {0x32,0x49,0x79,0x41,0x3E,0x00,0x00,0x00}, /* 0x40 @ */
  {0x7E,0x11,0x11,0x11,0x7E,0x00,0x00,0x00}, /* 0x41 A */
  {0x7F,0x49,0x49,0x49,0x36,0x00,0x00,0x00}, /* 0x42 B */
  {0x3E,0x41,0x41,0x41,0x22,0x00,0x00,0x00}, /* 0x43 C */
  {0x7F,0x41,0x41,0x22,0x1C,0x00,0x00,0x00}, /* 0x44 D */
  {0x7F,0x49,0x49,0x49,0x41,0x00,0x00,0x00}, /* 0x45 E */
  {0x7F,0x09,0x09,0x01,0x01,0x00,0x00,0x00}, /* 0x46 F */
  {0x3E,0x41,0x41,0x51,0x32,0x00,0x00,0x00}, /* 0x47 G */
  {0x7F,0x08,0x08,0x08,0x7F,0x00,0x00,0x00}, /* 0x48 H */
  {0x00,0x41,0x7F,0x41,0x00,0x00,0x00,0x00}, /* 0x49 I */
  {0x20,0x40,0x41,0x3F,0x01,0x00,0x00,0x00}, /* 0x4A J */
  {0x7F,0x08,0x14,0x22,0x41,0x00,0x00,0x00}, /* 0x4B K */
  {0x7F,0x40,0x40,0x40,0x40,0x00,0x00,0x00}, /* 0x4C L */
  {0x7F,0x02,0x04,0x02,0x7F,0x00,0x00,0x00}, /* 0x4D M */
  {0x7F,0x04,0x08,0x10,0x7F,0x00,0x00,0x00}, /* 0x4E N */
  {0x3E,0x41,0x41,0x41,0x3E,0x00,0x00,0x00}, /* 0x4F O */
  {0x7F,0x09,0x09,0x09,0x06,0x00,0x00,0x00}, /* 0x50 P */
  {0x3E,0x41,0x51,0x21,0x5E,0x00,0x00,0x00}, /* 0x51 Q */
  {0x7F,0x09,0x19,0x29,0x46,0x00,0x00,0x00}, /* 0x52 R */
  {0x46,0x49,0x49,0x49,0x31,0x00,0x00,0x00}, /* 0x53 S */
  {0x01,0x01,0x7F,0x01,0x01,0x00,0x00,0x00}, /* 0x54 T */
  {0x3F,0x40,0x40,0x40,0x3F,0x00,0x00,0x00}, /* 0x55 U */
  {0x1F,0x20,0x40,0x20,0x1F,0x00,0x00,0x00}, /* 0x56 V */
  {0x7F,0x20,0x18,0x20,0x7F,0x00,0x00,0x00}, /* 0x57 W */
  {0x63,0x14,0x08,0x14,0x63,0x00,0x00,0x00}, /* 0x58 X */
  {0x03,0x04,0x78,0x04,0x03,0x00,0x00,0x00}, /* 0x59 Y */
  {0x61,0x51,0x49,0x45,0x43,0x00,0x00,0x00}, /* 0x5A Z */
  {0x00,0x00,0x7F,0x41,0x41,0x00,0x00,0x00}, /* 0x5B [ */
  {0x02,0x04,0x08,0x10,0x20,0x00,0x00,0x00}, /* 0x5C \ */
  {0x41,0x41,0x7F,0x00,0x00,0x00,0x00,0x00}, /* 0x5D ] */
  {0x04,0x02,0x01,0x02,0x04,0x00,0x00,0x00}, /* 0x5E ^ */
  {0x40,0x40,0x40,0x40,0x40,0x00,0x00,0x00}, /* 0x5F _ */
  {0x00,0x01,0x02,0x04,0x00,0x00,0x00,0x00}, /* 0x60 ` */
  {0x20,0x54,0x54,0x54,0x78,0x00,0x00,0x00}, /* 0x61 a */
  {0x7F,0x48,0x44,0x44,0x38,0x00,0x00,0x00}, /* 0x62 b */
  {0x38,0x44,0x44,0x44,0x20,0x00,0x00,0x00}, /* 0x63 c */
  {0x38,0x44,0x44,0x48,0x7F,0x00,0x00,0x00}, /* 0x64 d */
  {0x38,0x54,0x54,0x54,0x18,0x00,0x00,0x00}, /* 0x65 e */
  {0x08,0x7E,0x09,0x01,0x02,0x00,0x00,0x00}, /* 0x66 f */
  {0x08,0x14,0x54,0x54,0x3C,0x00,0x00,0x00}, /* 0x67 g */
  {0x7F,0x08,0x04,0x04,0x78,0x00,0x00,0x00}, /* 0x68 h */
  {0x00,0x44,0x7D,0x40,0x00,0x00,0x00,0x00}, /* 0x69 i */
  {0x20,0x40,0x44,0x3D,0x00,0x00,0x00,0x00}, /* 0x6A j */
  {0x00,0x7F,0x10,0x28,0x44,0x00,0x00,0x00}, /* 0x6B k */
  {0x00,0x41,0x7F,0x40,0x00,0x00,0x00,0x00}, /* 0x6C l */
  {0x7C,0x04,0x18,0x04,0x78,0x00,0x00,0x00}, /* 0x6D m */
  {0x7C,0x08,0x04,0x04,0x78,0x00,0x00,0x00}, /* 0x6E n */
  {0x38,0x44,0x44,0x44,0x38,0x00,0x00,0x00}, /* 0x6F o */
  {0x7C,0x14,0x14,0x14,0x08,0x00,0x00,0x00}, /* 0x70 p */
  {0x08,0x14,0x14,0x18,0x7C,0x00,0x00,0x00}, /* 0x71 q */
  {0x7C,0x08,0x04,0x04,0x08,0x00,0x00,0x00}, /* 0x72 r */
  {0x48,0x54,0x54,0x54,0x20,0x00,0x00,0x00}, /* 0x73 s */
  {0x04,0x3F,0x44,0x40,0x20,0x00,0x00,0x00}, /* 0x74 t */
  {0x3C,0x40,0x40,0x20,0x7C,0x00,0x00,0x00}, /* 0x75 u */
  {0x1C,0x20,0x40,0x20,0x1C,0x00,0x00,0x00}, /* 0x76 v */
  {0x3C,0x40,0x30,0x40,0x3C,0x00,0x00,0x00}, /* 0x77 w */
  {0x44,0x28,0x10,0x28,0x44,0x00,0x00,0x00}, /* 0x78 x */
  {0x0C,0x50,0x50,0x50,0x3C,0x00,0x00,0x00}, /* 0x79 y */
  {0x44,0x64,0x54,0x4C,0x44,0x00,0x00,0x00}, /* 0x7A z */
  {0x00,0x08,0x36,0x41,0x00,0x00,0x00,0x00}, /* 0x7B { */
  {0x00,0x00,0x7F,0x00,0x00,0x00,0x00,0x00}, /* 0x7C | */
  {0x00,0x41,0x36,0x08,0x00,0x00,0x00,0x00}, /* 0x7D } */
  {0x08,0x04,0x08,0x10,0x08,0x00,0x00,0x00}, /* 0x7E ~ */
};

/* ==========================================================================
 * ADS1115 (I2C) 定義
 * ==========================================================================*/
#define ADS1115_I2C_PORT (HT_I2C1)
#define ADS1115_ADDR 0x48
#define I2C_CLOCK_SPEED 100000

/* ==========================================================================
 * 全域變數
 * ==========================================================================*/
vu32 gADC_Result[3];
float gVoltage[3];

/* ==========================================================================
 * 函數原型
 * ==========================================================================*/
void System_Configuration(void);
void Delay_ms(vu32 ms);

void SPI_PollSendByte(u8 data);
void OLED_WriteCmd(u8 cmd);
void OLED_WriteData(u8 data);
void OLED_Init(void);
void OLED_Clear(void);
void OLED_Fill(u8 val);
void OLED_SetCursor(u8 page, u8 col);
void OLED_DrawChar(u8 page, u8 col, char ch);
void OLED_DrawString(u8 page, u8 col, const char *str);

float ADS1115_ReadVoltage_A0(void);

/*********************************************************************************************************//**
  * @brief  主程式 Main
  ***********************************************************************************************************/
int main(void)
{
  char oled_buf[32];

  /* 初始化硬體 */
  System_Configuration();
  
  /* 初始化 UART 提供 printf */
  RETARGET_Configuration();

  printf("\r\n======= HT32 Sensor Integration =======\r\n");
  
  /* 初始化 OLED 螢幕 */
  OLED_Init();
  OLED_Clear();
  
  OLED_DrawString(0, 0, "HT32 System Init...");
  Delay_ms(1000);
  OLED_Clear();

  while (1)
  {
    /* 1. 觸發 ADC 轉換 (PA2, PA3, PD4) */
    ADC_SoftwareStartConvCmd(HT_ADC0, ENABLE);
    
    /* 等待三通道轉換完成 */
    while (ADC_GetFlagStatus(HT_ADC0, ADC_FLAG_CYCLE_EOC) == RESET);
    ADC_ClearIntPendingBit(HT_ADC0, ADC_FLAG_CYCLE_EOC | ADC_FLAG_SINGLE_EOC);
    
    /* 讀取內建 ADC 暫存器結果 */
    gADC_Result[0] = ADC_GetConversionData(HT_ADC0, ADC_REGULAR_DATA0);
    gADC_Result[1] = ADC_GetConversionData(HT_ADC0, ADC_REGULAR_DATA1);
    gADC_Result[2] = ADC_GetConversionData(HT_ADC0, ADC_REGULAR_DATA2);
    
    /* 計算 0~3.3V */
    gVoltage[0] = ((float)gADC_Result[0] * 3.3f) / 4095.0f; /* PA2 */
    gVoltage[1] = ((float)gADC_Result[1] * 3.3f) / 4095.0f; /* PA3 */
    gVoltage[2] = ((float)gADC_Result[2] * 3.3f) / 4095.0f; /* PD4 */

    /* 2. 讀取 ADS1115 電壓 */
    float ads_volt = ADS1115_ReadVoltage_A0();

    /* 3. 輸出到 UART (電腦端觀察) */
    printf("ADS: %.4f V | PA2: %.2fV | PA3: %.2fV | PD4: %.2fV\r\n",
           ads_volt, gVoltage[0], gVoltage[1], gVoltage[2]);

    /* 4. 輸出到 OLED (LCD端觀看) */
    sprintf(oled_buf, "ADS : %.2f V", (double)ads_volt);
    OLED_DrawString(0, 0, oled_buf);
    
    sprintf(oled_buf, "PA2 : %.2f V", (double)gVoltage[0]);
    OLED_DrawString(2, 0, oled_buf);
    
    sprintf(oled_buf, "PA3 : %.2f V", (double)gVoltage[1]);
    OLED_DrawString(4, 0, oled_buf);

    sprintf(oled_buf, "PD4 : %.2f V", (double)gVoltage[2]);
    OLED_DrawString(6, 0, oled_buf);

    /* 延遲 500ms */
    Delay_ms(500);
  }
}

/*********************************************************************************************************//**
  * @brief  系統腳位與周邊配置
  ***********************************************************************************************************/
void System_Configuration(void)
{
  CKCU_PeripClockConfig_TypeDef CKCUClock = {{0}};
  SPI_InitTypeDef SPI_InitStructure;
  I2C_InitTypeDef I2C_InitStructure;

  /* ========================================================================
     1. 開啟時鐘 (PA, PC, PD, AFIO, SPI1, I2C1, ADC0)
     ======================================================================== */
  CKCUClock.Bit.PA   = 1;
  CKCUClock.Bit.PC   = 1;
  CKCUClock.Bit.PD   = 1;
  CKCUClock.Bit.AFIO = 1;
  CKCUClock.Bit.SPI1 = 1;
  CKCUClock.Bit.I2C1 = 1;
  CKCUClock.Bit.ADC0 = 1;
  CKCU_PeripClockConfig(CKCUClock, ENABLE);

  /* ========================================================================
     2. OLED: SPI1 及 GPIO 初始化
     ======================================================================== */
  AFIO_GPxConfig(GPIO_PC, AFIO_PIN_11, AFIO_FUN_SPI); /* SCK  D13 */
  AFIO_GPxConfig(GPIO_PC, AFIO_PIN_8,  AFIO_FUN_SPI); /* MOSI D11 */
  AFIO_GPxConfig(GPIO_PC, AFIO_PIN_9,  AFIO_FUN_SPI); /* MISO D12 */
  AFIO_GPxConfig(GPIO_PC, AFIO_PIN_10, AFIO_FUN_SPI); /* SS   D10 */
  GPIO_PullResistorConfig(HT_GPIOC, GPIO_PIN_9, GPIO_PR_UP); /* MISO */
  
  /* DC(PA15), RES(PA14) -> GPIO 輸出 */
  AFIO_GPxConfig(GPIO_PA, AFIO_PIN_15, AFIO_FUN_GPIO);
  AFIO_GPxConfig(GPIO_PA, AFIO_PIN_14, AFIO_FUN_GPIO);
  GPIO_DirectionConfig(OLED_DC_PORT,  OLED_DC_PIN,  GPIO_DIR_OUT);
  GPIO_DirectionConfig(OLED_RES_PORT, OLED_RES_PIN, GPIO_DIR_OUT);
  OLED_DC_DAT();
  OLED_RES_H();

  /* 初始化 SPI1 */
  SPI_InitStructure.SPI_Mode               = SPI_MASTER;
  SPI_InitStructure.SPI_FIFO               = SPI_FIFO_DISABLE;
  SPI_InitStructure.SPI_DataLength         = SPI_DATALENGTH_8;
  SPI_InitStructure.SPI_SELMode            = SPI_SEL_HARDWARE;
  SPI_InitStructure.SPI_SELPolarity        = SPI_SELPOLARITY_LOW;
  SPI_InitStructure.SPI_CPOL             = SPI_CPOL_LOW;
  SPI_InitStructure.SPI_CPHA             = SPI_CPHA_FIRST;
  SPI_InitStructure.SPI_FirstBit         = SPI_FIRSTBIT_MSB;
  SPI_InitStructure.SPI_RxFIFOTriggerLevel = 0;
  SPI_InitStructure.SPI_TxFIFOTriggerLevel = 0;
  SPI_InitStructure.SPI_ClockPrescaler   = 8;  /* 60M/8 = 7.5MHz */
  SPI_Init(OLED_SPI_PORT, &SPI_InitStructure);

  SPI_SELOutputCmd(OLED_SPI_PORT, ENABLE);
  SPI_Cmd(OLED_SPI_PORT, ENABLE);

  /* ========================================================================
     3. ADS1115: I2C1 初始化 (PC4, PC5)
     ======================================================================== */
  AFIO_GPxConfig(GPIO_PC, AFIO_PIN_4, AFIO_FUN_I2C); /* SCL */
  AFIO_GPxConfig(GPIO_PC, AFIO_PIN_5, AFIO_FUN_I2C); /* SDA */
  GPIO_PullResistorConfig(HT_GPIOC, GPIO_PIN_4, GPIO_PR_UP); /* 強制開啟上拉 */
  GPIO_PullResistorConfig(HT_GPIOC, GPIO_PIN_5, GPIO_PR_UP);

  I2C_InitStructure.I2C_GeneralCall      = DISABLE;
  I2C_InitStructure.I2C_AddressingMode   = I2C_ADDRESSING_7BIT;
  I2C_InitStructure.I2C_Acknowledge      = DISABLE;
  I2C_InitStructure.I2C_OwnAddress       = 0x0A;
  I2C_InitStructure.I2C_Speed            = I2C_CLOCK_SPEED;
  I2C_InitStructure.I2C_SpeedOffset      = 0;
  I2C_Init(ADS1115_I2C_PORT, &I2C_InitStructure);
  I2C_Cmd(ADS1115_I2C_PORT, ENABLE);

  /* ========================================================================
     4. 內建 ADC0 初始化 (PA2, PA3, PD4)
     ======================================================================== */
  /* 將腳位轉為 ADC 複用功能，並關閉 GPIO 數位輸入防止漏電 */
  AFIO_GPxConfig(GPIO_PA, AFIO_PIN_2 | AFIO_PIN_3, AFIO_FUN_ADC);
  AFIO_GPxConfig(GPIO_PD, AFIO_PIN_4, AFIO_FUN_ADC);
  GPIO_InputConfig(HT_GPIOA, GPIO_PIN_2 | GPIO_PIN_3, DISABLE);
  GPIO_InputConfig(HT_GPIOD, GPIO_PIN_4, DISABLE);

  CKCU_SetADCnPrescaler(CKCU_ADCPRE_ADC0, CKCU_ADCPRE_DIV64);

  /* One Shot 模式，序列長度為 3 */
  ADC_RegularGroupConfig(HT_ADC0, ONE_SHOT_MODE, 3, 0);
  ADC_RegularChannelConfig(HT_ADC0, ADC_CH_2, 0, 0); /* Rank 0 -> PA2 */
  ADC_RegularChannelConfig(HT_ADC0, ADC_CH_3, 1, 0); /* Rank 1 -> PA3 */
  ADC_RegularChannelConfig(HT_ADC0, ADC_CH_8, 2, 0); /* Rank 2 -> PD4 */
  
  ADC_RegularTrigConfig(HT_ADC0, ADC_TRIG_SOFTWARE);
  ADC_Cmd(HT_ADC0, ENABLE);
}

/*********************************************************************************************************//**
  * @brief  ADS1115 讀取 A0
  ***********************************************************************************************************/
float ADS1115_ReadVoltage_A0(void) {
  u8 config_msb = 0xC3; /* 開始單次轉換, AIN0, +/- 4.096V, 單次模式 */
  u8 config_lsb = 0x83; /* 128SPS, 傳統比較器, 負極性, 非鎖存, 停用比較器 */
  
  u8 data_msb = 0, data_lsb = 0;
  int16_t raw_adc = 0;

  /* 寫入 Config Register (0x01) */
  I2C_TargetAddressConfig(ADS1115_I2C_PORT, ADS1115_ADDR, I2C_MASTER_WRITE);
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_SEND_START));
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_TRANSMITTER_MODE));

  I2C_SendData(ADS1115_I2C_PORT, 0x01);
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_TX_EMPTY));

  I2C_SendData(ADS1115_I2C_PORT, config_msb);
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_TX_EMPTY));

  I2C_SendData(ADS1115_I2C_PORT, config_lsb);
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_TX_EMPTY));

  I2C_GenerateSTOP(ADS1115_I2C_PORT);
  while (I2C_ReadRegister(ADS1115_I2C_PORT, I2C_REGISTER_SR) & 0x80000);

  /* 128 SPS 轉換大約需要 8ms */
  Delay_ms(10);

  /* 將指標指向 Conversion Register (0x00) */
  I2C_TargetAddressConfig(ADS1115_I2C_PORT, ADS1115_ADDR, I2C_MASTER_WRITE);
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_SEND_START));
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_TRANSMITTER_MODE));

  I2C_SendData(ADS1115_I2C_PORT, 0x00);
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_TX_EMPTY));

  I2C_GenerateSTOP(ADS1115_I2C_PORT);
  while (I2C_ReadRegister(ADS1115_I2C_PORT, I2C_REGISTER_SR) & 0x80000);

  /* 讀取 2 Bytes 的數值 */
  I2C_TargetAddressConfig(ADS1115_I2C_PORT, ADS1115_ADDR, I2C_MASTER_READ);
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_SEND_START));
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_RECEIVER_MODE));

  I2C_AckCmd(ADS1115_I2C_PORT, ENABLE);
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_RX_NOT_EMPTY));
  data_msb = I2C_ReceiveData(ADS1115_I2C_PORT);

  I2C_AckCmd(ADS1115_I2C_PORT, DISABLE);
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_RX_NOT_EMPTY));
  data_lsb = I2C_ReceiveData(ADS1115_I2C_PORT);

  I2C_GenerateSTOP(ADS1115_I2C_PORT);
  while (I2C_ReadRegister(ADS1115_I2C_PORT, I2C_REGISTER_SR) & 0x80000);

  raw_adc = (int16_t)((data_msb << 8) | data_lsb);
  return (float)raw_adc * (4.096f / 32768.0f);
}

/*********************************************************************************************************//**
  * @brief  延遲 MS Function
  ***********************************************************************************************************/
void Delay_ms(vu32 ms)
{
  volatile u32 i;
  while (ms--) {
    /* 簡易迴圈延遲，配合 HT32 時脈 (約 60MHz) */
    for (i = 0; i < 12000; i++);
  }
}

/*********************************************************************************************************//**
  * @brief  OLED 驅動控制區
  ***********************************************************************************************************/
void SPI_PollSendByte(u8 data)
{
  while (!SPI_GetFlagStatus(OLED_SPI_PORT, SPI_FLAG_TXBE));
  SPI_SendData(OLED_SPI_PORT, data);
  while (!SPI_GetFlagStatus(OLED_SPI_PORT, SPI_FLAG_RXBNE));
  SPI_ReceiveData(OLED_SPI_PORT);
}

void OLED_WriteCmd(u8 cmd)
{
  OLED_DC_CMD();
  SPI_PollSendByte(cmd);
}

void OLED_WriteData(u8 data)
{
  OLED_DC_DAT();
  SPI_PollSendByte(data);
}

void OLED_Init(void)
{
  OLED_RES_H(); Delay_ms(1);
  OLED_RES_L(); Delay_ms(1);
  OLED_RES_H(); Delay_ms(1);

  OLED_WriteCmd(0xAE); /* Sleep mode */
  OLED_WriteCmd(0x20); /* Addressing Mode */
  OLED_WriteCmd(0x00); /* Horizontal */
  OLED_WriteCmd(0x81); /* Contrast */
  OLED_WriteCmd(0x7F); 
  OLED_WriteCmd(0xA6); /* Normal Display */
  OLED_WriteCmd(0xAF); /* Display ON */

  OLED_WriteCmd(0x21); OLED_WriteCmd(0x00); OLED_WriteCmd(0x7F);
  OLED_WriteCmd(0x22); OLED_WriteCmd(0x00); OLED_WriteCmd(0x07);
}

void OLED_SetCursor(u8 page, u8 col)
{
  OLED_WriteCmd(0x21); OLED_WriteCmd(col);  OLED_WriteCmd(0x7F);
  OLED_WriteCmd(0x22); OLED_WriteCmd(page); OLED_WriteCmd(0x07);
}

void OLED_Fill(u8 val)
{
  u16 i;
  OLED_WriteCmd(0x21); OLED_WriteCmd(0x00); OLED_WriteCmd(0x7F);
  OLED_WriteCmd(0x22); OLED_WriteCmd(0x00); OLED_WriteCmd(0x07);
  for (i = 0; i < 1024; i++) {
    OLED_WriteData(val);
  }
}

void OLED_Clear(void) { 
  OLED_Fill(0x00); 
}

void OLED_DrawChar(u8 page, u8 col, char ch)
{
  u8 i;
  if (ch < 0x20 || ch > 0x7E) ch = ' ';
  OLED_SetCursor(page, col);
  for (i = 0; i < 8; i++) {
    OLED_WriteData(Font8x8[ch - 0x20][i]);
  }
}

void OLED_DrawString(u8 page, u8 col, const char *str)
{
  while (*str) {
    if (col + 8 > OLED_WIDTH) break;
    OLED_DrawChar(page, col, *str++);
    col += 8;
  }
}

#if (HT32_LIB_DEBUG == 1)
void assert_error(u8* filename, u32 uline) {
  while (1) {}
}
#endif
