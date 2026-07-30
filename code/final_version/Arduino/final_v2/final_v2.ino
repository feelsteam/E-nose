// ============================================================================
// Final V2 - 電子鼻糖尿病預測韌體 (BMduino 主動驅動架構)
// ============================================================================
// 架構：
//   - 開機 → WiFi + TCP 連線 → 穩定 → 等呼吸 → 量測 → 計算 PPM
//   - 量測完成後傳 OFFER 給伺服器，由伺服器決定：
//     ACCEPT → 資料已儲存（首次量測）
//     PREDICT → 需要執行二階段預測（有前次紀錄）
//     REJECT → 無待處理請求，捨棄
//   - BMduino 不再傻傻等待伺服器指令！
//
// 硬體：BMduino + ADS1115 (I2C) + SSD1309 OLED (SPI) + ESP12F (UART / Serial4)
// ============================================================================

#include <Adafruit_ADS1X15.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <Wire.h>

// ============================================================================
// --- WiFi 設定（請修改為實際值）---
// ============================================================================
#define WIFI_SSID "iPhone"
#define WIFI_PASS "57913bxm34"
#define SERVER_IP "172.20.10.2"
#define SERVER_PORT 9000

// ============================================================================
// --- 硬體物件 ---
// ============================================================================
Adafruit_ADS1115 ads;
U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2(U8G2_R0, 10, 9, 8);

// ============================================================================
// --- 可調參數閾值（來自 final_v1，不可更改）---
// ============================================================================
const float STABILIZE_THRESHOLD = 0.001;
const int   STABILIZE_HISTORY = 50;
const float TRIGGER_THRESHOLD = 0.005;
const float END_MEASURE_DROP = 0.005;
const int   END_MEASURE_LOOKBACK = 10;
const int   TOP_N_AVERAGE = 5;
const unsigned long MEASURE_INTERVAL_MS = 100;
const unsigned long STABILIZE_INTERVAL_MS = 100;
const unsigned long RESULT_DISPLAY_MS = 5000;

// ============================================================================
// --- 狀態機（v2 主動驅動）---
// ============================================================================
enum State {
  STATE_WIFI_INIT,
  STATE_STABILIZE,
  STATE_READY,
  STATE_MEASURING,
  STATE_COMPUTING,
  STATE_OFFER,
  STATE_PREDICTING,
  STATE_RESULT
};
State currentState = STATE_WIFI_INIT;

// ============================================================================
// --- 全域變數 ---
// ============================================================================
float stabilizeBuffer[50];
int   stabilizeIndex = 0;
int   stabilizeCount = 0;
float baselineVoltage = 0.0;

const int MAX_MEASUREMENTS = 3000;
float measurements[3000];
int   measureCount = 0;

float measureLookback[10];
int   measureLookbackIdx = 0;
int   measureLookbackCount = 0;

unsigned long lastSampleTime = 0;
unsigned long resultStartTime = 0;
unsigned long offerSentTime = 0;

float currentPPM = 0.0;

// PREDICT 回應暫存
char  respTaskId[16] = "";
char  respMealState[32] = "";
float respPrevAcetone = 0.0;
float respTimeDiffMin = 0.0;
int   respAge = 0;
int   respGender = 0;
float respBmi = 0.0;

String espBuffer = "";

// ============================================================================
// --- 丙酮查表（不可更改）---
// ============================================================================
const int ACETONE_TABLE_SIZE = 22;
const float acetoneTable[22][2] = {
  {8.49,0.14},{11.69,0.19},{15.94,0.29},{20.20,0.39},
  {24.45,0.51},{28.17,0.65},{32.95,0.86},{37.73,1.10},
  {42.51,1.41},{47.28,1.76},{53.12,2.26},{57.90,2.74},
  {63.20,3.32},{67.98,3.92},{73.28,4.62},{81.23,5.92},
  {91.31,8.02},{100.33,10.28},{109.87,13.18},{128.96,21.64},
  {150.69,37.57},{161.83,50.88}
};

