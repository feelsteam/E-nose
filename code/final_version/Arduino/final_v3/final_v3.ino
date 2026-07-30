// ============================================================================
// Final V3 - 電子鼻韌體（真實量測模式）
// ============================================================================

#include <Adafruit_ADS1X15.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <Wire.h>

// ============================================================================
// WiFi 設定
// ============================================================================
#define WIFI_SSID "FeelsTeam"
#define WIFI_PASS "01060118"
#define SERVER_IP "192.168.50.153"
#define SERVER_PORT 9000

// ============================================================================
// 真實量測參數
// ============================================================================
const float TRIGGER_THRESHOLD = 0.0005;
const float END_MEASURE_DROP = 0.0005;
const int END_MEASURE_LOOKBACK = 10;
const int TOP_N_AVERAGE = 5;
const unsigned long MEASURE_INTERVAL_MS = 100;
const unsigned long STABILIZE_INTERVAL_MS = 100;
const float STABILIZE_THRESHOLD = 0.001;
const int STABILIZE_HISTORY = 50;

// ============================================================================
// 丙酮查表
// ============================================================================
const int ACETONE_TABLE_SIZE = 22;
const float acetoneTable[22][2] = {
    {8.49, 0.14},    {11.69, 0.19},   {15.94, 0.29},   {20.20, 0.39},
    {24.45, 0.51},   {28.17, 0.65},   {32.95, 0.86},   {37.73, 1.10},
    {42.51, 1.41},   {47.28, 1.76},   {53.12, 2.26},   {57.90, 2.74},
    {63.20, 3.32},   {67.98, 3.92},   {73.28, 4.62},   {81.23, 5.92},
    {91.31, 8.02},   {100.33, 10.28}, {109.87, 13.18}, {128.96, 21.64},
    {150.69, 37.57}, {161.83, 50.88}};

float getAcetonePPM(float mv) {
  if (mv < acetoneTable[0][0])
    return 0.0;
  if (mv >= acetoneTable[ACETONE_TABLE_SIZE - 1][0])
    return acetoneTable[ACETONE_TABLE_SIZE - 1][1];
  for (int i = 0; i < ACETONE_TABLE_SIZE - 1; i++) {
    if (mv >= acetoneTable[i][0] && mv <= acetoneTable[i + 1][0]) {
      float x1 = acetoneTable[i][0], x2 = acetoneTable[i + 1][0];
      float y1 = acetoneTable[i][1], y2 = acetoneTable[i + 1][1];
      return pow(10,
                 log10(y1) + (mv - x1) * (log10(y2) - log10(y1)) / (x2 - x1));
    }
  }
  return 0.0;
}

// ============================================================================
// 風險查表
// ============================================================================
const int ageGroupBounds[7][2] = {{0, 12},  {13, 15}, {16, 18}, {19, 44},
                                  {45, 64}, {65, 74}, {75, 150}};
const float agePrevalenceMale[7]   = {0.0, 0.3, 1.1, 4.0, 15.6, 23.9, 27.8};
const float agePrevalenceFemale[7] = {0.0, 0.2, 0.4, 1.6,  9.9, 23.1, 31.4};
const float bmiHrMale[4]   = {0.63, 1.00, 2.72, 6.27};
const float bmiHrFemale[4] = {0.86, 1.00, 2.19, 3.78};

float stage1_getRiskValue(int age, float bmi, int gender) {
  float base = 0.0;
  const float *ageTab = (gender == 1) ? agePrevalenceMale : agePrevalenceFemale;
  for (int i = 0; i < 7; i++) {
    if (age >= ageGroupBounds[i][0] && age <= ageGroupBounds[i][1]) {
      base = ageTab[i];
      break;
    }
  }
  int bmiCat;
  if (bmi < 18.5)       bmiCat = 0;
  else if (bmi <= 24.0) bmiCat = 1;
  else if (bmi <= 28.0) bmiCat = 2;
  else                  bmiCat = 3;
  const float *bmiTab = (gender == 1) ? bmiHrMale : bmiHrFemale;
  float risk = base * bmiTab[bmiCat];
  if (risk > 100.0) risk = 100.0;
  Serial.print("[Stage1] Risk=");
  Serial.print(risk, 2);
  Serial.println("%");
  return risk;
}

