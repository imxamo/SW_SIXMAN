#include <Arduino.h>
#include "DHT.h"

#define DIRT_PIN A0
#define WATER_PIN 34
#define WATER_LED_PIN 2
#define TEMP_PIN 2
#define DHTTYPE DHT11
#define PUMP_IN1 16
#define PUMP_IN2 17
#define PUMP_ENA 18
#define PUMP_PWM_CHANNEL 0
#define PUMP_PWM_FREQ 5000
#define PUMP_PWM_RESOLUTION 8
#define loop_sec 1000

DHT dht(TEMP_PIN, DHTTYPE);

int dirt_value = 0;
int water_value = 0;
float temp_hum;
float temp_temp;

void setup() {
  Serial.begin(9600);
  pinMode(DIRT_PIN, INPUT);
  pinMode(WATER_LED_PIN, OUTPUT);
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

void pump() {
  // 펌프 ON
  digitalWrite(PUMP_IN1, HIGH);
  digitalWrite(PUMP_IN2, LOW);
  ledcWrite(PUMP_PWM_CHANNEL, 255);  // 최대 속도

  delay(5 * loop_sec);

  // 펌프 OFF
  digitalWrite(PUMP_IN1, LOW);
  digitalWrite(PUMP_IN2, LOW);
  ledcWrite(PUMP_PWM_CHANNEL, 0);
}

int pump_timer = 3; //최초 실행시 펌프 작동을 위함
void loop() {
  dirt();
  water();
  temp();
  if(pump_timer == 3){ //루프 3회마다 1번씩 실행
    pump();
    pump_timer = 0;
  }
  else pump_timer++;
  delay(10 * loop_sec);  // 10초마다 측정
}
