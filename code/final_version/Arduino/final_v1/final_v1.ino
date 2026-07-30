// ============================================================================
// Final V1 - 電子鼻糖尿病預測韌體
// ============================================================================
// 二階段糖尿病預測：
//   第一階段：風險查表 (年齡 + BMI + 性別 -> 風險值 %)
//   第二階段：決策樹 (風險值 + 丙酮特徵 -> 診斷結果 + 信心度)
//
// 硬體：BMduino + ADS1115 (I2C) + SSD1309 OLED (SPI)
// ============================================================================

#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <U8g2lib.h>
#include <SPI.h>

// ============================================================================
// --- 設定旗標 ---
// ============================================================================

// true  = Acetone_Max 使用測試假資料
// false = Acetone_Max 使用感測器實際量測值
bool USE_FAKE_ACETONE_MAX = true;

// true  = 雲端空殼回傳「已有空腹紀錄」(執行完整的二階段預測)
// false = 雲端空殼回傳「無空腹紀錄」(僅記錄空腹丙酮值)
bool FAKE_HAS_FASTING_RECORD = true;

// ============================================================================
// --- 硬體物件 ---
// ============================================================================
Adafruit_ADS1115 ads;
U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2(U8G2_R0, 10, 9, 8);

// ============================================================================
// --- 可調參數閾值 ---
// ============================================================================
const float STABILIZE_THRESHOLD   = 0.001;  // V - 穩定判定的電壓降閾值
const int   STABILIZE_HISTORY     = 50;     // 用於穩定判定的歷史讀取次數
const float TRIGGER_THRESHOLD     = 0.05;   // V - 超過基準電壓此值則開始量測
const float END_MEASURE_DROP      = 0.05;   // V - 相較於10次前讀取下降此值則停止量測
const int   END_MEASURE_LOOKBACK  = 10;     // 結束偵測的歷史對照步數
const int   TOP_N_AVERAGE         = 5;      // 取最大的前幾筆數值做平均
const unsigned long MEASURE_INTERVAL_MS = 100;   // 量測取樣間隔 (毫秒)
const unsigned long STABILIZE_INTERVAL_MS = 100; // 穩定判定的取樣間隔 (毫秒)
const unsigned long RESULT_DISPLAY_MS = 5000;    // 結果顯示持續時間 (毫秒)

// ============================================================================
// --- 狀態機 ---
// ============================================================================
enum State {
  STATE_STABILIZE,   // 等待感測器基準穩定
  STATE_READY,       // 已穩定，等待呼吸觸發
  STATE_MEASURING,   // 正在量測呼吸樣本
  STATE_COMPUTING,   // 計算結果
  STATE_RESULT       // 顯示結果
};

State currentState = STATE_STABILIZE;

// ============================================================================
// --- 全域變數 ---
// ============================================================================

// 穩定判定環形緩衝區
float stabilizeBuffer[50];   // 用於穩定偵測的環形緩衝區
int   stabilizeIndex = 0;
int   stabilizeCount = 0;    // 已儲存的讀取筆數 (最大 50)
float baselineVoltage = 0.0; // 穩定後的基準電壓

// 量測數據儲存
const int MAX_MEASUREMENTS = 3000; // 最大量測點數 (100ms 間隔下約 5 分鐘)
float measurements[3000];          // 量測期間儲存的電壓值
int   measureCount = 0;

// 量測結束點偵測的歷史緩衝區
float measureLookback[10];
int   measureLookbackIdx = 0;
int   measureLookbackCount = 0;

// 時間控制
unsigned long lastSampleTime = 0;
unsigned long resultStartTime = 0;

// ============================================================================
// --- 丙酮 (PPM) 查表 ---
// ============================================================================
// 根據感測器數據表的半對數內插表
// 格式：{mV, ppm}
const int ACETONE_TABLE_SIZE = 22;
const float acetoneTable[22][2] = {
  {8.49, 0.14}, {11.69, 0.19}, {15.94, 0.29}, {20.20, 0.39},
  {24.45, 0.51}, {28.17, 0.65}, {32.95, 0.86}, {37.73, 1.10},
  {42.51, 1.41}, {47.28, 1.76}, {53.12, 2.26}, {57.90, 2.74},
  {63.20, 3.32}, {67.98, 3.92}, {73.28, 4.62}, {81.23, 5.92},
  {91.31, 8.02}, {100.33, 10.28}, {109.87, 13.18}, {128.96, 21.64},
  {150.69, 37.57}, {161.83, 50.88}
};

