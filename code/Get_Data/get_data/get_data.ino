// 完整版_ADS+A1+A3+WiFi傳輸 (CNN 預觸發資料收集架構 - 動態基準版)
#include <Adafruit_ADS1X15.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <Wire.h>

// ==========================================
// --- WiFi 熱點與 GAS 設定 ---
// ==========================================
#define WIFI_SSID "iPhone"     // 你的手機基地台名稱
#define WIFI_PASS "57913bxm34" // 你的手機基地台密碼
#define GAS_URL                                                                \
  "/macros/s/"                                                                 \
  "AKfycby-rT6uT8QItvnjfsfGEON0rjBHa07nUH7MJdRKnBGznG78qs262Wz3tb291-MOsQ/"    \
  "exec"
#define GAS_HOST "script.google.com"

// ==========================================
// --- 硬體與腳位設定 ---
// ==========================================
#define BTN_PIN 0 // 測驗對象 ID 增加按鈕 (D0, 連接 D0 與 GND，使用內部上拉)

Adafruit_ADS1115 ads;
U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2(U8G2_R0, 10, 9, 8);

// ==========================================
// --- 系統狀態機與變數 ---
// ==========================================
enum SystemState {
  STATE_HEATING,
  STATE_IDLE,
  STATE_RECORDING,
  STATE_UPLOADING
};
SystemState currentState = STATE_HEATING;

unsigned long heatUpTime = 60000; // 預熱時間，4 分鐘 (240000ms)

// 按鈕與 ID 邏輯
int currentSubjectID = 1; // 初始值
bool previousBtnState = HIGH;
unsigned long lastBtnDebounceTime = 0;
int jump_num = 2; // 跳躍筆數

// ==========================================
// --- 資料緩衝區 (200筆) ---
// ==========================================
struct DataPoint {
  uint16_t mq136;
  uint16_t mq137;
  float tgs; // ADS 感測電壓絕對值
};

DataPoint recordBuffer[200];
int recordIndex = 0;         // 紀錄目前存到第幾筆
float currentBaseline = 0.0; // 閒置狀態下的動態基準線 (TGS)
int base_mq136 = 0;
int base_mq137 = 0;

unsigned long lastSampleTime = 0;

// ==========================================
// --- 輔助函式 ---
// ==========================================

// 傳送 AT 指令的輔助函式
bool sendATCommand(String cmd, String expected, unsigned long timeout) {
  if (cmd.length() > 0) {
    while (Serial4.available())
      Serial4.read();
    Serial4.println(cmd);
    Serial.println("[AT送出] -> " + cmd);
  }

  unsigned long t = millis();
  String response = "";
  bool found = false;

  while (millis() - t < timeout) {
    if (Serial4.available()) {
      char c = Serial4.read();
      response += c;
      if (response.indexOf(expected) != -1) {
        found = true;
        break;
      }
    }
  }
  Serial.println("[AT回傳] <- \n" + response);
  return found;
}

// OLED 顯示更新函式
void updateOLED() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.setCursor(0, 10);
  u8g2.print("Subject ID: ");
  u8g2.print(currentSubjectID);

  if (currentState == STATE_HEATING) {
    int timeLeft = (heatUpTime - millis()) / 1000;
    if (timeLeft < 0)
      timeLeft = 0;
    u8g2.setCursor(0, 25);
    u8g2.print("Status: Heating...");
    u8g2.setCursor(0, 40);
    u8g2.print("Wait: ");
    u8g2.print(timeLeft);
    u8g2.print(" s");
  } else if (currentState == STATE_IDLE) {
    u8g2.setCursor(0, 25);
    u8g2.print("Status: IDLE (Wait)");
    u8g2.setCursor(0, 40);
    u8g2.print("Base: ");
    u8g2.print(currentBaseline, 3);
    u8g2.print("V");
    u8g2.setCursor(0, 55);
    u8g2.print("Curr: ");
    u8g2.print(recordBuffer[49].tgs, 3);
    u8g2.print("V");
  } else if (currentState == STATE_RECORDING) {
    u8g2.setCursor(0, 25);
    u8g2.print("Status: RECORDING!!");
    u8g2.setCursor(0, 40);
    u8g2.print("Points: ");
    u8g2.print(recordIndex);
    u8g2.print("/200");
    // 繪製簡單的進度條
    int barWidth = map(recordIndex, 0, 200, 0, 128);
    u8g2.drawBox(0, 50, barWidth, 10);
  } else if (currentState == STATE_UPLOADING) {
    u8g2.setCursor(0, 25);
    u8g2.print("Status: UPLOADING...");
    u8g2.setCursor(0, 40);
    u8g2.print("Sending to Google");
  }

  u8g2.sendBuffer();
}

