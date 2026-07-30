/************************************************************************************************************
 * @file    main.c
 * @brief   HT32F52367 (BM53A367A)
 *          ADS1115 (I2C1) 讀取電壓 + SSD1309 OLED (SPI1) 顯示
 *
 * ============================================================
 * 硬體接線：
 *   ── SPI (OLED SSD1309) ──────────────────────────────────
 *   OLED CS   -> D10 (PC10)  [SPI1 硬體 SS]
 *   OLED SCL  -> D13 (PC11)  [SPI1 SCK]
 *   OLED SDA  -> D11 (PC8)   [SPI1 MOSI]
 *   OLED RES  -> D8  (PA14)  [GPIO]
 *   OLED DC   -> D9  (PA15)  [GPIO]
 *
 *   ── I2C (ADS1115) ───────────────────────────────────────
 *   ADS SCL   -> A5  (PC5)   [I2C1 SCL]
 *   ADS SDA   -> A4  (PC4)   [I2C1 SDA]
 *   ADS ADDR  -> GND         [I2C 7-bit addr = 0x48]
 *
 * ============================================================
 * 顯示佈局 (128x64 OLED):
 *   Page 0: "=== E-NOSE ===="
 *   Page 2: "ADS1115 A0:"
 *   Page 3: "  X.XXXX V"
 *   Page 5: "Raw: XXXXX"
 *   Page 7: 更新計數器
 ***********************************************************************************************************/

#include "ht32.h"
#include "ht32_board.h"
#include <stdio.h>
#include <string.h>

/* ==========================================================================
 * 【第一區】腳位 & 周邊定義
 * ==========================================================================*/

/* ── SPI / OLED ─────────────────────────────────── */
#define OLED_DC_PORT    HT_GPIOA
#define OLED_DC_PIN     GPIO_PIN_15   /* D9  = PA15 */

#define OLED_RES_PORT   HT_GPIOA
#define OLED_RES_PIN    GPIO_PIN_14   /* D8  = PA14 */

#define OLED_SPI_PORT   HT_SPI1       /* Arduino SPI 物件 = HT_SPI1 */

/* ── I2C / ADS1115 ──────────────────────────────── */
#define ADS1115_I2C_PORT  HT_I2C1      /* BM53A367A: PC4/PC5 對應 I2C1 */
#define ADS1115_ADDR      0x48         /* ADDR 腳接 GND */
#define I2C_CLOCK_SPEED   100000       /* 100 kHz */

/* ==========================================================================
 * 【第二區】GPIO 操作巨集
 * ==========================================================================*/
#define OLED_DC_CMD()  GPIO_WriteOutBits(OLED_DC_PORT,  OLED_DC_PIN,  RESET)
#define OLED_DC_DAT()  GPIO_WriteOutBits(OLED_DC_PORT,  OLED_DC_PIN,  SET)
#define OLED_RES_L()   GPIO_WriteOutBits(OLED_RES_PORT, OLED_RES_PIN, RESET)
#define OLED_RES_H()   GPIO_WriteOutBits(OLED_RES_PORT, OLED_RES_PIN, SET)

/* ==========================================================================
 * 【第三區】OLED 尺寸常數
 * ==========================================================================*/
#define OLED_WIDTH   128
#define OLED_PAGES   8

/* ==========================================================================
 * 【第四區】8x8 字型 (ASCII 0x20 ~ 0x7E)
 * ==========================================================================*/
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
 * 【第五區】函數原型
 * ==========================================================================*/

/* ── SPI / OLED ─── */
void SPI_Init_OLED(void);
void SPI_PollSendByte(u8 data);
void OLED_WriteCmd(u8 cmd);
void OLED_WriteData(u8 data);
void OLED_Init(void);
void OLED_Clear(void);
void OLED_Fill(u8 val);
void OLED_SetCursor(u8 page, u8 col);
void OLED_DrawChar(u8 page, u8 col, char ch);
void OLED_DrawString(u8 page, u8 col, const char *str);
void OLED_DrawLine(u8 page);
void OLED_ClearLine(u8 page);

/* ── I2C / ADS1115 ─ */
void I2C_Configuration(void);
float ADS1115_ReadVoltage_A0(void);

/* ── 通用 ─────────── */
void Delay_ms(u32 ms);
void FloatToStr(float val, char *buf, u8 decimals);