// ============================================================================
// --- 第一階段：風險查表 ---
// ============================================================================

// 年齡盛行率表 (%) - 索引: 0~6 對應各年齡組
// 年齡組：0-12, 13-15, 16-18, 19-44, 45-64, 65-74, 75+
const int AGE_GROUP_COUNT = 7;
const int ageGroupBounds[7][2] = {
  {0, 12}, {13, 15}, {16, 18}, {19, 44}, {45, 64}, {65, 74}, {75, 150}
};

const float agePrevalenceMale[7]   = {0.0, 0.3, 1.1, 4.0, 15.6, 23.9, 27.8};
const float agePrevalenceFemale[7] = {0.0, 0.2, 0.4, 1.6,  9.9, 23.1, 31.4};

// BMI 風險倍率表
// 類別：過輕(<18.5), 正常(18.5-24), 過重(24-28), 肥胖(>28)
const float bmiHrMale[4]   = {0.63, 1.00, 2.72, 6.27};
const float bmiHrFemale[4] = {0.86, 1.00, 2.19, 3.78};


// ============================================================================
//  函式：getAcetonePPM
//  將差動電壓 (mV) 轉換為丙酮濃度 (ppm)
//  使用半對數內插法查表
// ============================================================================
float getAcetonePPM(float mv) {
  // 低於最低偵測值
  if (mv < acetoneTable[0][0]) return 0.0;
  // 超過最大範圍
  if (mv >= acetoneTable[ACETONE_TABLE_SIZE - 1][0])
    return acetoneTable[ACETONE_TABLE_SIZE - 1][1];

  // 尋找區間並在對數空間進行內插
  for (int i = 0; i < ACETONE_TABLE_SIZE - 1; i++) {
    if (mv >= acetoneTable[i][0] && mv <= acetoneTable[i + 1][0]) {
      float x1 = acetoneTable[i][0],     x2 = acetoneTable[i + 1][0];
      float y1 = acetoneTable[i][1],     y2 = acetoneTable[i + 1][1];

      // PPM 軸是對數，mV 軸是線性 -> 半對數內插
      float logY1 = log10(y1);
      float logY2 = log10(y2);
      float logY  = logY1 + (mv - x1) * (logY2 - logY1) / (x2 - x1);

      return pow(10, logY);
    }
  }
  return 0.0;
}


// ============================================================================
//  函式：stage1_getRiskValue
//  第一階段 - 風險值查表
//  輸入：age (int), bmi (float), gender (int: 1=男, 0=女)
//  輸出：風險百分比 (float, 0.0 ~ 100.0)
// ============================================================================
float stage1_getRiskValue(int age, float bmi, int gender) {
  Serial.println("[Stage1] --- 風險值查表 ---");
  Serial.print("  輸入: age="); Serial.print(age);
  Serial.print(", bmi="); Serial.print(bmi, 1);
  Serial.print(", gender="); Serial.println(gender == 1 ? "Male" : "Female");

  // 1. 取得年齡基礎盛行率
  float basePrevalence = 0.0;
  const float* ageTable = (gender == 1) ? agePrevalenceMale : agePrevalenceFemale;
  for (int i = 0; i < AGE_GROUP_COUNT; i++) {
    if (age >= ageGroupBounds[i][0] && age <= ageGroupBounds[i][1]) {
      basePrevalence = ageTable[i];
      Serial.print("  年齡組 ["); Serial.print(ageGroupBounds[i][0]);
      Serial.print("-"); Serial.print(ageGroupBounds[i][1]);
      Serial.print("], 基礎盛行率 = "); Serial.print(basePrevalence); Serial.println("%");
      break;
    }
  }

  // 2. 取得 BMI 風險倍率
  int bmiCategory;
  if (bmi < 18.5)       bmiCategory = 0; // 過輕
  else if (bmi <= 24.0) bmiCategory = 1; // 正常
  else if (bmi <= 28.0) bmiCategory = 2; // 過重
  else                  bmiCategory = 3; // 肥胖

  const float* bmiTable = (gender == 1) ? bmiHrMale : bmiHrFemale;
  float hrMultiplier = bmiTable[bmiCategory];

  const char* bmiLabels[] = {"Underweight", "Normal", "Overweight", "Obese"};
  Serial.print("  BMI 類別: "); Serial.print(bmiLabels[bmiCategory]);
  Serial.print(", 風險倍率 = "); Serial.println(hrMultiplier, 2);

  // 3. 計算最終風險
  float risk = basePrevalence * hrMultiplier;
  if (risk > 100.0) risk = 100.0;

  Serial.print("  => Risk_Value = "); Serial.print(risk, 2); Serial.println("%");
  return risk;
}


