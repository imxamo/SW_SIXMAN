#include <Arduino.h>
#include "DHT.h"

#define DIRT_PIN A0
#define WATER_PIN 34
#define WATER_LED_PIN 2
#define TEMP_PIN 22
#define DHTTYPE DHT11
#define PUMP_IN1 16
#define PUMP_IN2 17
#define PUMP_ENA 18
#define PUMP_PWM_FREQ 5000
#define PUMP_PWM_RESOLUTION 8
#define COOLING_FAN_PIN 13
#define loop_sec 1000

DHT dht(TEMP_PIN, DHTTYPE);

int dirt_value = 0;
int water_value = 0;
float temp_hum;
float temp_temp;

void setup() {
  Serial.begin(9600);
  
  // 센서 핀
  pinMode(DIRT_PIN, INPUT);
  pinMode(WATER_LED_PIN, OUTPUT);
  
  // 펌프 제어 핀 설정
  pinMode(PUMP_IN1, OUTPUT);
  pinMode(PUMP_IN2, OUTPUT);
  
  // 쿨링팬 핀 설정
  pinMode(COOLING_FAN_PIN, OUTPUT);
  
  // PWM 초기화 (ESP32 Arduino Core 3.x 버전)
  ledcAttach(PUMP_ENA, PUMP_PWM_FREQ, PUMP_PWM_RESOLUTION);
  
  // 온습도 센서 초기화
  dht.begin();
}

void dirt() {
  dirt_value = analogRead(DIRT_PIN);
  Serial.print("토양 습도 값: ");
  Serial.println(dirt_value);
}

void water() {
  water_value = analogRead(WATER_PIN);
  Serial.print("수위 값: ");
  Serial.println(water_value);
}

void temp() {
  temp_hum = dht.readHumidity();
  temp_temp = dht.readTemperature();
  
  if (isnan(temp_hum) || isnan(temp_temp)) {
    Serial.println("DHT11 READ ERROR");
    return;
  }
  
  Serial.print("온도 : ");
  Serial.print(temp_temp);
  Serial.print("°C, 습도 : ");
  Serial.print(temp_hum);
  Serial.println("%");
}

void pump(int pump_seconds) {
  if (pump_seconds <= 0) {
    Serial.println("급수 불필요 - 토양 습도 적정");
    return;
  }
  
  Serial.print("급수 시작 - ");
  Serial.print(pump_seconds);
  Serial.println("초간 급수");
  
  // 펌프 ON
  digitalWrite(PUMP_IN1, HIGH);
  digitalWrite(PUMP_IN2, LOW);
  ledcWrite(PUMP_ENA, 255);  // 최대 속도
  delay(pump_seconds * 1000);  // 초 단위로 변환
  
  // 펌프 OFF
  digitalWrite(PUMP_IN1, LOW);
  digitalWrite(PUMP_IN2, LOW);
  ledcWrite(PUMP_ENA, 0);
  
  Serial.println("급수 완료");
}

int get_pump_time(int soil_moisture) {
  // 토양 습도 센서 값에 따른 급수 시간 결정
  // 값이 낮을수록 건조함 (센서에 따라 조정 필요)
  
  if (soil_moisture < 300) {        // 매우 건조 (20-30% 습도)
    Serial.println("토양 상태: 매우 건조 - 8초 급수");
    return 8;
  } else if (soil_moisture < 400) { // 건조 (30-40% 습도)
    Serial.println("토양 상태: 건조 - 6초 급수");
    return 6;
  } else if (soil_moisture < 500) { // 약간 건조 (40-50% 습도)
    Serial.println("토양 상태: 약간 건조 - 4초 급수");
    return 4;
  } else if (soil_moisture < 600) { // 조금 부족 (50-60% 습도)
    Serial.println("토양 상태: 조금 부족 - 2초 급수");
    return 2;
  } else {                          // 적정 습도 (60% 이상)
    Serial.println("토양 상태: 적정 습도 - 급수 불필요");
    return 0;
  }
}

void cooling_fan(bool fan_on) {
  if (fan_on) {
    digitalWrite(COOLING_FAN_PIN, HIGH);  // 쿨링팬 ON
    Serial.println("쿨링팬 ON");
  } else {
    digitalWrite(COOLING_FAN_PIN, LOW);   // 쿨링팬 OFF
    Serial.println("쿨링팬 OFF");
  }
}

int pump_timer = 3; // 최초 실행시 펌프 작동을 위함

void loop() {
  dirt();
  water();
  temp();
  
  // 온도에 따른 쿨링팬 제어 (30도 이상이면 ON)
  if (temp_temp >= 30.0 && !isnan(temp_temp)) {
    cooling_fan(true);
  } else {
    cooling_fan(false);
  }
  
  // 토양 습도에 따른 자동 급수 (3회 루프마다 1번씩 체크)
  if (pump_timer >= 3) {
    int pump_time = get_pump_time(dirt_value);
    pump(pump_time);
    pump_timer = 0;
  } else {
    pump_timer++;
  }
  
  Serial.println("====================");
  delay(5 * loop_sec);  // 5초마다 측정
}