// 初始化 WiFi 連線
void initWiFi() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 15, "WiFi Connecting...");
  u8g2.sendBuffer();

  Serial.println("=== 開始初始化 WiFi ===");
  delay(1500);

  sendATCommand("AT", "OK", 2000);
  sendATCommand("AT+RST", "OK", 2000);
  delay(2000);

  sendATCommand("AT+CWMODE=1", "OK", 2000); // Station mode

  String joinCmd = "AT+CWJAP=\"";
  joinCmd += WIFI_SSID;
  joinCmd += "\",\"";
  joinCmd += WIFI_PASS;
  joinCmd += "\"";

  if (sendATCommand(joinCmd, "WIFI GOT IP", 15000)) {
    Serial.println(">>> [SYSTEM] WiFi 連線成功! 已經連上基地台 <<<");
  } else {
    Serial.println(">>> [ERROR] WiFi 連線失敗! 請檢查帳密與熱點狀態 <<<");
  }

  sendATCommand("AT+CIPMUX=0", "OK", 2000); // 單一連線模式
}

void setup() {
  Serial.begin(115200);
  Serial4.begin(115200);
  Serial4.setTimeout(100);

  pinMode(BTN_PIN, INPUT_PULLUP);

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
  ads.setGain(GAIN_FOUR);

  initWiFi();
  Serial.println("[SYSTEM] 系統初始化完畢，準備進入加熱階段...");
}