/* ==========================================================================
 * 【第六區】主程式
 * ==========================================================================*/
int main(void)
{
  char      buf[32];
  u32       count = 0;
  float     voltage;
  int16_t   raw;

  /* ── 初始化 UART (printf 輸出) ── */
  RETARGET_Configuration();
  printf("\r\n*** HT32 ADS1115 + SSD1309 OLED ***\r\n");

  /* ── 初始化 SPI + OLED ────────── */
  SPI_Init_OLED();
  OLED_Init();

  /* ── 初始化 I2C + ADS1115 ──────── */
  I2C_Configuration();

  /* ── 靜態標題列 ──────────────── */
  OLED_DrawString(0, 0,  "=== E-NOSE ====");
  OLED_DrawLine(1);
  OLED_DrawString(2, 0,  "ADS1115 A0:");
  OLED_DrawLine(4);
  OLED_DrawString(5, 0,  "Raw:");
  OLED_DrawString(6, 0,  "Cnt:");

  while (1)
  {
    /* ── 讀取 ADS1115 ─────────────── */
    voltage = ADS1115_ReadVoltage_A0();
    raw     = (int16_t)(voltage / (4.096f / 32768.0f));

    /* ── 輸出至 UART ──────────────── */
    printf("[%lu] A0 = %.4f V  (raw=%d)\r\n", (unsigned long)count, voltage, raw);

    /* ── 更新 OLED Page 3: 電壓值 ── */
    OLED_ClearLine(3);
    /* 手動格式化 float (不用 sprintf 以節省 Flash) */
    FloatToStr(voltage, buf, 4);
    /* 拼接 " V" */
    {
      u8 len = (u8)strlen(buf);
      buf[len]   = ' ';
      buf[len+1] = 'V';
      buf[len+2] = '\0';
    }
    OLED_DrawString(3, 8, buf);

    /* ── 更新 OLED Page 5: Raw ───── */
    OLED_ClearLine(5);
    OLED_DrawString(5, 0, "Raw:");
    {
      /* 顯示有號整數 */
      char  sign = ' ';
      u32   absval;
      if (raw < 0) { sign = '-'; absval = (u32)(-raw); }
      else         {             absval = (u32)( raw);  }
      /* 最多 5 位 + 符號 */
      u8 pos = 5 * 8; /* 起始欄位 */
      buf[0] = sign;
      /* 簡單轉換 */
      buf[6] = '\0';
      buf[5] = '0' + (absval % 10); absval /= 10;
      buf[4] = '0' + (absval % 10); absval /= 10;
      buf[3] = '0' + (absval % 10); absval /= 10;
      buf[2] = '0' + (absval % 10); absval /= 10;
      buf[1] = '0' + (absval % 10);
      OLED_DrawString(5, pos, buf);
    }

    /* ── 更新 OLED Page 6: 計數器 ── */
    OLED_ClearLine(6);
    OLED_DrawString(6, 0, "Cnt:");
    {
      u32 c = count;
      buf[6] = '\0';
      buf[5] = '0' + (c % 10); c /= 10;
      buf[4] = '0' + (c % 10); c /= 10;
      buf[3] = '0' + (c % 10); c /= 10;
      buf[2] = '0' + (c % 10); c /= 10;
      buf[1] = '0' + (c % 10); c /= 10;
      buf[0] = '0' + (c % 10);
      OLED_DrawString(6, 5 * 8, buf);
    }

    count++;
    Delay_ms(500);   /* 約 0.5 秒更新一次 */
  }
}

/* ==========================================================================
 * 【第七區】硬體初始化 ─ SPI1 + OLED 腳位
 *
 * SPI 使用 HARDWARE SEL 模式：SPI1 自動控制 PC10 (CS)
 *   CS 拉低 → 8 SCK → CS 拉高
 * DC (PA15), RES (PA14) 仍由 GPIO 手動控制
 * ==========================================================================*/
