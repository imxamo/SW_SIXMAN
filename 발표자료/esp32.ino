#include <Arduino.h>
#include "DHT.h"  // 온습도 센서 라이브러리 포함

// ----- 센서 및 액추에이터 핀 정의 -----
#define DIRT_PIN A0         // 토양 습도 센서 (아날로그 입력)
#define WATER_PIN 34        // 수위 센서 (아날로그 입력)
#define WATER_LED_PIN 2     // 수위 표시용 LED 핀
#define TEMP_PIN 22         // 온습도 센서 데이터 핀
#define DHTTYPE DHT11       // 온습도 센서 타입 (DHT11)
#define PUMP_IN1 16         // 물 펌프 제어용 입력 1 (모터 방향)
#define PUMP_IN2 17         // 물 펌프 제어용 입력 2 (모터 방향)
#define PUMP_ENA 18         // 물 펌프 PWM 제어 핀
#define RELAY_PIN 23        // 외부 릴레이(LED) 제어 핀
#define COOLING_FAN_PIN 13  // 쿨링팬 제어 핀
#define BUTTON_PIN 4        // 사용자 버튼 입력 핀 (릴레이용)

// ----- 펌프 제어 PWM 설정 -----
#define PUMP_PWM_FREQ 5000
#define PUMP_PWM_RESOLUTION 8

// ----- 루프 주기 정의 (ms 단위) -----
#define loop_sec 1000

// ----- 센서 및 시스템 변수 -----
DHT dht(TEMP_PIN, DHTTYPE);  // 온습도 센서 객체 생성
int dirt_value = 0;          // 토양 습도 값 저장 변수
int water_value = 0;         // 수위 값 저장 변수
float temp_hum;              // 습도 값
float temp_temp;             // 온도 값
bool ledState = LOW;         // 릴레이(LED) 상태 저장 변수

void setup() {
  Serial.begin(9600);  // 시리얼 통신 초기화

  // 센서 입력 및 출력 핀 설정
  pinMode(DIRT_PIN, INPUT);
  pinMode(WATER_LED_PIN, OUTPUT);

  // 펌프 제어 핀 출력 설정
  pinMode(PUMP_IN1, OUTPUT);
  pinMode(PUMP_IN2, OUTPUT);

  // 쿨링팬 제어 핀 출력 설정
  pinMode(COOLING_FAN_PIN, OUTPUT);

  // 버튼 입력 핀 설정 (내부 풀업 저항 사용)
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // 펌프 PWM 제어 초기화
  ledcAttach(PUMP_ENA, PUMP_PWM_FREQ, PUMP_PWM_RESOLUTION);

  // 온습도 센서 초기화
  dht.begin();

  // 릴레이 핀 출력 설정
  pinMode(RELAY_PIN, OUTPUT);
}

// ----- 토양 습도 측정 함수 -----
void dirt() {
  dirt_value = analogRead(DIRT_PIN);  // 아날로그 값 읽기
  Serial.print("토양 습도 값: ");
  Serial.println(dirt_value);
}

// ----- 수위 센서 측정 함수 -----
void water() {
  water_value = analogRead(WATER_PIN);
  Serial.print("수위 값: ");
  Serial.println(water_value);
}

// ----- 온습도 센서 측정 함수 -----
void temp() {
  temp_hum = dht.readHumidity();
  temp_temp = dht.readTemperature();

  // 측정 오류 체크
  if (isnan(temp_hum) || isnan(temp_temp)) {
    Serial.println("DHT11 READ ERROR");
    return;
  }

  // 측정값 출력
  Serial.print("온도 : ");
  Serial.print(temp_temp);
  Serial.print("°C, 습도 : ");
  Serial.print(temp_hum);
  Serial.println("%");
}

// ----- 펌프 작동 함수 -----
void pump(int pump_seconds) {
  if (pump_seconds <= 0) {
    Serial.println("급수 불필요 - 토양 습도 적정");
    return;
  }

  Serial.print("급수 시작 - ");
  Serial.print(pump_seconds);
  Serial.println("초간 급수");

  // 펌프 ON (정방향 회전)
  digitalWrite(PUMP_IN1, HIGH);
  digitalWrite(PUMP_IN2, LOW);
  ledcWrite(PUMP_ENA, 255);  // PWM 최대값 (펌프 최고 속도)

  delay(pump_seconds * 1000);  // 지정 시간 동안 급수

  // 펌프 OFF
  digitalWrite(PUMP_IN1, LOW);
  digitalWrite(PUMP_IN2, LOW);
  ledcWrite(PUMP_ENA, 0);

  Serial.println("급수 완료");
}

// ----- 토양 습도에 따른 급수 시간 계산 -----
int get_pump_time(int soil_moisture) {
  // 값이 낮을수록 건조한 상태
  if (soil_moisture < 300) {
    Serial.println("토양 상태: 매우 건조 - 8초 급수");
    return 8;
  } else if (soil_moisture < 400) {
    Serial.println("토양 상태: 건조 - 6초 급수");
    return 6;
  } else if (soil_moisture < 500) {
    Serial.println("토양 상태: 약간 건조 - 4초 급수");
    return 4;
  } else if (soil_moisture < 600) {
    Serial.println("토양 상태: 조금 부족 - 2초 급수");
    return 2;
  } else {
    Serial.println("토양 상태: 적정 습도 - 급수 불필요");
    return 0;
  }
}

// ----- 쿨링팬 제어 함수 -----
void cooling_fan(bool fan_on) {
  if (fan_on) {
    digitalWrite(COOLING_FAN_PIN, HIGH);
    Serial.println("쿨링팬 ON");
  } else {
    digitalWrite(COOLING_FAN_PIN, LOW);
    Serial.println("쿨링팬 OFF");
  }
}

// ----- 펌프 작동을 위한 타이머 -----
int pump_timer = 3;  // 시작 시 펌프 작동을 유도하기 위한 초기값

// ----- 메인 루프 함수 -----
void loop() {
  dirt();    // 토양 습도 측정
  water();   // 수위 측정
  temp();    // 온습도 측정

  // ----- 버튼 입력 처리 (LED 상태 토글) -----
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(200);  // 디바운싱 (버튼 흔들림 제거)
    ledState = !ledState;
    digitalWrite(RELAY_PIN, ledState);
    Serial.print("LED 상태: ");
    Serial.println(ledState ? "켜짐" : "꺼짐");
  }

  // ----- 습도 기준으로 쿨링팬 제어 -----
  if (temp_hum > 65 && !isnan(temp_hum)) {
    cooling_fan(true);   // 습도 높음 → 팬 작동
  } else if (temp_hum < 55 && !isnan(temp_hum)) {
    cooling_fan(false);  // 습도 낮음 → 팬 정지
  }

  // ----- 토양 습도 기반 펌프 작동 (매 3 루프마다) -----
  if (pump_timer >= 3) {
    int pump_time = get_pump_time(dirt_value);
    pump(pump_time);
    pump_timer = 0;  // 타이머 초기화
  } else {
    pump_timer++;
  }

  Serial.println("====================");
  delay(5 * loop_sec);  // 5초 간격으로 루프 실행
}
