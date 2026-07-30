// 簡易版_OLED顯示即時電壓 (A1, A2, A3, ADS1115)
#include <Adafruit_ADS1X15.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <Wire.h>

// ==========================================
// --- 硬體與腳位設定 ---
// ==========================================
Adafruit_ADS1115 ads;
U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2(U8G2_R0, 10, 9, 8);

unsigned long lastUpdate = 0;

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  analogReference(DEFAULT);

  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 20, "System Starting...");
  u8g2.sendBuffer();

  if (!ads.begin()) {
    Serial.println("[ERROR] ADS1115 Init Fail! 請檢查 I2C 接線");
    while (1)
      ;
  }
  ads.setGain(GAIN_ONE);

  Serial.println("[SYSTEM] 系統初始化完畢，開始讀取電壓...");
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastUpdate >= 500) { // 每 0.5 秒更新一次
    lastUpdate = currentMillis;

    // 讀取 ADC 值
    int val_a1 = analogRead(A1);
    int val_a2 = analogRead(A2);
    int val_a3 = analogRead(A3);

    // 計算電壓值 (12-bit ADC, 3.3V 基準，乘以 1.5 的分壓係數)
    // 註：若 A2 腳位硬體沒有接分壓電路，請將 A2 的 1.5 乘數拿掉
    float v_a1 = (val_a1 / 4095.0) * 3.3 * 2; 
    float v_a2 = (val_a2 / 4095.0) * 3.3 * 2;
    float v_a3 = (val_a3 / 4095.0) * 3.3 * 2;

    // 讀取 ADS1115 (A0, A1 差動)
    float v_ads = ads.computeVolts(ads.readADC_Differential_0_1());

    // 輸出到 Serial
    Serial.print("A1: ");     Serial.print(v_a1, 3);
    Serial.print("V, A2: ");  Serial.print(v_a2, 3);
    Serial.print("V, A3: ");  Serial.print(v_a3, 3);
    Serial.print("V, ADS: "); Serial.print(v_ads, 4);
    Serial.println("V");

    // 顯示在 OLED 上
    u8g2.clearBuffer();
    // 使用比較小一點的字型，讓四行都可以清楚顯示
    u8g2.setFont(u8g2_font_7x13_tr); 

    u8g2.setCursor(0, 13);
    u8g2.print("A1: ");
    u8g2.print(v_a1, 2);
    u8g2.print(" V");

    u8g2.setCursor(0, 28);
    u8g2.print("A2: ");
    u8g2.print(v_a2, 2);
    u8g2.print(" V");

    u8g2.setCursor(0, 43);
    u8g2.print("A3: ");
    u8g2.print(v_a3, 2);
    u8g2.print(" V");

    u8g2.setCursor(0, 58);
    u8g2.print("AD: ");
    u8g2.print(v_ads, 3);
    u8g2.print(" V");

    u8g2.sendBuffer();
  }
}