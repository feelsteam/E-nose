#ifndef __HT32_BOARD_CONFIG_H
#define __HT32_BOARD_CONFIG_H

#ifdef __cplusplus
 extern "C" {
#endif

/*
 * EEPROM project 已改為 ADS1115 + SSD1309 OLED 合併程式
 * I2C1 (PC4=SDA, PC5=SCL) — 與 7_bit_mode_master 測試成功組態相同
 * 本 header 保留作相容性佔位，實際 I2C 設定已直接寫於 main.c
 */

#ifdef __cplusplus
}
#endif

#endif /* __HT32_BOARD_CONFIG_H */