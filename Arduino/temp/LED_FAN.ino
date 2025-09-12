#include <Arduino.h>
#include "DHT.h"


#define COOLING_FAN_PIN 33
#define LED_PIN 5  // led 제어 버튼 핀


void setup() {
  // 쿨링팬 핀 설정
  pinMode(COOLING_FAN_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  digitalWrite(COOLING_FAN_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  delay(2000);
  digitalWrite(COOLING_FAN_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);
  delay(2000);
}
