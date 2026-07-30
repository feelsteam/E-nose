#include "ht32.h"
#include "ht32_board.h"
#include <stdio.h>

/* Private constants
 * ---------------------------------------------------------------------------------------*/
#define ADS1115_I2C_PORT (HT_I2C1) /* BM53A367A: PC4/PC5 對應 I2C1 */
#define ADS1115_ADDR 0x48      /* 預設 ADS1115 I2C 7-bit 位址 (ADDR 腳接 GND) */
#define I2C_CLOCK_SPEED 100000 /* 100kHz  */

/* Private function prototypes
 * -----------------------------------------------------------------------------*/
void I2C_Configuration(void);
float ADS1115_ReadVoltage_A0(void);
void Delay(vu32 count);

/* Global functions
 * ----------------------------------------------------------------------------------------*/
int main(void) {
  /* 初始化 UART 讓 printf 可以輸出到終端機 */
  RETARGET_Configuration();

  printf("\r\n======= HT32 I2C ADS1115 Polling Example =======\r\n");
  printf("Initializing I2C on PC4(SCL) and PC5(SDA)...\r\n");

  /* 初始化 I2C1 (PC4, PC5) 並開啟上拉電阻 */
  I2C_Configuration();

  printf("I2C Initialized! Entering main loop...\r\n");

  while (1) {
    /* 讀取 A0 電壓 (先印出提示，如果卡住就知道是 I2C 當掉) */
    float voltage = ADS1115_ReadVoltage_A0();

    /* 印出結果 (格式跟 ADC 範例類似) */
    printf("ADS1115 A0 (I2C): %.4f V\r\n", voltage);

    /* 延遲約 0.5 秒再讀取下一次 */
    {
      vu32 i;
      for (i = 0; i < 6000000; i++)
        ;
    }
  }
}

void I2C_Configuration(void) {
  { /* 1. 開啟周邊時脈 (I2C0, PORTC, AFIO) */
    CKCU_PeripClockConfig_TypeDef CKCUClock = {{0}};
    CKCUClock.Bit.I2C1 = 1; /* BM53A367A 使用 I2C1 */
    CKCUClock.Bit.PC = 1;
    CKCUClock.Bit.AFIO = 1;
    CKCU_PeripClockConfig(CKCUClock, ENABLE);
  }

  /* 2. 設定 GPIO PC4, PC5 為 I2C 複用功能 */
  AFIO_GPxConfig(GPIO_PC, AFIO_PIN_4, AFIO_FUN_I2C); /* SCL */
  AFIO_GPxConfig(GPIO_PC, AFIO_PIN_5, AFIO_FUN_I2C); /* SDA */

  /* ★ 新增：強制作為 I2C 使用的腳位開啟內部上拉電阻 (Pull-Up) ★ */
  GPIO_PullResistorConfig(HT_GPIOC, GPIO_PIN_4, GPIO_PR_UP);
  GPIO_PullResistorConfig(HT_GPIOC, GPIO_PIN_5, GPIO_PR_UP);

  { /* 3. 配置 I2C0 參數 */
    I2C_InitTypeDef I2C_InitStructure;
    I2C_InitStructure.I2C_GeneralCall = DISABLE;
    I2C_InitStructure.I2C_AddressingMode = I2C_ADDRESSING_7BIT;
    I2C_InitStructure.I2C_Acknowledge = DISABLE;
    I2C_InitStructure.I2C_OwnAddress = 0x0A; /* 主機自身位址(隨意設定) */
    I2C_InitStructure.I2C_Speed = I2C_CLOCK_SPEED;
    I2C_InitStructure.I2C_SpeedOffset = 0;
    I2C_Init(ADS1115_I2C_PORT, &I2C_InitStructure);
  }

  /* 4. 啟動 I2C */
  I2C_Cmd(ADS1115_I2C_PORT, ENABLE);
}

float ADS1115_ReadVoltage_A0(void) {
  u8 config_msb = 0xC3; /* 1100 0011: 開始單次轉換(1), AIN0(100),
                           +/- 4.096V(001), 單次模式(1) */
  u8 config_lsb = 0x83; /* 1000 0011: 128SPS(100), 傳統比較器(0), 負極性(0),
                           非鎖存(0), 停用比較器(11) */

  u8 data_msb = 0, data_lsb = 0;
  int16_t raw_adc = 0;

  /* ==========================================
     步驟 1：寫入設定值到 Config Register (0x01)
     ========================================== */
  I2C_TargetAddressConfig(ADS1115_I2C_PORT, ADS1115_ADDR, I2C_MASTER_WRITE);
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_SEND_START))
    ;
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_TRANSMITTER_MODE))
    ;

  I2C_SendData(ADS1115_I2C_PORT, 0x01); /* 指向 Config Register */
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_TX_EMPTY))
    ;

  I2C_SendData(ADS1115_I2C_PORT, config_msb);
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_TX_EMPTY))
    ;

  I2C_SendData(ADS1115_I2C_PORT, config_lsb);
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_TX_EMPTY))
    ;

  I2C_GenerateSTOP(ADS1115_I2C_PORT);
  while (I2C_ReadRegister(ADS1115_I2C_PORT, I2C_REGISTER_SR) & 0x80000)
    ; /* 等待 Bus 閒置 */

  /* 等待轉換完成 (128 SPS 大約需要 8ms) */
  Delay(1000000);

  /* ==========================================
     步驟 2：將指標指向 Conversion Register (0x00)
     ========================================== */
  I2C_TargetAddressConfig(ADS1115_I2C_PORT, ADS1115_ADDR, I2C_MASTER_WRITE);
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_SEND_START))
    ;
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_TRANSMITTER_MODE))
    ;

  I2C_SendData(ADS1115_I2C_PORT, 0x00); /* 指向 Conversion Register */
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_TX_EMPTY))
    ;

  I2C_GenerateSTOP(ADS1115_I2C_PORT);
  while (I2C_ReadRegister(ADS1115_I2C_PORT, I2C_REGISTER_SR) & 0x80000)
    ;

  /* ==========================================
     步驟 3：讀取 2 Bytes 的 ADC 數值
     ========================================== */
  I2C_TargetAddressConfig(ADS1115_I2C_PORT, ADS1115_ADDR, I2C_MASTER_READ);
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_SEND_START))
    ;
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_RECEIVER_MODE))
    ;

  /* 開啟 ACK，準備接收第一個 Byte */
  I2C_AckCmd(ADS1115_I2C_PORT, ENABLE);
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_RX_NOT_EMPTY))
    ;
  data_msb = I2C_ReceiveData(ADS1115_I2C_PORT);

  /* 準備接收最後一個 Byte，需關閉 ACK (發送 NACK) */
  I2C_AckCmd(ADS1115_I2C_PORT, DISABLE);
  while (!I2C_CheckStatus(ADS1115_I2C_PORT, I2C_MASTER_RX_NOT_EMPTY))
    ;
  data_lsb = I2C_ReceiveData(ADS1115_I2C_PORT);

  I2C_GenerateSTOP(ADS1115_I2C_PORT);
  while (I2C_ReadRegister(ADS1115_I2C_PORT, I2C_REGISTER_SR) & 0x80000)
    ;

  /* ==========================================
     步驟 4：計算電壓值
     ========================================== */
  raw_adc = (int16_t)((data_msb << 8) | data_lsb);

  return (float)raw_adc * (4.096f / 32768.0f);
}

void Delay(vu32 count) {
  while (count--)
    ;
}

#if (HT32_LIB_DEBUG == 1)
void assert_error(u8 *filename, u32 uline) {
  while (1) {
  }
}
#endif