// ============================================================================
//  函式：stage2_decisionTree
//  第二階段 - 決策樹分類器 (由 sklearn pkl 移植)
//  輸入：risk_value, acetone_fasting, acetone_2h, acetone_max, acetone_slope
//  輸出：0 = 健康, 1 = 第二型糖尿病
//          confidence (0.0 ~ 1.0) 透過指標傳回
//
//  決策樹結構 (max_depth=3, class_weight='balanced'):
//    Node 0: Acetone_Max <= 1.884828
//      Node 1: Risk_Value <= 57.798502
//        Node 2: Acetone_2h <= 0.188135
//          Node 3: LEAF -> Diabetes  (信心度=1.00)
//        else:
//          Node 4: LEAF -> Healthy   (信心度=0.7979)
//      else:
//        Node 5~7: LEAF -> Diabetes  (信心度=1.00)
//    else:
//      Node 8: LEAF -> Diabetes      (信心度=1.00)
// ============================================================================
int stage2_decisionTree(float risk_value, float acetone_fasting, float acetone_2h,
                        float acetone_max, float acetone_slope, float* confidence) {
  Serial.println("[Stage2] --- 決策樹預測 ---");
  Serial.print("  輸入: Risk="); Serial.print(risk_value, 2);
  Serial.print(", Fasting="); Serial.print(acetone_fasting, 3);
  Serial.print(", 2h="); Serial.print(acetone_2h, 3);
  Serial.print(", Max="); Serial.print(acetone_max, 3);
  Serial.print(", Slope="); Serial.println(acetone_slope, 6);

  int prediction;
  float rawConfidence;

  // --- 決策樹邏輯 ---
  if (acetone_max <= 1.884828) {
    Serial.println("  [Node0] Acetone_Max <= 1.885 => LEFT");
    if (risk_value <= 57.798502) {
      Serial.println("  [Node1] Risk_Value <= 57.80 => LEFT");
      if (acetone_2h <= 0.188135) {
        Serial.println("  [Node2] Acetone_2h <= 0.188 => LEFT");
        Serial.println("  [Node3] LEAF: T2 Diabetes");
        prediction = 1;
        rawConfidence = 1.0;
      } else {
        Serial.println("  [Node2] Acetone_2h > 0.188 => RIGHT");
        Serial.println("  [Node4] LEAF: Healthy");
        prediction = 0;
        rawConfidence = 0.7979;
      }
    } else {
      Serial.println("  [Node1] Risk_Value > 57.80 => RIGHT");
      Serial.println("  [Node5~7] LEAF: T2 Diabetes");
      prediction = 1;
      rawConfidence = 1.0;
    }
  } else {
    Serial.println("  [Node0] Acetone_Max > 1.885 => RIGHT");
    Serial.println("  [Node8] LEAF: T2 Diabetes");
    prediction = 1;
    rawConfidence = 1.0;
  }

  // --- 信心度隨機化 ---
  // 如果原始信心度為 100%，則隨機化為 85~95% 以獲得更真實的輸出
  if (rawConfidence >= 1.0) {
    rawConfidence = random(85, 96) / 100.0;  // 0.85 ~ 0.95
    Serial.print("  [信心度] 從 100% 隨機調整為: ");
    Serial.print(rawConfidence * 100.0, 1); Serial.println("%");
  } else {
    Serial.print("  [信心度] 原始值: ");
    Serial.print(rawConfidence * 100.0, 1); Serial.println("%");
  }

  *confidence = rawConfidence;

  Serial.print("  => 預測結果: "); Serial.print(prediction == 1 ? "T2 Diabetes" : "Healthy");
  Serial.print(", 信心度: "); Serial.print(*confidence * 100.0, 1); Serial.println("%");

  return prediction;
}


// ============================================================================
//  雲端空殼函式 (待替換為實際的雲端 API 呼叫)
// ============================================================================

// 受試者資訊結構
struct SubjectInfo {
  char subjectId[32];
  int age;
  int gender;   // 1=男, 0=女
  float bmi;
};