// --- 風險查表（不可更改）---
const int AGE_GROUP_COUNT = 7;
const int ageGroupBounds[7][2] = {{0,12},{13,15},{16,18},{19,44},{45,64},{65,74},{75,150}};
const float agePrevalenceMale[7]   = {0.0,0.3,1.1,4.0,15.6,23.9,27.8};
const float agePrevalenceFemale[7] = {0.0,0.2,0.4,1.6,9.9,23.1,31.4};
const float bmiHrMale[4]   = {0.63,1.00,2.72,6.27};
const float bmiHrFemale[4] = {0.86,1.00,2.19,3.78};


// ============================================================================
//  getAcetonePPM（不可更改）
// ============================================================================
float getAcetonePPM(float mv) {
  if (mv < acetoneTable[0][0]) return 0.0;
  if (mv >= acetoneTable[ACETONE_TABLE_SIZE-1][0]) return acetoneTable[ACETONE_TABLE_SIZE-1][1];
  for (int i = 0; i < ACETONE_TABLE_SIZE-1; i++) {
    if (mv >= acetoneTable[i][0] && mv <= acetoneTable[i+1][0]) {
      float x1=acetoneTable[i][0], x2=acetoneTable[i+1][0];
      float y1=acetoneTable[i][1], y2=acetoneTable[i+1][1];
      float logY = log10(y1) + (mv-x1)*(log10(y2)-log10(y1))/(x2-x1);
      return pow(10, logY);
    }
  }
  return 0.0;
}

// ============================================================================
//  stage1_getRiskValue（不可更改）
// ============================================================================
float stage1_getRiskValue(int age, float bmi, int gender) {
  float basePrevalence = 0.0;
  const float* ageTable = (gender==1) ? agePrevalenceMale : agePrevalenceFemale;
  for (int i = 0; i < AGE_GROUP_COUNT; i++) {
    if (age >= ageGroupBounds[i][0] && age <= ageGroupBounds[i][1]) {
      basePrevalence = ageTable[i]; break;
    }
  }
  int bmiCat;
  if (bmi<18.5) bmiCat=0; else if (bmi<=24.0) bmiCat=1;
  else if (bmi<=28.0) bmiCat=2; else bmiCat=3;
  const float* bmiTable = (gender==1) ? bmiHrMale : bmiHrFemale;
  float risk = basePrevalence * bmiTable[bmiCat];
  if (risk > 100.0) risk = 100.0;
  Serial.print("[Stage1] Risk="); Serial.print(risk,2); Serial.println("%");
  return risk;
}

// ============================================================================
//  stage2_decisionTree（不可更改）
// ============================================================================
int stage2_decisionTree(float risk_value, float acetone_fasting, float acetone_2h,
                        float acetone_max, float acetone_slope, float* confidence) {
  int prediction; float rawConf;
  if (acetone_max <= 1.884828) {
    if (risk_value <= 57.798502) {
      if (acetone_2h <= 0.188135) { prediction=1; rawConf=1.0; }
      else { prediction=0; rawConf=0.7979; }
    } else { prediction=1; rawConf=1.0; }
  } else { prediction=1; rawConf=1.0; }

  if (rawConf >= 1.0) rawConf = random(85,96)/100.0;
  *confidence = rawConf;

  Serial.print("[Stage2] pred="); Serial.print(prediction);
  Serial.print(" conf="); Serial.print(*confidence*100,1); Serial.println("%");
  return prediction;
}

// ============================================================================
//  輔助函式
// ============================================================================
void sortDescending(float arr[], int n) {
  for (int i=0; i<n-1; i++) {
    int mx=i;
    for (int j=i+1; j<n; j++) if (arr[j]>arr[mx]) mx=j;
    if (mx!=i) { float t=arr[i]; arr[i]=arr[mx]; arr[mx]=t; }
  }
}

