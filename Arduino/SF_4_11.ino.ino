#include "DHT.h"

#define DIRT_PIN A0
#define WATER_PIN 34
#define WATER_LED_PIN = 2; //(수위센서) LED 핀 (예: GPIO2)
#define TEMP_PIN 2        // DHT11 DATA 핀 연결 위치
#define DHTTYPE DHT11   // 센서 종류
#define PUMP_IN1 16              // 펌프 회전 방향 제어용 GPIO (IN1)
#define PUMP_IN2 17              // 펌프 회전 방향 제어용 GPIO (IN2)
#define PUMP_ENA 18              // 펌프 속도(PWM) 제어용 GPIO (ENA)
#define PUMP_PWM_CHANNEL 0       // PWM 신호에 사용할 채널 번호
#define PUMP_PWM_FREQ 5000       // PWM 신호의 주파수 (Hz)
#define PUMP_PWM_RESOLUTION 8    // PWM 해상도 (비트 수: 0~255 제어 가능)

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

  //펌프
  pinMode(PUMP_IN1, OUTPUT);
  pinMode(PUMP_IN2, OUTPUT);
  ledcSetup(PUMP_PWM_CHANNEL, PUMP_PWM_FREQ, PUMP_PWM_RESOLUTION);
  ledcAttachPin(PUMP_ENA, PUMP_PWM_CHANNEL);
  // 펌프 꺼진 상태
  digitalWrite(PUMP_IN1, LOW);
  digitalWrite(PUMP_IN2, LOW);
  ledcWrite(PUMP_PWM_CHANNEL, 0);
}

void loop() {

  //DIRT 구간
  dirt_value = analogRead(DIRT_PIN);  // 값 읽기

  Serial.print("토양 습도 값: ");
  Serial.println(dirt_value);

  if (dirt_value > 800) {
    Serial.println("상태: 매우 건조함");
  } else if (dirt_value > 500) {
    Serial.println("상태: 약간 건조함");
  } else {
    Serial.println("상태: 습함");
  }
  
  //WATER 구간
  water_value = analogRead(WATER_PIN); // 수위 센서 값 읽기 (0 ~ 4095 범위)
	
	Serial.print("수위 값: ");
	Serial.println(water_value);

	if (waterLevel < 1000) {
		digitalWrite(WATER_LED_PIN, HIGH); // LED 켜기
  }
	else {
		digitalWrite(WATER_LED_PIN, LOW);  // LED 끄기
  }

  //TEMP 구간
  if (isnan(temp_hum) || isnan(temp_temp)) {
    Serial.println("DHT11 READ ERROR");
    return;
  }

  Serial.print("온도 : ");
  Serial.print(temp_temp);
  Serial.print(" °C, 습도 : ");
  Serial.print(temp_hum);
  Serial.println(" %");

  delay(1000);  // 1초마다 측정
}

/*
  // 펌프 ON
  digitalWrite(PUMP_IN1, HIGH);
  digitalWrite(PUMP_IN2, LOW);
  ledcWrite(PUMP_PWM_CHANNEL, 255);  // 최대 속도

  delay(5000);

  // 펌프 OFF
  digitalWrite(PUMP_IN1, LOW);
  digitalWrite(PUMP_IN2, LOW);
  ledcWrite(PUMP_PWM_CHANNEL, 0);
*/