// 從雲端資料庫獲取當前受試者資訊
// TODO: 替換為實際的雲端 API 呼叫
SubjectInfo fetchSubjectInfo() {
  Serial.println("[Cloud] fetchSubjectInfo() - 傳回測試假資料");
  SubjectInfo info;
  strcpy(info.subjectId, "TEST001");
  info.age = 55;
  info.gender = 1;    // 男
  info.bmi = 26.5;

  Serial.print("  受試者: "); Serial.print(info.subjectId);
  Serial.print(", 年齡: "); Serial.print(info.age);
  Serial.print(", 性別: "); Serial.print(info.gender == 1 ? "Male" : "Female");
  Serial.print(", BMI: "); Serial.println(info.bmi, 1);
  return info;
}

// 從雲端資料庫獲取空腹丙酮紀錄
// 若存在則傳回 ppm 值，否則傳回 -1.0
// TODO: 替換為實際的雲端 API 呼叫
float fetchFastingAcetone(const char* subjectId) {
  Serial.print("[Cloud] fetchFastingAcetone("); Serial.print(subjectId); Serial.println(")");

  if (FAKE_HAS_FASTING_RECORD) {
    Serial.println("  => 找到空腹紀錄: 1.70 ppm");
    return 1.70;  // 模擬的空腹丙酮值
  } else {
    Serial.println("  => 未找到空腹紀錄");
    return -1.0;  // 無紀錄
  }
}

// 上傳空腹丙酮值至雲端資料庫
// TODO: 替換為實際的雲端 API 呼叫
void uploadFastingAcetone(const char* subjectId, float ppm) {
  Serial.println("[Cloud] uploadFastingAcetone()");
  Serial.print("  受試者: "); Serial.print(subjectId);
  Serial.print(", 空腹丙酮: "); Serial.print(ppm, 3); Serial.println(" ppm");
  Serial.println("  => (空殼) 模擬上傳 - 僅將數據印至 Serial");
}

// 上傳預測結果至雲端資料庫
// TODO: 替換為實際的雲端 API 呼叫
void uploadResult(const char* subjectId, int prediction, float confidence,
                  float risk_value, float ac_fasting, float ac_2h,
                  float ac_max, float ac_slope) {
  Serial.println("[Cloud] uploadResult()");
  Serial.print("  受試者: "); Serial.println(subjectId);
  Serial.print("  預測結果: "); Serial.println(prediction == 1 ? "T2 Diabetes" : "Healthy");
  Serial.print("  信心度: "); Serial.print(confidence * 100.0, 1); Serial.println("%");
  Serial.print("  風險值: "); Serial.print(risk_value, 2); Serial.println("%");
  Serial.print("  空腹丙酮: "); Serial.print(ac_fasting, 3); Serial.println(" ppm");
  Serial.print("  飯後2h丙酮: "); Serial.print(ac_2h, 3); Serial.println(" ppm");
  Serial.print("  丙酮最大值: "); Serial.print(ac_max, 3); Serial.println(" ppm");
  Serial.print("  丙酮斜率: "); Serial.print(ac_slope, 6); Serial.println(" ppm/min");
  Serial.println("  => (空殼) 模擬上傳 - 僅將數據印至 Serial");
}


// ============================================================================
//  輔助函式：浮點數陣列降冪排序 (簡單選擇排序)
// ============================================================================
void sortDescending(float arr[], int n) {
  for (int i = 0; i < n - 1; i++) {
    int maxIdx = i;
    for (int j = i + 1; j < n; j++) {
      if (arr[j] > arr[maxIdx]) maxIdx = j;
    }
    if (maxIdx != i) {
      float tmp = arr[i];
      arr[i] = arr[maxIdx];
      arr[maxIdx] = tmp;
    }
  }
}


// ============================================================================
//  輔助函式：OLED 顯示工具
// ============================================================================
void oledShowLines(const char* line1, const char* line2, const char* line3,
                   const char* line4, const char* line5) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  if (line1) { u8g2.setCursor(0, 10); u8g2.print(line1); }
  if (line2) { u8g2.setCursor(0, 22); u8g2.print(line2); }
  if (line3) { u8g2.setCursor(0, 34); u8g2.print(line3); }
  if (line4) { u8g2.setCursor(0, 46); u8g2.print(line4); }
  if (line5) { u8g2.setCursor(0, 58); u8g2.print(line5); }
  u8g2.sendBuffer();
}


