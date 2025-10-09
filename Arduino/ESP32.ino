#include <Arduino.h>
#include <time.h>
#include "DHT.h"
#include <WiFi.h>
#include <HTTPClient.h>

#define TIMEOUT 5

// ===== 시간 설정 =====
#define START_HOUR 6               // 하루 시작 시각
#define LIGHT_GOAL_HOURS 16        // 하루 채광 목표 (시간)

#define LED_PIN 5 // LED
#define TEMP_PIN 22 // 온습도 센서
#define COOLING_FAN_PIN 33 // Fan
#define WATER_PIN 34 // 수위 센서
#define LDR_PIN 35 // 조도 센서
#define DIRT_PIN A0 // 토양 센서
#define DHTTYPE DHT11 // 온습도 관련

#define HOT 25
#define WET 60
#define DRY 20

// 수위 센서 범위 : 0~4095
// ★★★★★★★★ 팩트 기반 값 필요
#define Water_Level_Caution 40
#define Water_Level_Warning 30
#define Water_Level_Danger 20

// ===== 상태 변수 =====
float light_hours_count = 0;        // 채광 카운터
bool sunlight = false;              // 조도 센서 : 채광 여부

// ===== NTP 서버 설정 =====
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 9 * 3600;    // 한국 UTC+9
const int daylightOffset_sec = 0;

// ===== Wi-Fi 설정 =====
const char* ssid = "JJY_WIFI";       // 와이파이 SSID
const char* password = "62935701";   // 와이파이 비밀번호
const char* serverUrl  = "http://116.124.191.174:15020/get?id=ESP32-001";
const char* uploadUrl  = "http://116.124.191.174:15020/upload?id=ESP32-001";
const char* levelUrl   = "http://116.124.191.174:15020/level?id=ESP32-001";

struct tm ntime; // 현재 시간
time_t last_check_time; // LED & 조도 센서 관련
DHT dht(TEMP_PIN, DHTTYPE); // 온습도 관련

int airTemp, airMoist, soilMoist, waterLevel;
int edge_hour, edge_minute;
int hour, minute, second;
int plantLevel;
int loop_count;
float waterLevelPercent;

void led() {

    hour = ntime.tm_hour;
    minute = ntime.tm_min;
    second = ntime.tm_sec;

    // 하루 시작 전 (자정 ~ 06:00) → 카운터 리셋 & LED OFF
    if (hour < START_HOUR) {
        light_hours_count = 0;
        digitalWrite(LED_PIN, LOW);
    }

    // 채광 시간대 (06:00 ~ 22:00) 에만 LED on/off 판단
    else if (hour >= START_HOUR && hour < START_HOUR + LIGHT_GOAL_HOURS) {
        // 경과 시간 계산 (Δt 누적)
        time_t now = time(nullptr);
        int delta = difftime(now, last_check_time);
        last_check_time = now;

        // 조도센서 판정 (밝기 임계값은 환경에 맞게 조정 필요)
        int ldr_value = analogRead(LDR_PIN);
        sunlight = (ldr_value < 1000); // ★★★★★★★ 팩트 기반 값 필요
    

        // 카운터 증가 (빛 공급이 있을 때만)
        if (light_hours_count < LIGHT_GOAL_HOURS) {
        if (sunlight) {
            digitalWrite(LED_PIN, LOW); // 해 있음 → LED OFF
            light_hours_count += delta / 3600.0;
        }
        else {
            digitalWrite(LED_PIN, HIGH); // 해 없음 → LED ON
            light_hours_count += delta / 3600.0;
        }
        }
        else {
        digitalWrite(LED_PIN, LOW); // 목표 달성 시 LED OFF
        }
    }

    // 하루 종료 (22:00 ~ 자정) → LED OFF
    else {
        digitalWrite(LED_PIN, LOW);
    }

    // 상태 출력
    Serial.printf("시간 %02d:%02d:%02d | 누적 %.2f h | LED %s | LDR %d\n",
                hour, minute, second,
                light_hours_count,
                digitalRead(LED_PIN) ? "ON" : "OFF",
                analogRead(LDR_PIN));
}       

void fan(){
	if(digitalRead(COOLING_FAN_PIN) == LOW){
    	if(airTemp >= HOT || airMoist >= WET){
		  digitalWrite(COOLING_FAN_PIN, HIGH);
	  	}
  	}
  	else if(airTemp < HOT && airMoist < WET){
		digitalWrite(COOLING_FAN_PIN, LOW);
	}
}