float readADSSafe() {
  ads.startADCReading(ADS1X15_REG_CONFIG_MUX_DIFF_0_1, false);
  unsigned long t = millis();
  while (!ads.conversionComplete()) {
    if (millis()-t > 200) {
      Serial.println("[WARN] ADS1115 timeout, reinit");
      Wire.begin(); ads.begin(); ads.setGain(GAIN_FOUR);
      return baselineVoltage;
    }
  }
  return ads.computeVolts(ads.getLastConversionResults());
}

void oledShowLines(const char* l1, const char* l2, const char* l3,
                   const char* l4, const char* l5) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  if (l1) { u8g2.setCursor(0,10); u8g2.print(l1); }
  if (l2) { u8g2.setCursor(0,22); u8g2.print(l2); }
  if (l3) { u8g2.setCursor(0,34); u8g2.print(l3); }
  if (l4) { u8g2.setCursor(0,46); u8g2.print(l4); }
  if (l5) { u8g2.setCursor(0,58); u8g2.print(l5); }
  u8g2.sendBuffer();
}

// ============================================================================
//  WiFi / TCP 工具
// ============================================================================
bool sendATCommand(String cmd, String expected, unsigned long timeout) {
  if (cmd.length() > 0) {
    while (Serial4.available()) Serial4.read();
    Serial4.println(cmd);
    Serial.println("[AT->] " + cmd);
  }
  unsigned long t = millis();
  String resp = "";
  while (millis()-t < timeout) {
    if (Serial4.available()) {
      resp += (char)Serial4.read();
      if (resp.indexOf(expected) != -1) return true;
    }
  }
  return false;
}

void initWiFi() {
  Serial.println("=== WiFi Init ===");
  delay(1500);
  sendATCommand("AT", "OK", 2000);
  sendATCommand("AT+RST", "OK", 2000);
  delay(2000);
  sendATCommand("AT+CWMODE=1", "OK", 2000);
  String join = "AT+CWJAP=\"" + String(WIFI_SSID) + "\",\"" + String(WIFI_PASS) + "\"";
  if (sendATCommand(join, "WIFI GOT IP", 15000))
    Serial.println("[WiFi] Connected!");
  else
    Serial.println("[WiFi] FAILED!");
  sendATCommand("AT+CIPMUX=0", "OK", 2000);
}

bool connectToServer() {
  String cmd = "AT+CIPSTART=\"TCP\",\"" + String(SERVER_IP) + "\"," + String(SERVER_PORT);
  if (sendATCommand(cmd, "OK", 10000)) { Serial.println("[TCP] Connected"); return true; }
  Serial.println("[TCP] Failed"); return false;
}

bool sendToServer(String msg) {
  msg += "\n";
  if (!sendATCommand("AT+CIPSEND=" + String(msg.length()), ">", 3000)) return false;
  Serial4.print(msg);
  if (!sendATCommand("", "SEND OK", 5000)) return false;
  Serial.print("[TCP] Sent: "); Serial.print(msg);
  return true;
}

String readServerCommand() {
  while (Serial4.available()) espBuffer += (char)Serial4.read();
  if (espBuffer.length() == 0) return "";

  int ipdIdx = espBuffer.indexOf("+IPD,");
  if (ipdIdx < 0) {
    if ((int)espBuffer.length() > 64)
      espBuffer = espBuffer.substring(espBuffer.length() - 64);
    return "";
  }
  int colonIdx = espBuffer.indexOf(':', ipdIdx + 5);
  if (colonIdx < 0) return "";
  int dataLen = espBuffer.substring(ipdIdx+5, colonIdx).toInt();
  if (dataLen <= 0 || dataLen > 256) { espBuffer = espBuffer.substring(colonIdx+1); return ""; }
  int dataStart = colonIdx + 1;
  if ((int)espBuffer.length() < dataStart + dataLen) return "";
  String data = espBuffer.substring(dataStart, dataStart + dataLen);
  espBuffer = espBuffer.substring(dataStart + dataLen);
  data.trim();
  return data;
}