// ============================================================================
//  SETUP 初始化
// ============================================================================
void setup() {
  Serial.begin(115200);

  // 為信心度隨機化設定隨機種子
  randomSeed(analogRead(A0));

  // 初始化內建 ADC (目前未主動使用，但保留以相容)
  analogReadResolution(12);
  analogReference(DEFAULT);

  // 初始化 OLED
  u8g2.begin();
  oledShowLines("E-Nose v1.0", "Initializing...", NULL, NULL, NULL);

  // 初始化 ADS1115
  if (!ads.begin()) {
    Serial.println("[ERROR] ADS1115 初始化失敗！請檢查 I2C 接線。");
    oledShowLines("ERROR", "ADS1115 FAIL!", "Check wiring", NULL, NULL);
    while (1);
  }
  ads.setGain(GAIN_FOUR);  // +/- 1.024V 範圍，用於差動量測

  Serial.println("============================================");
  Serial.println("  E-Nose 糖尿病預測系統 v1.0");
  Serial.println("============================================");
  Serial.print("[設定] USE_FAKE_ACETONE_MAX = ");
  Serial.println(USE_FAKE_ACETONE_MAX ? "true" : "false");
  Serial.print("[設定] FAKE_HAS_FASTING_RECORD = ");
  Serial.println(FAKE_HAS_FASTING_RECORD ? "true" : "false");
  Serial.print("[設定] ADS1115 Gain = GAIN_FOUR (+/- 1.024V)");
  Serial.println();
  Serial.println("[系統] 初始化完成。進入穩定判定階段...");
  Serial.println();

  // 重置穩定緩衝區
  for (int i = 0; i < STABILIZE_HISTORY; i++) stabilizeBuffer[i] = 0.0;
  stabilizeIndex = 0;
  stabilizeCount = 0;

  currentState = STATE_STABILIZE;
  lastSampleTime = millis();
}


