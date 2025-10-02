#include <Arduino.h>
#include "DHT.h"

// ===== 습도 설정 =====
#define WET 60          // 팬 작동 습도
#define DRY_HUMID 40    // 팬 정지 습도

// ===== 핀 설정 =====
#define COOLING_FAN_PIN 33
#define TEMP_PIN 22
#define DHTTYPE DHT11

DHT dht(TEMP_PIN, DHTTYPE);

int airTemp = 0;
int airMoist = 0;

void fan() {
  if (digitalRead(COOLING_FAN_PIN) == LOW) { // 팬이 꺼져있을 때
    if (airMoist >= WET) { // 60% 이상일 때
      digitalWrite(COOLING_FAN_PIN, HIGH);
      Serial.println("🌀 팬 작동 시작 (습도 높음)");
    }
  }
  else { // 팬이 켜져있을 때
    if (airMoist < DRY_HUMID) { // 40% 미만일 때
      digitalWrite(COOLING_FAN_PIN, LOW);
      Serial.println("⏸️ 팬 정지 (습도 낮음)");
    }
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  
  pinMode(COOLING_FAN_PIN, OUTPUT);
  digitalWrite(COOLING_FAN_PIN, LOW); // 초기 팬 OFF
  
  Serial.println("===== 습도 제어 팬 테스트 시작 =====");
  Serial.printf("작동 습도: %d%% 이상\n", WET);
  Serial.printf("정지 습도: %d%% 미만\n", DRY_HUMID);
}

void loop() {
  // DHT11 온습도 읽기
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  if (!isnan(h) && !isnan(t)) {
    airTemp = (int)t;
    airMoist = (int)h;
    
    Serial.print("💧 습도: ");
    Serial.print(airMoist);
    Serial.print(" %, 온도: ");
    Serial.print(airTemp);
    Serial.print(" °C, 팬 상태: ");
    Serial.println(digitalRead(COOLING_FAN_PIN) ? "ON" : "OFF");
  }
  else {
    Serial.println("❌ DHT11 READ ERROR");
  }
  
  // 팬 제어
  fan();
  
  delay(2000); // 2초마다 체크
}