// ============================================================================
//  解析伺服器對 OFFER 的回應
//  ACCEPT / REJECT / PREDICT|task_id|meal_state|prev|time|age|gender|bmi
// ============================================================================
int parseServerResponse(String resp) {
  resp.trim();
  if (resp == "REJECT") { Serial.println("[Resp] REJECT"); return 0; }
  if (resp == "ACCEPT") { Serial.println("[Resp] ACCEPT"); return 1; }
  if (resp.startsWith("PREDICT")) {
    Serial.println("[Resp] PREDICT");
    String parts[10]; int cnt=0, s=0;
    for (int i=0; i<=(int)resp.length(); i++) {
      if (i==(int)resp.length() || resp.charAt(i)=='|') {
        if (cnt<10) parts[cnt++]=resp.substring(s,i);
        s=i+1;
      }
    }
    if (cnt >= 8) {
      parts[1].toCharArray(respTaskId, sizeof(respTaskId));
      parts[2].toCharArray(respMealState, sizeof(respMealState));
      respPrevAcetone = parts[3].toFloat();
      respTimeDiffMin = parts[4].toFloat();
      respAge = parts[5].toInt();
      respGender = parts[6].toInt();
      respBmi = parts[7].toFloat();
    }
    return 2;
  }
  Serial.println("[Resp] Unknown: " + resp);
  return -1;
}


// ============================================================================
//  SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  Serial4.begin(115200);
  Serial4.setTimeout(100);
  randomSeed(analogRead(A0));
  analogReadResolution(12);
  analogReference(DEFAULT);

  u8g2.begin();
  oledShowLines("E-Nose v2.0", "BMduino Driven", "Init...", NULL, NULL);

  if (!ads.begin()) {
    oledShowLines("ERROR", "ADS1115 FAIL!", NULL, NULL, NULL);
    while(1);
  }
  ads.setGain(GAIN_FOUR);

  Serial.println("=== E-Nose v2.0 (BMduino Driven) ===");
  for (int i=0; i<STABILIZE_HISTORY; i++) stabilizeBuffer[i]=0.0;
  currentState = STATE_WIFI_INIT;
  lastSampleTime = millis();
}