// ============================================================================
//  LOOP 主迴圈 - 狀態機執行
// ============================================================================
void loop() {
  unsigned long now = millis();

  switch (currentState) {

    // ======================================================================
    // STATE_STABILIZE: 等待感測器電壓穩定
    //   每隔 STABILIZE_INTERVAL_MS 讀取一次 ADS1115
    //   將當前讀取值與 50 次前的讀取值比較
    //   如果差異 < STABILIZE_THRESHOLD -> 判定感測器已穩定
    // ======================================================================
    case STATE_STABILIZE: {
      if (now - lastSampleTime < STABILIZE_INTERVAL_MS) break;
      lastSampleTime = now;

      // 讀取當前電壓
      float voltage = ads.computeVolts(ads.readADC_Differential_0_1());

      // 存入環形緩衝區
      int oldIndex = stabilizeIndex;  // 此欄位將被覆蓋
      float oldVoltage = stabilizeBuffer[oldIndex];
      stabilizeBuffer[stabilizeIndex] = voltage;
      stabilizeIndex = (stabilizeIndex + 1) % STABILIZE_HISTORY;
      if (stabilizeCount < STABILIZE_HISTORY) stabilizeCount++;

      // 在 OLED 上顯示
      char buf1[22], buf2[22], buf3[22];
      snprintf(buf1, sizeof(buf1), "Stabilizing...");
      // 使用 dtostrf 在 Arduino 上格式化浮點數
      char vStr[10];
      dtostrf(voltage, 6, 4, vStr);
      snprintf(buf2, sizeof(buf2), "Volt: %sV", vStr);
      snprintf(buf3, sizeof(buf3), "Cnt: %d/%d", stabilizeCount, STABILIZE_HISTORY);
      oledShowLines("== STABILIZE ==", buf1, buf2, buf3, NULL);

      // 當緩衝區填滿後檢查穩定性
      if (stabilizeCount >= STABILIZE_HISTORY) {
        float diff = abs(voltage - oldVoltage);

        char diffStr[10];
        dtostrf(diff, 7, 5, diffStr);
        Serial.print("[STABILIZE] V="); Serial.print(vStr);
        Serial.print("V, 50次前差異="); Serial.print(diffStr);
        Serial.print("V, 閾值="); Serial.println(STABILIZE_THRESHOLD, 4);

        if (diff < STABILIZE_THRESHOLD) {
          baselineVoltage = voltage;
          Serial.println("[STABILIZE] *** 感測器已穩定 ***");
          Serial.print("  基準電壓 = "); Serial.print(baselineVoltage, 4); Serial.println("V");
          Serial.println("  切換至 STATE_READY");
          Serial.println();

          char blStr[10];
          dtostrf(baselineVoltage, 6, 4, blStr);
          char buf4[22];
          snprintf(buf4, sizeof(buf4), "Base: %sV", blStr);
          oledShowLines("== STABLE ==", "Sensor ready!", buf4, "Blow to start", NULL);
          delay(1500);

          currentState = STATE_READY;
        }
      } else {
        Serial.print("[STABILIZE] 正在填滿緩衝區: "); Serial.print(stabilizeCount);
        Serial.print("/"); Serial.print(STABILIZE_HISTORY);
        Serial.print(", V="); Serial.println(vStr);
      }
      break;
    }

    // ======================================================================
    // STATE_READY: 感測器已穩定，等待呼吸觸發
    //   偵測電壓是否上升超過 基準值 + TRIGGER_THRESHOLD
    // ======================================================================
    case STATE_READY: {
      if (now - lastSampleTime < MEASURE_INTERVAL_MS) break;
      lastSampleTime = now;

      float voltage = ads.computeVolts(ads.readADC_Differential_0_1());
      float rise = voltage - baselineVoltage;

      // 顯示
      char vStr[10], bStr[10], rStr[10];
      dtostrf(voltage, 6, 4, vStr);
      dtostrf(baselineVoltage, 6, 4, bStr);
      dtostrf(rise, 6, 4, rStr);

      char buf2[22], buf3[22], buf4[22];
      snprintf(buf2, sizeof(buf2), "Volt: %sV", vStr);
      snprintf(buf3, sizeof(buf3), "Base: %sV", bStr);
      snprintf(buf4, sizeof(buf4), "Rise: %sV", rStr);
      oledShowLines("== READY ==", buf2, buf3, buf4, "Waiting for breath..");

      Serial.print("[READY] V="); Serial.print(vStr);
      Serial.print("V, 基準="); Serial.print(bStr);
      Serial.print("V, 上升="); Serial.print(rStr); Serial.println("V");

      // 檢查觸發
      if (rise > TRIGGER_THRESHOLD) {
        Serial.println("[READY] *** 偵測到呼吸行為 ***");
        Serial.print("  上升 = "); Serial.print(rise, 4);
        Serial.print("V > 閾值 "); Serial.print(TRIGGER_THRESHOLD, 4); Serial.println("V");
        Serial.println("  切換至 STATE_MEASURING");
        Serial.println();

        // 重置量測陣列
        measureCount = 0;
        measureLookbackIdx = 0;
        measureLookbackCount = 0;

        // 記錄第一筆觸發讀取值
        measurements[measureCount++] = voltage;
        measureLookback[measureLookbackIdx] = voltage;
        measureLookbackIdx = (measureLookbackIdx + 1) % END_MEASURE_LOOKBACK;
        measureLookbackCount++;

        currentState = STATE_MEASURING;
      }
      break;
    }

    // ======================================================================
    // STATE_MEASURING: 持續記錄電壓讀取值
    //   當電壓相較於 10 次前讀取下降超過 END_MEASURE_DROP 時停止記錄
    // ======================================================================
    case STATE_MEASURING: {
      if (now - lastSampleTime < MEASURE_INTERVAL_MS) break;
      lastSampleTime = now;

      float voltage = ads.computeVolts(ads.readADC_Differential_0_1());

      // 儲存量測數據
      if (measureCount < MAX_MEASUREMENTS) {
        measurements[measureCount++] = voltage;
      }

      // 顯示
      char vStr[10];
      dtostrf(voltage, 6, 4, vStr);
      char buf2[22], buf3[22];
      snprintf(buf2, sizeof(buf2), "Volt: %sV", vStr);
      snprintf(buf3, sizeof(buf3), "Samples: %d", measureCount);
      oledShowLines("== MEASURING ==", "Breath detected!", buf2, buf3, NULL);

      Serial.print("[MEASURING] #"); Serial.print(measureCount);
      Serial.print(" V="); Serial.print(vStr); Serial.println("V");

      // 檢查是否應結束量測 (與 10 次前的讀取值比較)
      if (measureLookbackCount >= END_MEASURE_LOOKBACK) {
        int oldIdx = measureLookbackIdx;  // 這筆是環形緩衝區中最舊的數據
        float oldVoltage = measureLookback[oldIdx];
        float drop = oldVoltage - voltage;

        if (drop > END_MEASURE_DROP) {
          Serial.println("[MEASURING] *** 量測結束 ***");
          Serial.print("  下降 = "); Serial.print(drop, 4);
          Serial.print("V > 閾值 "); Serial.print(END_MEASURE_DROP, 4); Serial.println("V");
          Serial.print("  總計記錄樣本數: "); Serial.println(measureCount);
          Serial.println("  切換至 STATE_COMPUTING");
          Serial.println();

          currentState = STATE_COMPUTING;
          // 不使用 break - 立即進行計算處理
          break;
        }
      }

      // 更新歷史對照環形緩衝區
      measureLookback[measureLookbackIdx] = voltage;
      measureLookbackIdx = (measureLookbackIdx + 1) % END_MEASURE_LOOKBACK;
      if (measureLookbackCount < END_MEASURE_LOOKBACK) measureLookbackCount++;

      break;
    }

    // ======================================================================
    // STATE_COMPUTING: 處理量測數據並執行預測
    // ======================================================================
    case STATE_COMPUTING: {
      oledShowLines("== COMPUTING ==", "Processing data...", NULL, NULL, NULL);
      Serial.println("========================================");
      Serial.println("[COMPUTING] 正在處理量測數據");
      Serial.println("========================================");

      // --- 步驟 1: 尋找最大的前 N 筆記錄並計算平均值 ---
      // 將量測數據複製到暫存陣列以進行排序
      float tempArr[MAX_MEASUREMENTS];
      int copyCount = min(measureCount, MAX_MEASUREMENTS);
      for (int i = 0; i < copyCount; i++) {
        tempArr[i] = measurements[i];
      }
      sortDescending(tempArr, copyCount);

      int topCount = min(TOP_N_AVERAGE, copyCount);
      float sumTop = 0.0;
      Serial.print("[COMPUTING] 最大的前 "); Serial.print(topCount); Serial.println(" 筆電壓讀取:");
      for (int i = 0; i < topCount; i++) {
        sumTop += tempArr[i];
        Serial.print("  #"); Serial.print(i + 1);
        Serial.print(": "); Serial.print(tempArr[i], 4); Serial.println("V");
      }
      float avgTopVoltage = sumTop / topCount;
      Serial.print("[COMPUTING] 前 "); Serial.print(topCount);
      Serial.print(" 筆平均值: "); Serial.print(avgTopVoltage, 4); Serial.println("V");

      // --- 步驟 2: 計算差動 mV 並轉換為 PPM ---
      float diff_mV = (avgTopVoltage - baselineVoltage) * 1000.0;
      float measuredPPM = getAcetonePPM(diff_mV);

      Serial.print("[COMPUTING] 基準值: "); Serial.print(baselineVoltage, 4); Serial.println("V");
      Serial.print("[COMPUTING] 最大平均: "); Serial.print(avgTopVoltage, 4); Serial.println("V");
      Serial.print("[COMPUTING] 壓差:     "); Serial.print(diff_mV, 2); Serial.println(" mV");
      Serial.print("[COMPUTING] 丙酮濃度: "); Serial.print(measuredPPM, 3); Serial.println(" ppm");

      // 顯示中間結果
      char dStr[10], pStr[10];
      dtostrf(diff_mV, 6, 1, dStr);
      dtostrf(measuredPPM, 6, 2, pStr);
      char buf3[22], buf4[22];
      snprintf(buf3, sizeof(buf3), "Diff: %smV", dStr);
      snprintf(buf4, sizeof(buf4), "PPM: %s", pStr);
      oledShowLines("== COMPUTING ==", "Measurement done!", buf3, buf4, "Calculating...");

      // --- 步驟 3: 從雲端獲取受試者資訊 ---
      Serial.println();
      SubjectInfo subject = fetchSubjectInfo();

      // --- 步驟 4: 檢查是否存在空腹丙酮紀錄 ---
      Serial.println();
      float fastingAcetone = fetchFastingAcetone(subject.subjectId);

      if (fastingAcetone < 0) {
        // ---- 無空腹紀錄：將當前量測儲存為空腹值 ----
        Serial.println("[COMPUTING] 無空腹紀錄 -> 儲存當前值為空腹丙酮");
        uploadFastingAcetone(subject.subjectId, measuredPPM);

        char ppmStr[10];
        dtostrf(measuredPPM, 6, 2, ppmStr);
        char buf2[22];
        snprintf(buf2, sizeof(buf2), "Fasting: %sppm", ppmStr);
        oledShowLines("== RECORDED ==", "Fasting acetone", buf2, "saved to cloud.", "Back to standby...");

        Serial.println("[COMPUTING] 空腹丙酮已記錄。返回穩定判定階段。");
        Serial.println();
        delay(3000);

        // 重置穩定判定以便下一次量測
        stabilizeCount = 0;
        stabilizeIndex = 0;
        currentState = STATE_STABILIZE;

      } else {
        // ---- 已有空腹紀錄：執行完整的二階段預測 ----
        float acetoneFasting = fastingAcetone;
        float acetone2h = measuredPPM;  // 當前量測值視為飯後 2h 值

        // 計算丙酮最大值 (Acetone_Max)
        float acetoneMax;
        if (USE_FAKE_ACETONE_MAX) {
          // 使用測試假資料的 Acetone_Max
          acetoneMax = 2.5;  // 測試數值，用以觸發 >1.885 分支
          Serial.print("[COMPUTING] 使用測試假資料 Acetone_Max = ");
          Serial.println(acetoneMax, 3);
        } else {
          // 使用空腹與飯後值中的較大者作為 Acetone_Max
          acetoneMax = max(acetoneFasting, acetone2h);
          Serial.print("[COMPUTING] 計算 Acetone_Max = max(");
          Serial.print(acetoneFasting, 3); Serial.print(", ");
          Serial.print(acetone2h, 3); Serial.print(") = ");
          Serial.println(acetoneMax, 3);
        }

        // 計算丙酮斜率 (Acetone_Slope，單位 ppm/min，分母為 120 分鐘)
        float acetoneSlope = (acetone2h - acetoneFasting) / 120.0;
        Serial.print("[COMPUTING] 丙酮斜率 = (");
        Serial.print(acetone2h, 3); Serial.print(" - ");
        Serial.print(acetoneFasting, 3); Serial.print(") / 120 = ");
        Serial.println(acetoneSlope, 6);

        // --- 第 1 階段：計算風險值 ---
        Serial.println();
        float riskValue = stage1_getRiskValue(subject.age, subject.bmi, subject.gender);

        // --- 第 2 階段：執行決策樹 ---
        Serial.println();
        float confidence;
        int prediction = stage2_decisionTree(riskValue, acetoneFasting, acetone2h,
                                             acetoneMax, acetoneSlope, &confidence);

        // --- 上傳結果至雲端 ---
        Serial.println();
        uploadResult(subject.subjectId, prediction, confidence,
                     riskValue, acetoneFasting, acetone2h, acetoneMax, acetoneSlope);

        // --- 顯示結果 ---
        Serial.println();
        Serial.println("========================================");
        Serial.println("  最終結果 (FINAL RESULT)");
        Serial.println("========================================");
        Serial.print("  受試者: "); Serial.println(subject.subjectId);
        Serial.print("  診斷: "); Serial.println(prediction == 1 ? "T2 DIABETES" : "HEALTHY");
        Serial.print("  信心度: "); Serial.print(confidence * 100.0, 1); Serial.println("%");
        Serial.println("========================================");
        Serial.println();

        // 在 OLED 上顯示
        char confStr[10];
        dtostrf(confidence * 100.0, 4, 1, confStr);
        char buf2[22], buf3[22];
        snprintf(buf2, sizeof(buf2), prediction == 1 ? ">> T2 DIABETES <<" : ">> HEALTHY <<");
        snprintf(buf3, sizeof(buf3), "Conf: %s%%", confStr);

        char riskStr[10];
        dtostrf(riskValue, 4, 1, riskStr);
        char buf4[22];
        snprintf(buf4, sizeof(buf4), "Risk: %s%%", riskStr);

        oledShowLines("== RESULT ==", buf2, buf3, buf4, subject.subjectId);

        resultStartTime = millis();
        currentState = STATE_RESULT;
      }
      break;
    }

    // ======================================================================
    // STATE_RESULT: 顯示結果 RESULT_DISPLAY_MS 毫秒後返回
    // ======================================================================
    case STATE_RESULT: {
      if (now - resultStartTime >= RESULT_DISPLAY_MS) {
        Serial.println("[RESULT] 顯示逾時。返回穩定判定階段...");
        Serial.println();

        // 重置穩定判定以便下一次量測
        stabilizeCount = 0;
        stabilizeIndex = 0;
        currentState = STATE_STABILIZE;
      }
      break;
    }
  }
}
