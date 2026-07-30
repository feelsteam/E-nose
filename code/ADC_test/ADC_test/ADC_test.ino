const int pinA1 = A1;
const int pinA3 = A3;
const float VREF = 3.3; 
const float ADC_MAX = 4095.0; // 使用 12-bit 解析度

void setup() {
  Serial.begin(115200);
  
  // 強制設定 analogRead 回傳 12-bit 解析度 (0~4095)
  analogReadResolution(12); 
}

void loop() {
  int rawA1 = analogRead(pinA1);
  int rawA3 = analogRead(pinA3);

  float voltageA1 = (rawA1 / ADC_MAX) * VREF;
  float voltageA3 = (rawA3 / ADC_MAX) * VREF;

  Serial.print("A1: ");
  Serial.print(voltageA1, 3);
  Serial.print(" V, A3: ");
  Serial.print(voltageA3, 3);
  Serial.println(" V");

  delay(500);
}