void SPI_Init_OLED(void)
{
  SPI_InitTypeDef               SPI_InitStructure;
  CKCU_PeripClockConfig_TypeDef CKCUClock = {{0}};

  /* 開啟時鐘：SPI1, PORTA, PORTC, AFIO */
  CKCUClock.Bit.SPI1 = 1;
  CKCUClock.Bit.PA   = 1;
  CKCUClock.Bit.PC   = 1;
  CKCUClock.Bit.AFIO = 1;
  CKCU_PeripClockConfig(CKCUClock, ENABLE);

  /* SPI1 四個腳位全部設 AFIO_FUN_SPI (含 SS=PC10，消除 Mode Fault) */
  AFIO_GPxConfig(GPIO_PC, AFIO_PIN_11, AFIO_FUN_SPI); /* SCK  D13 */
  AFIO_GPxConfig(GPIO_PC, AFIO_PIN_8,  AFIO_FUN_SPI); /* MOSI D11 */
  AFIO_GPxConfig(GPIO_PC, AFIO_PIN_9,  AFIO_FUN_SPI); /* MISO D12 */
  AFIO_GPxConfig(GPIO_PC, AFIO_PIN_10, AFIO_FUN_SPI); /* SS   D10 */
  GPIO_PullResistorConfig(HT_GPIOC, GPIO_PIN_9, GPIO_PR_UP); /* MISO 上拉 */

  /* DC (PA15), RES (PA14) → GPIO 輸出 */
  AFIO_GPxConfig(GPIO_PA, AFIO_PIN_15, AFIO_FUN_GPIO);
  AFIO_GPxConfig(GPIO_PA, AFIO_PIN_14, AFIO_FUN_GPIO);
  GPIO_DirectionConfig(OLED_DC_PORT,  OLED_DC_PIN,  GPIO_DIR_OUT);
  GPIO_DirectionConfig(OLED_RES_PORT, OLED_RES_PIN, GPIO_DIR_OUT);
  OLED_DC_DAT();
  OLED_RES_H();

  /* SPI1 初始化 (HARDWARE SEL，7.5 MHz，SPI Mode 0，MSB first) */
  SPI_InitStructure.SPI_Mode               = SPI_MASTER;
  SPI_InitStructure.SPI_FIFO               = SPI_FIFO_DISABLE;
  SPI_InitStructure.SPI_DataLength         = SPI_DATALENGTH_8;
  SPI_InitStructure.SPI_SELMode            = SPI_SEL_HARDWARE;
  SPI_InitStructure.SPI_SELPolarity        = SPI_SELPOLARITY_LOW;
  SPI_InitStructure.SPI_CPOL              = SPI_CPOL_LOW;
  SPI_InitStructure.SPI_CPHA              = SPI_CPHA_FIRST;
  SPI_InitStructure.SPI_FirstBit          = SPI_FIRSTBIT_MSB;
  SPI_InitStructure.SPI_RxFIFOTriggerLevel = 0;
  SPI_InitStructure.SPI_TxFIFOTriggerLevel = 0;
  SPI_InitStructure.SPI_ClockPrescaler    = 8;   /* 60 MHz / 8 = 7.5 MHz */
  SPI_Init(OLED_SPI_PORT, &SPI_InitStructure);

  SPI_SELOutputCmd(OLED_SPI_PORT, ENABLE);
  SPI_Cmd(OLED_SPI_PORT, ENABLE);
}

/* ==========================================================================
 * 【第八區】I2C1 初始化 (for ADS1115)
 *   PC4 = SDA, PC5 = SCL, 內部上拉, 100 kHz
 * ==========================================================================*/
void I2C_Configuration(void)
{
  CKCU_PeripClockConfig_TypeDef CKCUClock = {{0}};

  /* 1. 開啟時鐘：I2C1, PORTC, AFIO */
  CKCUClock.Bit.I2C1 = 1;
  CKCUClock.Bit.PC   = 1;
  CKCUClock.Bit.AFIO = 1;
  CKCU_PeripClockConfig(CKCUClock, ENABLE);

  /* 2. 設定 GPIO PC4 (SDA), PC5 (SCL) 為 I2C 複用功能 */
  AFIO_GPxConfig(GPIO_PC, AFIO_PIN_5, AFIO_FUN_I2C); /* SCL */
  AFIO_GPxConfig(GPIO_PC, AFIO_PIN_4, AFIO_FUN_I2C); /* SDA */

  /* 3. 開啟內部上拉電阻 */
  GPIO_PullResistorConfig(HT_GPIOC, GPIO_PIN_4, GPIO_PR_UP);
  GPIO_PullResistorConfig(HT_GPIOC, GPIO_PIN_5, GPIO_PR_UP);

  /* 4. 配置 I2C1 參數 */
  {
    I2C_InitTypeDef I2C_InitStructure;
    I2C_InitStructure.I2C_GeneralCall    = DISABLE;
    I2C_InitStructure.I2C_AddressingMode = I2C_ADDRESSING_7BIT;
    I2C_InitStructure.I2C_Acknowledge    = DISABLE;
    I2C_InitStructure.I2C_OwnAddress     = 0x0A;
    I2C_InitStructure.I2C_Speed          = I2C_CLOCK_SPEED;
    I2C_InitStructure.I2C_SpeedOffset    = 0;
    I2C_Init(ADS1115_I2C_PORT, &I2C_InitStructure);
  }

  /* 5. 啟動 I2C1 */
  I2C_Cmd(ADS1115_I2C_PORT, ENABLE);
}