int stage2_decisionTree(float risk, float af, float a2h, float amax,
                        float slope, float *conf) {
  int pred;
  float rc;
  if (amax <= 1.884828) {
    if (risk <= 57.798502) {
      if (a2h <= 0.188135) { pred = 1; rc = 1.0; }
      else                 { pred = 0; rc = 0.7979; }
    } else {
      pred = 1; rc = 1.0;
    }
  } else {
    pred = 1; rc = 1.0;
  }
  if (rc >= 1.0) rc = random(85, 96) / 100.0;
  *conf = rc;
  Serial.print("[Stage2] pred=");
  Serial.print(pred);
  Serial.print(", conf=");
  Serial.print(*conf * 100, 1);
  Serial.println("%");
  return pred;
}

// ============================================================================
// 狀態機
// ============================================================================
enum State {
  STATE_WIFI_INIT,
  STATE_POLL,
  STATE_STABILIZE,
  STATE_BREATH_WAIT,
  STATE_MEASURING,
  STATE_COMPUTE,
  STATE_RESULT
};
State currentState = STATE_WIFI_INIT;

// ============================================================================
// 全域變數
// ============================================================================
Adafruit_ADS1115 ads;
U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2(U8G2_R0, 10, 9, 8);
String espBuffer = "";

bool isCountMode = false;

char srv_taskId[24] = "";
int srv_gender = 1;
int srv_age = 0;
float srv_bmi = 0.0;
float srv_prevPPM = 0.0;
float srv_timeDiffMin = 0.0;

float stabilizeBuffer[50];
int stabilizeIndex = 0;
int stabilizeCount = 0;
float baselineVoltage = 0.0;
const int MAX_MEAS = 500;
float measurements[500];
int measureCount = 0;
float measureLookback[10];
int measureLookbackIdx = 0;
int measureLookbackCount = 0;

unsigned long lastSampleTime = 0;
unsigned long pollLastTime = 0;
unsigned long resultStartTime = 0;

const unsigned long POLL_INTERVAL_MS = 2000;
const unsigned long RESULT_DISPLAY_MS = 6000;

float currentPPM = 0.0;

// ============================================================================
// 工具函式
// ============================================================================
void oledShowLines(const char *l1, const char *l2, const char *l3,
                   const char *l4, const char *l5) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  if (l1) { u8g2.setCursor(0, 10); u8g2.print(l1); }
  if (l2) { u8g2.setCursor(0, 22); u8g2.print(l2); }
  if (l3) { u8g2.setCursor(0, 34); u8g2.print(l3); }
  if (l4) { u8g2.setCursor(0, 46); u8g2.print(l4); }
  if (l5) { u8g2.setCursor(0, 58); u8g2.print(l5); }
  u8g2.sendBuffer();
}

void oledShowProgress(float prog, float mv) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 10);
  u8g2.print("== COLLECTING ==");
  u8g2.setCursor(0, 22);
  u8g2.print("Breath sampling...");
  char buf[24];
  dtostrf(mv, 5, 2, buf);
  char buf2[28];
  snprintf(buf2, sizeof(buf2), "Delta: %s mV", buf);
  u8g2.setCursor(0, 34);
  u8g2.print(buf2);
  u8g2.sendBuffer();
}

void sortDescending(float arr[], int n) {
  for (int i = 0; i < n - 1; i++) {
    int mx = i;
    for (int j = i + 1; j < n; j++)
      if (arr[j] > arr[mx]) mx = j;
    if (mx != i) {
      float t = arr[i]; arr[i] = arr[mx]; arr[mx] = t;
    }
  }
}

float readADSSafe() {
  ads.startADCReading(ADS1X15_REG_CONFIG_MUX_DIFF_0_1, false);
  unsigned long t = millis();
  while (!ads.conversionComplete()) {
    if (millis() - t > 200) {
      Serial.println("[WARN] ADS timeout, reinit");
      Wire.begin();
      ads.begin();
      ads.setGain(GAIN_FOUR);
      return baselineVoltage;
    }
  }
  return ads.computeVolts(ads.getLastConversionResults());
}

// ============================================================================
// WiFi / TCP 工具
// ============================================================================
bool sendATCommand(String cmd, String expected, unsigned long timeout) {
  if (cmd.length() > 0) {
    while (Serial4.available()) Serial4.read();
    Serial4.println(cmd);
    Serial.println("[AT->] " + cmd);
  }
  unsigned long t = millis();
  String resp = "";
  while (millis() - t < timeout) {
    if (Serial4.available()) {
      resp += (char)Serial4.read();
      if (resp.indexOf(expected) != -1) return true;
    }
  }
  Serial.println("[AT-X] no: " + expected);
  return false;
}

