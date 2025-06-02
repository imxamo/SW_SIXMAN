#include <Arduino.h>
#include "DHT.h"

#define DIRT_PIN A0
#define WATER_PIN 34
#define WATER_LED_PIN 2 //(수위센서) LED 핀 (예: GPIO2)
#define TEMP_PIN 2        // DHT11 DATA 핀 연결 위치
#define DHTTYPE DHT11   // 센서 종류
#define PUMP_IN1 16              // 펌프 회전 방향 제어용 GPIO (IN1)
#define PUMP_IN2 17              // 펌프 회전 방향 제어용 GPIO (IN2)
#define PUMP_ENA 18              // 펌프 속도(PWM) 제어용 GPIO (ENA)
#define PUMP_PWM_CHANNEL 0       // PWM 신호에 사용할 채널 번호
#define PUMP_PWM_FREQ 5000       // PWM 신호의 주파수 (Hz)
#define PUMP_PWM_RESOLUTION 8    // PWM 해상도 (비트 수: 0~255 제어 가능)

// === 상추 생장 조건에 따른 제어 기준 ===
#define LETTUCE_SOIL_DRY_THRESHOLD 500     // 토양 수분 기준 (500 이상이면 건조)
#define LETTUCE_TEMP_MIN 18                // 최소 적정 온도
#define LETTUCE_TEMP_MAX 28                // 최대 적정 온도
#define LETTUCE_HUM_MIN 50                 // 최소 적정 습도

#define loop_sec 1000

DHT dht(TEMP_PIN, DHTTYPE); //온습도 센서 클래스

int dirt_value = 0;
int water_value = 0;
float temp_hum = dht.readHumidity();        // 습도 읽기
float temp_temp = dht.readTemperature();  // 온도 읽기 (°C)

void setup() {
  Serial.begin(9600);
  pinMode(DIRT_PIN, INPUT);
  //WATER_PIN은 아날로그 신호라서 필요하지 않음
 	pinMode(WATER_LED_PIN, OUTPUT);
  dht.begin(); //온습도 센서 클래스

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

  // 상추 생장 조건 기반 펌프 제어 로직
  if (dirt_value > LETTUCE_SOIL_DRY_THRESHOLD &&
      temp_temp >= LETTUCE_TEMP_MIN &&
      temp_temp <= LETTUCE_TEMP_MAX &&
      temp_hum >= LETTUCE_HUM_MIN) {
    Serial.println("조건 만족: 펌프 작동");
    pump();
  } else {
    Serial.println("조건 불충분: 펌프 미작동");
  }

  delay(10 * loop_sec);  // 10초마다 측정
}

  else pump_timer++;
  delay(10 * loop_sec);  // 10초마다 측정
}