/* ==========================================================================
 * 【第九區】ADS1115 讀取電壓 (Channel A0, ±4.096 V, 單次模式)
 * ==========================================================================*/
float ADS1115_ReadVoltage_A0(void)
{
  /* Config register:
   *   MSB = 0xC3 : 開始單次 | AIN0 | ±4.096V | 單次模式
   *   LSB = 0x83 : 128 SPS | 傳統比較 | 停用比較器
   */
  u8 config_msb = 0xC3;
  u8 config_lsb = 0x83;
  u8 data_msb = 0, data_lsb = 0;

  /* ── 步驟 1：寫 Config Register (0x01) ── */
  I2C_TargetAddressConfig(ADS1115_I2C_PORT, ADS1115_ADDR, I2C_MASTER_WRITE);
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_SEND_START));
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_TRANSMITTER_MODE));

  I2C_SendData(ADS1115_I2C_PORT, 0x01);          /* 指向 Config Register */
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_TX_EMPTY));

  I2C_SendData(ADS1115_I2C_PORT, config_msb);
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_TX_EMPTY));

  I2C_SendData(ADS1115_I2C_PORT, config_lsb);
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_TX_EMPTY));

  I2C_GenerateSTOP(ADS1115_I2C_PORT);
  while (I2C_ReadRegister(ADS1115_I2C_PORT, I2C_REGISTER_SR) & 0x80000); /* Bus 閒置 */

  /* 等待 ADS1115 轉換完成 (128 SPS ≈ 8 ms，給 15 ms 確保穩定) */
  Delay_ms(15);

  /* ── 步驟 2：指向 Conversion Register (0x00) ── */
  I2C_TargetAddressConfig(ADS1115_I2C_PORT, ADS1115_ADDR, I2C_MASTER_WRITE);
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_SEND_START));
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_TRANSMITTER_MODE));

  I2C_SendData(ADS1115_I2C_PORT, 0x00);
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_TX_EMPTY));

  I2C_GenerateSTOP(ADS1115_I2C_PORT);
  while (I2C_ReadRegister(ADS1115_I2C_PORT, I2C_REGISTER_SR) & 0x80000);

  /* ── 步驟 3：讀取 2 Bytes ── */
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

  /* ── 步驟 4：計算電壓 ── */
  return (float)(int16_t)((data_msb << 8) | data_lsb) * (4.096f / 32768.0f);
}

/* ==========================================================================
 * 【第十區】SPI 輪詢傳送
 * ==========================================================================*/
void SPI_PollSendByte(u8 data)
{
  while (!SPI_GetFlagStatus(OLED_SPI_PORT, SPI_FLAG_TXBE));
  SPI_SendData(OLED_SPI_PORT, data);
  while (!SPI_GetFlagStatus(OLED_SPI_PORT, SPI_FLAG_RXBNE));
  SPI_ReceiveData(OLED_SPI_PORT);
}

/* ==========================================================================
 * 【第十一區】OLED 寫入指令 / 資料
 * ==========================================================================*/
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

/* ==========================================================================
 * 【第十二區】SSD1309 初始化
 * ==========================================================================*/
void OLED_Init(void)
{
  OLED_RES_H(); Delay_ms(1);
  OLED_RES_L(); Delay_ms(1);
  OLED_RES_H(); Delay_ms(10);

  OLED_WriteCmd(0xAE); /* Display OFF */
  OLED_WriteCmd(0x20); /* Memory Addressing Mode */
  OLED_WriteCmd(0x00); /* Horizontal Addressing Mode */
  OLED_WriteCmd(0x81); /* Set Contrast */
  OLED_WriteCmd(0x7F);
  OLED_WriteCmd(0xA6); /* Normal Display */
  OLED_WriteCmd(0xAF); /* Display ON */

  /* 設定全螢幕範圍後清除 */
  OLED_WriteCmd(0x21); OLED_WriteCmd(0x00); OLED_WriteCmd(0x7F);
  OLED_WriteCmd(0x22); OLED_WriteCmd(0x00); OLED_WriteCmd(0x07);
  OLED_Clear();
}