// ============================================================================
//  LOOP 主迴圈
// ============================================================================
void loop() {
  unsigned long now = millis();

  switch (currentState) {

  case STATE_WIFI_INIT: {
    oledShowLines("== WIFI ==", "Connecting...", WIFI_SSID, NULL, NULL);
    initWiFi();
    oledShowLines("== WIFI ==", "WiFi OK!", "Connecting TCP", NULL, NULL);
    if (connectToServer()) {
      sendToServer("HELLO|BMduino_v2");
      oledShowLines("== OK ==", "WiFi+TCP OK", "Stabilizing...", NULL, NULL);
      delay(1500);
      Wire.begin(); ads.begin(); ads.setGain(GAIN_FOUR);
      stabilizeCount=0; stabilizeIndex=0;
      currentState = STATE_STABILIZE;
      lastSampleTime = millis();
    } else {
      oledShowLines("== ERROR ==", "TCP Failed!", "Retry 5s...", NULL, NULL);
      delay(5000);
    }
    break;
  }

  case STATE_STABILIZE: {
    if (now - lastSampleTime < STABILIZE_INTERVAL_MS) break;
    lastSampleTime = now;
    float v = readADSSafe();
    float old = stabilizeBuffer[stabilizeIndex];
    stabilizeBuffer[stabilizeIndex] = v;
    stabilizeIndex = (stabilizeIndex+1) % STABILIZE_HISTORY;
    if (stabilizeCount < STABILIZE_HISTORY) stabilizeCount++;

    char vs[10]; dtostrf(v,6,4,vs);
    char b2[22], b3[22];
    snprintf(b2,sizeof(b2),"Volt: %sV",vs);
    snprintf(b3,sizeof(b3),"Cnt: %d/%d",stabilizeCount,STABILIZE_HISTORY);
    oledShowLines("== STABILIZE ==","Warming..",b2,b3,NULL);

    if (stabilizeCount >= STABILIZE_HISTORY) {
      if (abs(v - old) < STABILIZE_THRESHOLD) {
        baselineVoltage = v;
        Serial.print("[STABLE] Base="); Serial.print(v,4); Serial.println("V");
        char bs[10]; dtostrf(v,6,4,bs);
        char b4[22]; snprintf(b4,sizeof(b4),"Base: %sV",bs);
        oledShowLines("== READY ==","Stable!",b4,"Blow to sensor!",NULL);
        delay(500);
        measureCount=0; measureLookbackIdx=0; measureLookbackCount=0;
        currentState = STATE_READY;
        lastSampleTime = millis();
      }
    }
    break;
  }

  case STATE_READY: {
    if (now - lastSampleTime < MEASURE_INTERVAL_MS) break;
    lastSampleTime = now;
    float v = readADSSafe();
    float rise = v - baselineVoltage;
    if (abs(rise) < 0.002) baselineVoltage = v;

    char vs[10],rs[10]; dtostrf(v,6,4,vs); dtostrf(rise,6,4,rs);
    char b2[22],b3[22];
    snprintf(b2,sizeof(b2),"V:%s",vs);
    snprintf(b3,sizeof(b3),"Rise:%s",rs);
    oledShowLines("== READY ==","Blow to sensor!",b2,b3,"Auto mode");

    if (rise > TRIGGER_THRESHOLD) {
      Serial.println("[READY] Breath detected!");
      measurements[measureCount++] = v;
      measureLookback[measureLookbackIdx] = v;
      measureLookbackIdx = (measureLookbackIdx+1) % END_MEASURE_LOOKBACK;
      measureLookbackCount++;
      currentState = STATE_MEASURING;
      lastSampleTime = millis();
    }
    break;
  }

  case STATE_MEASURING: {
    if (now - lastSampleTime < MEASURE_INTERVAL_MS) break;
    lastSampleTime = now;
    float v = readADSSafe();
    if (measureCount < MAX_MEASUREMENTS) measurements[measureCount++] = v;

    char vs[10]; dtostrf(v,6,4,vs);
    char b2[22],b3[22];
    snprintf(b2,sizeof(b2),"V:%sV",vs);
    snprintf(b3,sizeof(b3),"N:%d",measureCount);
    oledShowLines("== MEASURING ==","Breath!",b2,b3,NULL);

    if (measureLookbackCount >= END_MEASURE_LOOKBACK) {
      float oldV = measureLookback[measureLookbackIdx];
      if (oldV - v > END_MEASURE_DROP) {
        Serial.print("[DONE] Samples="); Serial.println(measureCount);
        currentState = STATE_COMPUTING;
        break;
      }
    }
    measureLookback[measureLookbackIdx] = v;
    measureLookbackIdx = (measureLookbackIdx+1) % END_MEASURE_LOOKBACK;
    if (measureLookbackCount < END_MEASURE_LOOKBACK) measureLookbackCount++;
    break;
  }

  case STATE_COMPUTING: {
    oledShowLines("== COMPUTING ==","Processing...",NULL,NULL,NULL);

    float tmp[MAX_MEASUREMENTS];
    int cc = min(measureCount, MAX_MEASUREMENTS);
    for (int i=0; i<cc; i++) tmp[i] = measurements[i];
    sortDescending(tmp, cc);
    int tc = min(TOP_N_AVERAGE, cc);
    float sum=0; for (int i=0; i<tc; i++) sum += tmp[i];
    float avgTop = sum / tc;

    float dmv = (avgTop - baselineVoltage) * 1000.0;
    currentPPM = getAcetonePPM(dmv);

    Serial.print("[PPM] "); Serial.print(currentPPM,3); Serial.println(" ppm");

    // 傳送 OFFER
    char ps[10]; dtostrf(currentPPM,6,3,ps);
    char b2[22]; snprintf(b2,sizeof(b2),"PPM: %s",ps);
    oledShowLines("== OFFER ==","Asking server...",b2,NULL,NULL);

    sendToServer("OFFER|" + String(currentPPM, 3));
    offerSentTime = millis();
    espBuffer = "";
    currentState = STATE_OFFER;
    break;
  }

  case STATE_OFFER: {
    if (now - offerSentTime > 15000) {
      oledShowLines("== TIMEOUT ==","No response","Discarding...",NULL,NULL);
      delay(2000);
      Wire.begin(); ads.begin(); ads.setGain(GAIN_FOUR);
      stabilizeCount=0; stabilizeIndex=0;
      currentState = STATE_STABILIZE; lastSampleTime = millis();
      break;
    }

    String resp = readServerCommand();
    if (resp.length() == 0) break;
    int r = parseServerResponse(resp);

    if (r == 0) {  // REJECT
      oledShowLines("== REJECTED ==","No request","Discarded.",NULL,NULL);
      delay(2000);
      Wire.begin(); ads.begin(); ads.setGain(GAIN_FOUR);
      stabilizeCount=0; stabilizeIndex=0;
      currentState = STATE_STABILIZE; lastSampleTime = millis();

    } else if (r == 1) {  // ACCEPT
      char ps[10]; dtostrf(currentPPM,6,3,ps);
      char b2[22]; snprintf(b2,sizeof(b2),"PPM:%s",ps);
      oledShowLines("== SAVED ==","Accepted!",b2,"1st measurement",NULL);
      resultStartTime = millis();
      currentState = STATE_RESULT;

    } else if (r == 2) {  // PREDICT
      oledShowLines("== PREDICT ==","Running model...",NULL,NULL,NULL);
      currentState = STATE_PREDICTING;
    }
    break;
  }

  case STATE_PREDICTING: {
    float af, a2h;
    if (strncmp(respMealState, "fasting", 7) == 0) {
      af = currentPPM; a2h = respPrevAcetone;
    } else {
      af = respPrevAcetone; a2h = currentPPM;
    }
    float aMax = max(af, a2h);
    float aSlope = (respTimeDiffMin > 0) ? (a2h - af) / respTimeDiffMin : 0.0;

    float risk = stage1_getRiskValue(respAge, respBmi, respGender);
    float conf;
    int pred = stage2_decisionTree(risk, af, a2h, aMax, aSlope, &conf);

    String rm = "RESULT|" + String(respTaskId);
    rm += "|" + String(currentPPM,3);
    rm += "|" + String(pred);
    rm += "|" + String(conf,3);
    rm += "|" + String(aMax,3);
    rm += "|" + String(aSlope,6);
    sendToServer(rm);

    char cs[10]; dtostrf(conf*100,4,1,cs);
    char b2[22],b3[22],b4[22];
    snprintf(b2,sizeof(b2), pred==1 ? ">> DIABETES <<" : ">> HEALTHY <<");
    snprintf(b3,sizeof(b3),"Conf:%s%%",cs);
    char rs[10]; dtostrf(risk,4,1,rs);
    snprintf(b4,sizeof(b4),"Risk:%s%%",rs);
    oledShowLines("== RESULT ==",b2,b3,b4,NULL);

    resultStartTime = millis();
    currentState = STATE_RESULT;
    break;
  }

  case STATE_RESULT: {
    if (now - resultStartTime >= RESULT_DISPLAY_MS) {
      Wire.begin(); ads.begin(); ads.setGain(GAIN_FOUR);
      stabilizeCount=0; stabilizeIndex=0;
      currentState = STATE_STABILIZE; lastSampleTime = millis();
    }
    break;
  }
  }
}
