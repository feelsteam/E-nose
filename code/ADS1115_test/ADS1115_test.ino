#include <Wire.h>
#include <Adafruit_ADS1X15.h>

// 宣告一個 ADS1115 物件
Adafruit_ADS1115 ads;

void setup(void) {
  Serial.begin(115200);
  Serial.println("Hello! 正在啟動 ADS1115 硬體測試...");

  // 預設 I2C 位址為 0x48 (ADDR 腳位懸空或接地)
  // 如果初始化失敗，代表硬體沒接好或是晶片壞了
  delay(3000);
  if (!ads.begin()) {
    Serial.println("❌ 慘了！找不到 ADS1115。");
    Serial.println("請檢查：1. 杜邦線是否斷裂 2. SDA/SCL 是否接反 3. 模組是否通電");
    while (1); // 程式卡死在這裡
  }
  
  Serial.println("✅ 成功連上 ADS1115！");
  
  // 設定放大倍率 (Gain)
  // GAIN_TWOTHIRDS: 測量範圍 +/-6.144V (預設值)
  // GAIN_ONE:       測量範圍 +/-4.096V
  ads.setGain(GAIN_TWOTHIRDS); 
}

void loop(void) {
  int16_t adc0;
  float volts0;
  delay(5000);

  // 讀取 A0 腳位的單端 (Single-ended) 原始數值
  adc0 = ads.readADC_SingleEnded(0);
  
  // 讓函式庫幫我們把原始數值轉換成真實電壓 (伏特)
  volts0 = ads.computeVolts(adc0);

  Serial.print("A0 原始數值 (Raw): ");
  Serial.print(adc0);
  Serial.print("\t 換算電壓 (Voltage): ");
  Serial.print(volts0, 3); // 顯示到小數點後三位
  Serial.println(" V");

}