/* ==========================================================================
 * 【第十三區】OLED 顯示工具函數
 * ==========================================================================*/
void OLED_SetCursor(u8 page, u8 col)
{
  OLED_WriteCmd(0x21);
  OLED_WriteCmd(col);
  OLED_WriteCmd(0x7F);
  OLED_WriteCmd(0x22);
  OLED_WriteCmd(page);
  OLED_WriteCmd(0x07);
}

void OLED_Fill(u8 val)
{
  u16 i;
  OLED_WriteCmd(0x21); OLED_WriteCmd(0x00); OLED_WriteCmd(0x7F);
  OLED_WriteCmd(0x22); OLED_WriteCmd(0x00); OLED_WriteCmd(0x07);
  for (i = 0; i < 1024; i++)
    OLED_WriteData(val);
}

void OLED_Clear(void) { OLED_Fill(0x00); }

void OLED_ClearLine(u8 page)
{
  u8 col;
  OLED_SetCursor(page, 0);
  for (col = 0; col < OLED_WIDTH; col++)
    OLED_WriteData(0x00);
}

void OLED_DrawChar(u8 page, u8 col, char ch)
{
  u8 i;
  if (ch < 0x20 || ch > 0x7E) ch = ' ';
  OLED_SetCursor(page, col);
  for (i = 0; i < 8; i++)
    OLED_WriteData(Font8x8[ch - 0x20][i]);
}

void OLED_DrawString(u8 page, u8 col, const char *str)
{
  while (*str) {
    if (col + 8 > OLED_WIDTH) break;
    OLED_DrawChar(page, col, *str++);
    col += 8;
  }
}

void OLED_DrawLine(u8 page)
{
  u8 col;
  OLED_SetCursor(page, 0);
  for (col = 0; col < OLED_WIDTH; col++)
    OLED_WriteData(0xFF);
}

/* ==========================================================================
 * 【第十四區】Float 轉字串（避免使用 sprintf 浮點格式）
 *   val      : 輸入值 (不限符號)
 *   buf      : 輸出字串緩衝，至少 16 bytes
 *   decimals : 小數位數 (0~4)
 * ==========================================================================*/
void FloatToStr(float val, char *buf, u8 decimals)
{
  u8   idx = 0;
  u32  mul = 1;
  u8   d;
  u32  frac;
  u32  intpart;
  char tmp[16];
  u8   tlen, i;

  /* 負數 */
  if (val < 0.0f) { buf[idx++] = '-'; val = -val; }

  /* 計算倍率 */
  for (d = 0; d < decimals; d++) mul *= 10;

  intpart = (u32)val;
  frac    = (u32)((val - (float)intpart) * (float)mul + 0.5f);

  /* 整數部分 (反序) */
  tlen = 0;
  if (intpart == 0) { tmp[tlen++] = '0'; }
  else {
    u32 v = intpart;
    while (v > 0) { tmp[tlen++] = '0' + (u8)(v % 10); v /= 10; }
  }
  for (i = tlen; i > 0; i--) buf[idx++] = tmp[i-1];

  /* 小數部分 */
  if (decimals > 0) {
    buf[idx++] = '.';
    /* 補零並填入 */
    tlen = 0;
    {
      u32 f = frac;
      for (d = 0; d < decimals; d++) {
        tmp[decimals - 1 - d] = '0' + (u8)(f % 10);
        f /= 10;
      }
    }
    for (d = 0; d < decimals; d++) buf[idx++] = tmp[d];
  }

  buf[idx] = '\0';
}

/* ==========================================================================
 * 【第十五區】延遲函數
 * ==========================================================================*/
void Delay_ms(u32 ms)
{
  volatile u32 i;
  while (ms--)
    for (i = 0; i < 12000; i++);
}

/* ==========================================================================
 * 【附錄】assert_error (HT32 除錯用)
 * ==========================================================================*/
#if (HT32_LIB_DEBUG == 1)
void assert_error(u8 *filename, u32 uline)
{
  while (1) {}
}
#endif