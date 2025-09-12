#include <Arduino.h>

#define PUMP_IN1 16
#define PUMP_IN2 17
#define PUMP_ENA 18   // PWM 가능 핀

#define PUMP_PWM_FREQ 5000
#define PUMP_PWM_RESOLUTION 8  // 0~255

void setup() {
  // 펌프 제어 핀
  pinMode(PUMP_IN1, OUTPUT);
  pinMode(PUMP_IN2, OUTPUT);

  // 회전 방향: IN1=HIGH, IN2=LOW
  digitalWrite(PUMP_IN1, HIGH);
  digitalWrite(PUMP_IN2, LOW);

  // ENA 최대 듀티로 고정 (항상 켜짐)
  ledcAttach(PUMP_ENA, PUMP_PWM_FREQ, PUMP_PWM_RESOLUTION);
  ledcWrite(PUMP_ENA, 255);  // 255 = 100%
}

void loop() {
  // 아무 것도 안 함 (항상 켜져 있음)
  delay(1000);
}