void wifi(){
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wifi 연결 끊김. 재접속 시도.");
        wifi_connect();
    }

    // 서버 접속 begin
    HTTPClient http;
    http.begin(serverUrl);
    http.setTimeout(TIMEOUT * 1000);

    int httpCode = http.GET();
    if (httpCode > 0) {
        String payload = http.getString();
        payload.trim();
        Serial.println("응답 도착 : " + payload);
        if (payload == "201") {
            sendSensorData();
        }
    }
    else {
        Serial.printf("연결 실패 : %s\n", http.errorToString(httpCode).c_str());
    }

    if(loop_count == 5){
        Serial.println("센서값 송신");
        sendSensorData();
        Serial.println("식물 생장 단계 수신");
        getPlantLevel();
        loop_count = 0;
    }
    http.end();
}

// 센서 데이터 보내는 함수
void sendSensorData() {
  // === sensor.txt 문자열 만들기 ===
  String sensorData = "";
  sensorData += "온도: " + String(airTemp) + " C\n";
  sensorData += "습도: " + String(airMoist) + " %\n";
  sensorData += "토양습도: " + String(soilMoist) + "\n";
  sensorData += "물수위: " + String(waterLevelPercent) + " %\n";

  // === 서버로 POST 전송 ===
  if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(uploadUrl);   // ✅ 업로드 전용 주소
      http.addHeader("Content-Type", "text/plain"); // sensor.txt 형식
      int httpCode = http.POST(sensorData);
      if (httpCode > 0) {
          Serial.printf("POST 응답: %d\n", httpCode);
      } else {
          Serial.printf("POST 실패: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end();
  }
}

void getPlantLevel() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(levelUrl);
        http.setTimeout(TIMEOUT * 1000);

        int httpCode = http.GET();   // POST가 아니라 GET 요청일 가능성이 높음
        if (httpCode > 0) {
            String payload = http.getString();
            payload.trim();
            Serial.printf("응답: %s\n", payload.c_str());

            plantLevel = payload.toInt();
            Serial.printf("plantLevel = %d\n", plantLevel);
        } else {
            Serial.printf("통신 실패: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
    }
    else {
        Serial.println("wifi 연결 실패로 plantLevel 초기화");
        plantLevel = 0;
    }
}

void wifi_connect(){
    WiFi.begin(ssid, password);
    Serial.print("WiFi connecting");
    int wifi_counter = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        wifi_counter++;
        if(wifi_counter == 2 * 10){
            Serial.println(" failed");
            return;
        }
    }
    Serial.println(" connected!");
}

void sensor(){
    // --- DHT11 온습도 ---
    delay(1000); // 안정화
  	float h = dht.readHumidity();
  	float t = dht.readTemperature();
  	if (!isnan(h) && !isnan(t)) {
        airTemp = (int)t;
        airMoist = (int)h;
        Serial.print("온도: "); Serial.print(airTemp);
        Serial.print(" °C, 습도: "); Serial.print(airMoist); Serial.println(" %");
    }
	else {
    	Serial.println("DHT11 READ ERROR");
  	}
    // --- 토양 습도 ---
    soilMoist = analogRead(DIRT_PIN);
    Serial.print("토양 습도 값: ");
    Serial.println(soilMoist);
    // --- 물 수위 ---
    waterLevel = analogRead(WATER_PIN);
    waterLevelPercent = waterLevel / 40.95;
    Serial.printf("물통 수위: %d (%.1f%%)\n", waterLevel, waterLevelPercent);
}

void setup() {
	Serial.begin(115200);
    dht.begin();
    
    // 제어핀 설정
  	pinMode(COOLING_FAN_PIN, OUTPUT);
  	pinMode(LED_PIN, OUTPUT);

    Serial.println("시간 불러오는 중");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    delay(10000);
    Serial.println("wifi 연결하는 중");
    wifi_connect();
    Serial.println("식물 성장 단계 불러오는 중");
    getPlantLevel();
    last_check_time = time(nullptr);
    edge_hour = 61; //0~60분 범위 밖의 값을 사용
    edge_minute = 61;
    loop_count = 0;
}

void loop(){
    if (!getLocalTime(&ntime)) {
        Serial.println("시간 불러오기 실패. 이전 시간 유지.");
        return;
    }

    sensor();
    fan();
    led();
    wifi();

    edge_hour = ntime.tm_hour;
    edge_minute = ntime.tm_min;
    loop_count++;

    delay(60*1000);
}