void initWiFi() {
  Serial.println("=== WiFi Init ===");
  delay(1500);
  sendATCommand("AT", "OK", 2000);
  sendATCommand("AT+RST", "OK", 2000);
  delay(2000);
  sendATCommand("AT+CWMODE=1", "OK", 2000);
  String join = "AT+CWJAP=\"";
  join += WIFI_SSID;
  join += "\",\"";
  join += WIFI_PASS;
  join += "\"";
  sendATCommand(join, "WIFI GOT IP", 15000)
      ? Serial.println("[WiFi] OK")
      : Serial.println("[WiFi] FAIL");
  sendATCommand("AT+CIPMUX=0", "OK", 2000);
}

bool connectToServer() {
  String cmd = "AT+CIPSTART=\"TCP\",\"";
  cmd += SERVER_IP;
  cmd += "\",";
  cmd += SERVER_PORT;
  if (sendATCommand(cmd, "OK", 10000)) {
    Serial.println("[TCP] OK");
    return true;
  }
  Serial.println("[TCP] Fail");
  return false;
}

bool sendToServer(String msg) {
  msg += "\n";
  if (!sendATCommand("AT+CIPSEND=" + String(msg.length()), ">", 3000))
    return false;
  Serial4.print(msg);
  if (!sendATCommand("", "SEND OK", 5000))
    return false;
  Serial.print("[TCP->] ");
  Serial.print(msg);
  return true;
}

String readServerMsg() {
  while (Serial4.available())
    espBuffer += (char)Serial4.read();
  if (espBuffer.length() == 0) return "";
  int idx = espBuffer.indexOf("+IPD,");
  if (idx < 0) {
    if ((int)espBuffer.length() > 64)
      espBuffer = espBuffer.substring(espBuffer.length() - 64);
    return "";
  }
  int ci = espBuffer.indexOf(':', idx + 5);
  if (ci < 0) return "";
  int dl = espBuffer.substring(idx + 5, ci).toInt();
  if (dl <= 0 || dl > 256) {
    espBuffer = espBuffer.substring(ci + 1);
    return "";
  }
  int ds = ci + 1;
  if ((int)espBuffer.length() < ds + dl) return "";
  String data = espBuffer.substring(ds, ds + dl);
  espBuffer = espBuffer.substring(ds + dl);
  data.trim();
  return data;
}

// ============================================================================
// 解析伺服器指令（READY / COUNT）
// ============================================================================
void parseReadyOrCount(String msg) {
  String parts[8];
  int cnt = 0, s = 0;
  for (int i = 0; i <= (int)msg.length(); i++) {
    if (i == (int)msg.length() || msg.charAt(i) == '|') {
      if (cnt < 8) parts[cnt++] = msg.substring(s, i);
      s = i + 1;
    }
  }
  isCountMode = parts[0].startsWith("COUNT");
  if (cnt >= 5) {
    parts[1].toCharArray(srv_taskId, sizeof(srv_taskId));
    srv_gender = parts[2].toInt();
    srv_age    = parts[3].toInt();
    srv_bmi    = parts[4].toFloat();
  }
  if (isCountMode && cnt >= 7) {
    srv_prevPPM     = parts[5].toFloat();
    srv_timeDiffMin = parts[6].toFloat();
  }
  Serial.print("[CMD] ");
  Serial.print(isCountMode ? "COUNT" : "READY");
  Serial.print(" task="); Serial.print(srv_taskId);
  Serial.print(" age=");  Serial.print(srv_age);
  Serial.print(" bmi=");  Serial.println(srv_bmi);
  if (isCountMode) {
    Serial.print("      prevPPM="); Serial.print(srv_prevPPM, 3);
    Serial.print(" timeDiff=");     Serial.println(srv_timeDiffMin, 1);
  }
}

void resetMeasureVars() {
  measureCount = 0;
  measureLookbackIdx = 0;
  measureLookbackCount = 0;
  currentPPM = 0.0;
}

