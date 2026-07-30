#include <SPI.h>

// 定義 SSD1309 控制腳位 (對應 BMduino 上的 D8, D9, D10)
const int OLED_RES = 8;   // Reset
const int OLED_DC  = 9;   // Data/Command
const int OLED_CS  = 10;  // Chip Select

// 硬體 SPI 腳位 (BMduino 預設硬體 SPI)
// SCL (SCK)  = 13
// SDA (MOSI) = 11

// --- 傳送指令的函式 ---
void sendCommand(uint8_t cmd) {
  digitalWrite(OLED_DC, LOW);      // D/C 拉低代表傳送指令
  digitalWrite(OLED_CS, LOW);      // CS 拉低開始通訊
  SPI.transfer(cmd);               // 透過硬體 SPI 移位傳送
  digitalWrite(OLED_CS, HIGH);     // CS 拉高結束通訊
}

// --- 傳送資料的函式 ---
void sendData(uint8_t data) {
  digitalWrite(OLED_DC, HIGH);     // D/C 拉高代表傳送資料
  digitalWrite(OLED_CS, LOW);      // CS 拉低開始通訊
  SPI.transfer(data);              // 透過硬體 SPI 移位傳送
  digitalWrite(OLED_CS, HIGH);     // CS 拉高結束通訊
}

void setup() {
  // 1. 初始化腳位為輸出模式
  pinMode(OLED_CS, OUTPUT);
  pinMode(OLED_DC, OUTPUT);
  pinMode(OLED_RES, OUTPUT);

  // 預設將 CS 拉高 (不選取晶片)
  digitalWrite(OLED_CS, HIGH);

  // 2. 初始化硬體 SPI
  // SSD1309 支援最高 10MHz 的 SPI 時脈，我們這裡設定 8MHz 確保通訊穩定
  SPI.begin();
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));

  // 3. 硬體重置 SSD1309 (嚴格遵照 Datasheet 時序)
  // RES# 必須保持 LOW 至少 3us [cite: 1270]
  // 恢復 HIGH 後必須等待至少 3us 才能發送指令 [cite: 1272]
  digitalWrite(OLED_RES, HIGH);
  delay(1);
  digitalWrite(OLED_RES, LOW);
  delayMicroseconds(10);           
  digitalWrite(OLED_RES, HIGH);
  delayMicroseconds(10);           

  // 4. 發送 SSD1309 基礎初始化指令
  sendCommand(0xAE); // 關閉顯示 (Display OFF / Sleep mode) [cite: 1495]

  sendCommand(0x20); // 設定記憶體定址模式 
  sendCommand(0x00); // 0x00 = 水平定址模式 (Horizontal Addressing Mode)

  sendCommand(0x81); // 設定對比度
  sendCommand(0x7F); // 預設對比度為 0x7F

  sendCommand(0xA6); // 設定正常顯示 (Normal display，1=亮，0=暗)

  sendCommand(0xAF); // 開啟顯示 (Display ON) [cite: 1495]

  // 5. 初始清除螢幕 (將 128x64 全部填入 0x00)
  sendCommand(0x21); // 設定行 (Column) 範圍
  sendCommand(0x00); // Start Column = 0
  sendCommand(0x7F); // End Column = 127

  sendCommand(0x22); // 設定頁 (Page) 範圍
  sendCommand(0x00); // Start Page = 0
  sendCommand(0x07); // End Page = 7

  for (int i = 0; i < 1024; i++) { // 128 columns * 8 pages = 1024 bytes
    sendData(0x00);
  }
}

void loop() {
  // --- 測試畫面 1：顯示水平相間的條紋 (0xAA = 二進位 10101010) ---
  sendCommand(0x21); // 重設行範圍
  sendCommand(0x00);
  sendCommand(0x7F);

  sendCommand(0x22); // 重設頁範圍
  sendCommand(0x00);
  sendCommand(0x07);

  for (int i = 0; i < 1024; i++) {
    sendData(0xAA);
  }

  delay(1500); // 畫面停留 1.5 秒

  // --- 測試畫面 2：反轉的水平條紋 (0x55 = 二進位 01010101) ---
  sendCommand(0x21); 
  sendCommand(0x00);
  sendCommand(0x7F);

  sendCommand(0x22); 
  sendCommand(0x00);
  sendCommand(0x07);

  for (int i = 0; i < 1024; i++) {
    sendData(0x55);
  }

  delay(1500); // 畫面停留 1.5 秒
}