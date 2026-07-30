// ============================================================
// Measured_value — 感測器即時電壓監測
// 顯示：ADS1115 差動(AIN0-AIN1)、類比 A1、類比 A3
// 更新率：500ms
// ============================================================
#include <Adafruit_ADS1X15.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <Wire.h>

Adafruit_ADS1115 ads;
U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2(U8G2_R0, 10, 9, 8);

unsigned long lastUpdate = 0;

void setup() {
  Serial.begin(115200);

  // 12-bit ADC，3.3V 基準
  analogReadResolution(12);
  analogReference(DEFAULT);

  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x13_tr);
  u8g2.drawStr(0, 20, "Initializing...");
  u8g2.sendBuffer();

  Wire.begin();
  if (!ads.begin()) {
    Serial.println("[ERROR] ADS1115 not found! Check I2C wiring.");
    u8g2.clearBuffer();
    u8g2.drawStr(0, 20, "ADS1115 FAIL!");
    u8g2.drawStr(0, 36, "Check wiring");
    u8g2.sendBuffer();
    while (1);
  }

  // GAIN_FOUR = ±1.024V，與正式韌體一致
  // 若要看更大範圍電壓可改成 GAIN_ONE (±4.096V)
  ads.setGain(GAIN_FOUR);

  Serial.println("[OK] ADS1115 ready. Gain = FOUR (±1.024V)");
  Serial.println("---------------------------------------------");
  Serial.println("  ADS(V)      A0(V)  A1(V)  A2(V)  A3(V)");
  Serial.println("---------------------------------------------");
}

void loop() {
  if (millis() - lastUpdate < 500) return;
  lastUpdate = millis();

  // ── ADS1115 差動讀取（AIN0 - AIN1）──
  float v_ads = ads.computeVolts(ads.readADC_Differential_0_1());

  // ── 類比輸入（12-bit, 3.3V ref, ×2 分壓補償）──
  float v_a0 = (analogRead(A0) / 4095.0) * 3.3 * 2.0;
  float v_a1 = (analogRead(A1) / 4095.0) * 3.3 * 2.0;
  float v_a2 = (analogRead(A2) / 4095.0) * 3.3 * 2.0;
  float v_a3 = (analogRead(A3) / 4095.0) * 3.3 * 2.0;

  // ── Serial 輸出 ──
  Serial.print("ADS:"); Serial.print(v_ads, 4); Serial.print("V  ");
  Serial.print("A0:");  Serial.print(v_a0,  3);  Serial.print("V  ");
  Serial.print("A1:");  Serial.print(v_a1,  3);  Serial.print("V  ");
  Serial.print("A2:");  Serial.print(v_a2,  3);  Serial.print("V  ");
  Serial.print("A3:");  Serial.print(v_a3,  3);  Serial.println("V");

  // ── OLED 顯示（5 行，使用 6x10 小字型）──
  char buf[22];

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);

  snprintf(buf, sizeof(buf), "ADS:%+.4fV", v_ads);
  u8g2.setCursor(0, 10); u8g2.print(buf);

  snprintf(buf, sizeof(buf), "A0 :%.3fV", v_a0);
  u8g2.setCursor(0, 22); u8g2.print(buf);

  snprintf(buf, sizeof(buf), "A1 :%.3fV", v_a1);
  u8g2.setCursor(0, 34); u8g2.print(buf);

  snprintf(buf, sizeof(buf), "A2 :%.3fV", v_a2);
  u8g2.setCursor(0, 46); u8g2.print(buf);

  snprintf(buf, sizeof(buf), "A3 :%.3fV", v_a3);
  u8g2.setCursor(0, 58); u8g2.print(buf);

  u8g2.sendBuffer();
}