void loop() {
  unsigned long currentMillis = millis();

  // ------------------------------------------
  // 1. 處理實體按鈕 (ID 切換)
  // ------------------------------------------
  int btnReading = digitalRead(BTN_PIN);
  if (btnReading != previousBtnState) {
    lastBtnDebounceTime = currentMillis;
  }
  if ((currentMillis - lastBtnDebounceTime) > 50) {
    if (btnReading == LOW && currentState == STATE_IDLE) {
      currentSubjectID = currentSubjectID + jump_num;
      Serial.print("[SYSTEM] 受測者 ID 增加，目前為: 對象 ");
      Serial.println(currentSubjectID);
      while (digitalRead(BTN_PIN) == LOW) {
        delay(10);
      }
      updateOLED();
    }
  }
  previousBtnState = btnReading;

  // ------------------------------------------
  // 2. 狀態機邏輯處理
  // ------------------------------------------
  switch (currentState) {

  // --- 階段 1：預熱 (4分鐘) ---
  case STATE_HEATING: {
    static unsigned long lastHeatingPrint = 0;
    static unsigned long lastHeatingOled = 0;

    if (currentMillis >= heatUpTime) {
      float initial_tgs = ads.computeVolts(ads.readADC_Differential_0_1());
      int initial_mq136 = analogRead(A1);
      int initial_mq137 = analogRead(A3);

      // 填滿初始緩衝區
      for (int i = 0; i < 50; i++) {
        recordBuffer[i] = {initial_mq136, initial_mq137, initial_tgs};
      }
      currentBaseline = initial_tgs;
      base_mq136 = initial_mq136;
      base_mq137 = initial_mq137;

      currentState = STATE_IDLE;
      lastSampleTime = millis();
      Serial.println(
          "\n[SYSTEM] 預熱期結束！進入 IDLE 待機模式尋找動態基準線。");
      updateOLED();
      break;
    }

    // 每 5 秒在 Serial 印出加熱近況
    if (currentMillis - lastHeatingPrint >= 5000) {
      lastHeatingPrint = currentMillis;
      int timeLeft = (heatUpTime - currentMillis) / 1000;
      float currentVolt = ads.computeVolts(ads.readADC_Differential_0_1());
      Serial.print("[HEATING] 預熱中... 剩餘: ");
      Serial.print(timeLeft);
      Serial.print(" 秒, TGS目前電壓: ");
      Serial.print(currentVolt, 4);
      Serial.println(" V");
    }

    // 每 1 秒更新 OLED 倒數
    if (currentMillis - lastHeatingOled >= 1000) {
      lastHeatingOled = currentMillis;
      updateOLED();
    }
    break;
  }

  // --- 階段 2：待機偵測 (環狀預觸發緩衝區 + 動態基線演算法) ---
  case STATE_IDLE: {
    if (currentMillis - lastSampleTime >= 100) { // 嚴格維持 100ms 取樣率
      lastSampleTime += 100;

      // 移位緩衝區 (改為 50 筆)
      memmove(&recordBuffer[0], &recordBuffer[1], 49 * sizeof(DataPoint));

      // 讀取最新的一筆
      recordBuffer[49].mq136 = analogRead(A1);
      recordBuffer[49].mq137 = analogRead(A3);
      recordBuffer[49].tgs = ads.computeVolts(ads.readADC_Differential_0_1());

      // --- 動態基準電壓演算法 ---
      // 計算近 5 秒 (50 筆資料) 的下降幅度
      float drop = recordBuffer[0].tgs - recordBuffer[49].tgs;
      bool baselineUpdated = false;

      // 若下降幅度在 0v 到 0.005v 之間，視為極度平穩，更新基準點
      if (drop >= 0.0 && drop <= 0.002) {
        currentBaseline = recordBuffer[49].tgs;
        base_mq136 = recordBuffer[49].mq136;
        base_mq137 = recordBuffer[49].mq137;
        baselineUpdated = true;
      }

      // 每秒 (每 10 次取樣) 印出一次 Serial 以供監看
      static int idlePrintCounter = 0;
      idlePrintCounter++;
      if (idlePrintCounter >= 10) {
        Serial.print("[IDLE] MQ136: ");
        Serial.print(recordBuffer[49].mq136);
        Serial.print("(Base:");
        Serial.print(base_mq136);
        Serial.print(") | ");
        Serial.print("MQ137: ");
        Serial.print(recordBuffer[49].mq137);
        Serial.print("(Base:");
        Serial.print(base_mq137);
        Serial.print(") | ");
        Serial.print("TGS: ");
        Serial.print(recordBuffer[49].tgs, 3);
        Serial.print("V(Base:");
        Serial.print(currentBaseline, 3);
        Serial.println("V)");

        if (baselineUpdated) {
          Serial.println(
              "       -> [INFO] 感測器已達平穩狀態，自動咬合新基準電壓！");
        }

        idlePrintCounter = 0;
        updateOLED(); // 也順便更新 OLED
      }

      // --- 核心觸發邏輯 ---
      // 若當下電壓比 動態基準點 飆升大於 0.2V，引發連續紀錄
      if (recordBuffer[49].tgs - currentBaseline > 0.007) {
        Serial.println("\n=============================================");
        Serial.print("[TRIGGER] 偵測到大幅上升! 目前: ");
        Serial.print(recordBuffer[49].tgs, 3);
        Serial.print("V (高於基準 ");
        Serial.print(currentBaseline, 3);
        Serial.println("V)。開始連續錄製 20 秒!");
        Serial.println("=============================================\n");

        currentState = STATE_RECORDING;
        recordIndex =
            50; // 由於前 50 筆預觸發資料已在陣列內，從 index 50 繼續錄
        updateOLED();
      }
    }
    break;
  }

  // --- 階段 3：純記錄 (補足剩下的 180 筆資料) ---
  case STATE_RECORDING: {
    if (currentMillis - lastSampleTime >= 100) { // 嚴格 100ms
      lastSampleTime += 100;

      recordBuffer[recordIndex].mq136 = analogRead(A1);
      recordBuffer[recordIndex].mq137 = analogRead(A3);
      recordBuffer[recordIndex].tgs =
          ads.computeVolts(ads.readADC_Differential_0_1());

      // 紀錄期間，每 2 秒 (20 筆) 輸出一次進度到 Serial
      if (recordIndex % 20 == 0) {
        Serial.print("[RECORDING] 正在錄製資料... ");
        Serial.print(recordIndex);
        Serial.println(" / 200");
        updateOLED(); // OLED 每 2 秒才畫一次進度條，力求 100ms 時序精準
      }

      recordIndex++;

      // 錄滿 200 筆即轉往上傳狀態
      if (recordIndex >= 200) {
        Serial.println("[RECORDING] 20秒資料收集完畢！");
        currentState = STATE_UPLOADING;
        updateOLED();
      }
    }
    break;
  }

  // --- 階段 4：打包上傳雲端 ---
  case STATE_UPLOADING: {
    Serial.println("\n[UPLOADING] === 準備打包上傳至 Google Sheets ===");

    String idHeader = "ID:" + String(currentSubjectID) + "\n";

    // 動態計算 Payload 總長度
    unsigned long payloadLen = idHeader.length();
    float mq136_base_v = (base_mq136 / 4095.0) * 3.3 * 1.5;
    float mq137_base_v = (base_mq137 / 4095.0) * 3.3 * 1.5;

    for (int i = 0; i < 200; i++) {
      float mq136_v = (recordBuffer[i].mq136 / 4095.0) * 3.3 * 1.5;
      float mq137_v = (recordBuffer[i].mq137 / 4095.0) * 3.3 * 1.5;
      float mq136_vpp = mq136_v - mq136_base_v;
      float mq137_vpp = mq137_v - mq137_base_v;
      float tgs_vpp = recordBuffer[i].tgs - currentBaseline;

      String rowStr =
          String(i + 1) + "," + String(mq136_v, 4) + "," + String(mq137_v, 4) +
          "," + String(recordBuffer[i].tgs, 4) + "," + String(mq136_vpp, 4) +
          "," + String(mq137_vpp, 4) + "," + String(tgs_vpp, 4) + "\n";
      payloadLen += rowStr.length();
    }

    String httpReq = "POST " + String(GAS_URL) + " HTTP/1.1\r\n";
    httpReq += "Host: " + String(GAS_HOST) + "\r\n";
    httpReq += "Content-Type: text/plain\r\n";
    httpReq += "Connection: close\r\n";
    httpReq += "Content-Length: " + String(payloadLen) + "\r\n\r\n";

    Serial.println("[UPLOADING] 正在與 Google Server 建立 SSL 連線...");
    if (sendATCommand("AT+CIPSTART=\"SSL\",\"script.google.com\",443", "OK",
                      10000)) {

      Serial.println("[UPLOADING] 連線成功！因為 ESP 模組單次限制 2048 "
                     "bytes，開始分批傳送...");

      // 1. 傳送 HTTP Header 與 ID (約 200 bytes)
      String headChunk = httpReq + idHeader;
      if (sendATCommand("AT+CIPSEND=" + String(headChunk.length()), ">",
                        3000)) {
        Serial4.print(headChunk);
      }

      // 2. 分 8 批傳送資料 (每 25 筆為一個 Batch，約 1100 bytes，避免超過 2048
      // bytes)
      for (int batch = 0; batch < 8; batch++) {
        String dataChunk = "";
        for (int i = 0; i < 25; i++) {
          int idx = batch * 25 + i;
          float mq136_v = (recordBuffer[idx].mq136 / 4095.0) * 3.3 * 1.5;
          float mq137_v = (recordBuffer[idx].mq137 / 4095.0) * 3.3 * 1.5;
          float mq136_vpp = mq136_v - mq136_base_v;
          float mq137_vpp = mq137_v - mq137_base_v;
          float tgs_vpp = recordBuffer[idx].tgs - currentBaseline;

          dataChunk += String(idx + 1) + "," + String(mq136_v, 4) + "," +
                       String(mq137_v, 4) + "," +
                       String(recordBuffer[idx].tgs, 4) + "," +
                       String(mq136_vpp, 4) + "," + String(mq137_vpp, 4) + "," +
                       String(tgs_vpp, 4) + "\n";
        }
        if (sendATCommand("AT+CIPSEND=" + String(dataChunk.length()), ">",
                          3000)) {
          Serial4.print(dataChunk);
          Serial.print("[UPLOADING] 已送出 Batch ");
          Serial.print(batch + 1);
          Serial.println(" / 8");
        }
      }

      Serial.println("[UPLOADING] 資料全部送出，等待 Google 端完成作業...");
      bool ok = sendATCommand("", "HTTP/1.", 10000);
      if (ok) {
        Serial.println("\n[SUCCESS] 雲端存檔成功！\n");
      } else {
        Serial.println("\n[WARNING] 未收到 302/HTTP 回應，但是資料可能已經到達 "
                       "Google。\n");
      }
      sendATCommand("AT+CIPCLOSE", "OK", 1000);
    } else {
      Serial.println("[ERROR] SSL 連線失敗！網路可能塞車或ESP韌體問題。");
    }

    // 重設準備接下一輪量測
    Serial.println(
        "[SYSTEM] 系統存檔完畢。強制將當前電壓設為新臨時基準，等待回穩...");

    // 重新填寫前 50 筆緩衝區，並強制刷新目前的 Baseline
    // 防止在電壓高位下降時
    // (即使慢慢下降)，因為對比「吹氣前」的極低基準而一直發生誤觸發
    float current_tgs = ads.computeVolts(ads.readADC_Differential_0_1());
    int current_mq136 = analogRead(A1);
    int current_mq137 = analogRead(A3);
    for (int i = 0; i < 50; i++) {
      recordBuffer[i] = {current_mq136, current_mq137, current_tgs};
    }
    currentBaseline = current_tgs;
    base_mq136 = current_mq136;
    base_mq137 = current_mq137;

    recordIndex = 0;
    currentState = STATE_IDLE;
    lastSampleTime = millis();
    updateOLED();
    break;
  }
  }
}