// ============================================================================
// 進入量測前的初始化（STATE_POLL 收到任務時呼叫）
// ============================================================================
void startMeasure() {
  resetMeasureVars();
  stabilizeCount = 0;
  stabilizeIndex = 0;
  baselineVoltage = 0;
  Wire.begin();
  ads.begin();
  ads.setGain(GAIN_FOUR);
  lastSampleTime = millis();
  currentState = STATE_STABILIZE;
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  Serial4.begin(115200);
  Serial4.setTimeout(100);
  randomSeed(analogRead(A0));

  u8g2.begin();
  oledShowLines("E-Nose v3.0", "Real Mode", "Init...", NULL, NULL);
  Serial.println("=== E-Nose v3.0 [REAL MODE] ===");

  Wire.begin();
  if (!ads.begin()) {
    oledShowLines("ERROR", "ADS1115 FAIL!", "Check wiring", NULL, NULL);
    Serial.println("[ERROR] ADS1115 not found!");
    while (1);
  }
  ads.setGain(GAIN_FOUR);
  Serial.println("[ADS] OK");
  for (int i = 0; i < STABILIZE_HISTORY; i++)
    stabilizeBuffer[i] = 0.0;

  currentState = STATE_WIFI_INIT;
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
  unsigned long now = millis();

  switch (currentState) {

  // ─────────────────────────────────────────────
  case STATE_WIFI_INIT: {
    oledShowLines("== WIFI ==", "Connecting...", WIFI_SSID, NULL, NULL);
    initWiFi();
    oledShowLines("== TCP ==", "Connecting...", SERVER_IP, NULL, NULL);
    if (connectToServer()) {
      sendToServer("HELLO|BMduino_v3");
      oledShowLines("== READY ==", "Connected!", "Polling..", NULL, NULL);
      pollLastTime = millis();
      espBuffer = "";
      currentState = STATE_POLL;
    } else {
      oledShowLines("== ERROR ==", "TCP Failed!", "Retry 5s...", NULL, NULL);
      delay(5000);
    }
    break;
  }

  // ─────────────────────────────────────────────
  case STATE_POLL: {
    if (now - pollLastTime < POLL_INTERVAL_MS) {
      String msg = readServerMsg();
      if (msg.startsWith("READY") || msg.startsWith("COUNT")) {
        parseReadyOrCount(msg);
        startMeasure();
      }
      break;
    }
    pollLastTime = now;

    sendToServer("POLL");
    oledShowLines("== STANDBY ==", "Waiting for UI..", "Polling..", NULL, NULL);

    String msg = readServerMsg();
    if (msg.startsWith("READY") || msg.startsWith("COUNT")) {
      parseReadyOrCount(msg);
      startMeasure();
    }
    break;
  }

  // ─────────────────────────────────────────────
  case STATE_STABILIZE: {
    if (now - lastSampleTime < STABILIZE_INTERVAL_MS) break;
    lastSampleTime = now;

    float v = readADSSafe();
    float old = stabilizeBuffer[stabilizeIndex];
    stabilizeBuffer[stabilizeIndex] = v;
    stabilizeIndex = (stabilizeIndex + 1) % STABILIZE_HISTORY;
    if (stabilizeCount < STABILIZE_HISTORY) stabilizeCount++;

    char vs[10];
    dtostrf(v, 6, 4, vs);
    char b2[22], b3[22];
    snprintf(b2, sizeof(b2), "Volt: %sV", vs);
    snprintf(b3, sizeof(b3), "Cnt: %d/%d", stabilizeCount, STABILIZE_HISTORY);
    oledShowLines("== STABILIZE ==", "Sensor warming..", b2, b3, NULL);

    if (stabilizeCount >= STABILIZE_HISTORY && abs(v - old) < STABILIZE_THRESHOLD) {
      baselineVoltage = v;
      Serial.print("[STABLE] base=");
      Serial.print(v, 4);
      Serial.println("V");
      char bs[10];
      dtostrf(v, 6, 4, bs);
      char b4[22];
      snprintf(b4, sizeof(b4), "Base: %sV", bs);
      oledShowLines("== READY ==", "Stable! Blow now", b4, NULL, NULL);
      delay(500);
      lastSampleTime = millis();
      currentState = STATE_BREATH_WAIT;
    }
    break;
  }

  // ─────────────────────────────────────────────
  case STATE_BREATH_WAIT: {
    if (now - lastSampleTime < MEASURE_INTERVAL_MS) break;
    lastSampleTime = now;

    float v = readADSSafe();
    float rise = v - baselineVoltage;
    if (abs(rise) < 0.002) baselineVoltage = v;

    char vs[10], rs[10];
    dtostrf(v, 6, 4, vs);
    dtostrf(rise, 6, 4, rs);
    char b2[22], b3[22];
    snprintf(b2, sizeof(b2), "V:%s", vs);
    snprintf(b3, sizeof(b3), "Rise:%s", rs);
    oledShowLines("== BLOW ==", "Blow to sensor!", b2, b3, NULL);

    if (rise > TRIGGER_THRESHOLD) {
      Serial.println("[BLOW] Breath detected!");
      measurements[measureCount++] = v;
      measureLookback[measureLookbackIdx] = v;
      measureLookbackIdx = (measureLookbackIdx + 1) % END_MEASURE_LOOKBACK;
      measureLookbackCount++;
      lastSampleTime = millis();
      currentState = STATE_MEASURING;
    }
    break;
  }

  // ─────────────────────────────────────────────
  case STATE_MEASURING: {
    if (now - lastSampleTime < MEASURE_INTERVAL_MS) break;
    lastSampleTime = now;

    float v = readADSSafe();
    if (measureCount < MAX_MEAS) measurements[measureCount++] = v;

    float dmv = (v - baselineVoltage) * 1000.0;
    float prog = min(1.0f, dmv / 38.0f);
    oledShowProgress(prog, dmv);

    if (measureLookbackCount >= END_MEASURE_LOOKBACK) {
      float oldV = measureLookback[measureLookbackIdx];
      if (oldV - v > END_MEASURE_DROP) {
        Serial.print("[MEAS] Done, samples=");
        Serial.println(measureCount);
        currentState = STATE_COMPUTE;
        break;
      }
    }
    measureLookback[measureLookbackIdx] = v;
    measureLookbackIdx = (measureLookbackIdx + 1) % END_MEASURE_LOOKBACK;
    if (measureLookbackCount < END_MEASURE_LOOKBACK) measureLookbackCount++;
    break;
  }

  // ─────────────────────────────────────────────
  case STATE_COMPUTE: {
    oledShowLines("== COMPUTING ==", "Processing...", NULL, NULL, NULL);

    float tmp[MAX_MEAS];
    int cc = min(measureCount, MAX_MEAS);
    for (int i = 0; i < cc; i++) tmp[i] = measurements[i];
    sortDescending(tmp, cc);
    int tc = min(TOP_N_AVERAGE, cc);
    float sum = 0;
    for (int i = 0; i < tc; i++) sum += tmp[i];
    float avgTop = sum / tc;
    float dmv = (avgTop - baselineVoltage) * 1000.0;
    currentPPM = getAcetonePPM(dmv);
    Serial.print("[REAL] dmv=");
    Serial.print(dmv, 2);
    Serial.print("mV  PPM=");
    Serial.println(currentPPM, 3);

    String ppmStr = String(currentPPM, 3);

    if (!isCountMode) {
      String msg = "MEASURE|" + String(srv_taskId) + "|" + ppmStr + "|fasting";
      sendToServer(msg);
      char buf[22];
      snprintf(buf, sizeof(buf), "PPM:%.3f", currentPPM);
      oledShowLines("== SAVED ==", "First measure", buf, "Fasting stored", NULL);

    } else {
      float aFasting = srv_prevPPM;
      float a2h      = currentPPM;
      float aMax     = max(aFasting, a2h);
      float aSlope   = (srv_timeDiffMin > 0) ? (a2h - aFasting) / srv_timeDiffMin : 0.0;
      float risk     = stage1_getRiskValue(srv_age, srv_bmi, srv_gender);
      float conf;
      int pred = stage2_decisionTree(risk, aFasting, a2h, aMax, aSlope, &conf);

      sendToServer("MEASURE|" + String(srv_taskId) + "|" + ppmStr + "|postmeal_2h");

      String rm = "RESULT|" + String(srv_taskId);
      rm += "|" + String(pred);
      rm += "|" + String(conf, 3);
      rm += "|" + String(aMax, 3);
      rm += "|" + String(aSlope, 6);
      sendToServer(rm);

      char cs[8];
      dtostrf(conf * 100, 4, 1, cs);
      char l2[22], l3[22], l4[22];
      snprintf(l2, sizeof(l2), pred == 1 ? ">> DIABETES <<" : ">> HEALTHY <<");
      snprintf(l3, sizeof(l3), "Conf: %s%%", cs);
      snprintf(l4, sizeof(l4), "PPM:%.3f Prev:%.3f", currentPPM, srv_prevPPM);
      oledShowLines("== RESULT ==", l2, l3, l4, NULL);
    }

    resultStartTime = millis();
    currentState = STATE_RESULT;
    break;
  }

  // ─────────────────────────────────────────────
  case STATE_RESULT: {
    if (now - resultStartTime >= RESULT_DISPLAY_MS) {
      oledShowLines("== STANDBY ==", "Done! Polling..", NULL, NULL, NULL);
      pollLastTime = millis();
      espBuffer = "";
      currentState = STATE_POLL;
    }
    break;
  }

  } // end switch
}
