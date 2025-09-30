#include <Arduino.h>
#include "DHT.h"
#include <time.h>
// ★
#include <WiFi.h>
#include <HTTPClient.h>
// ★

#define Water_Day_Limit 100
#define Water_Per_Try 25.0 // 1회 당 출력 요구량. 나눗셈 했을 때 float 연산을 유도하기 위해 소수점을 사용.
// 1. 펌프 초당 출력 테스트
#define Water_Per_Sec 25.0 // 펌프 초당 출력 25~30ml
#define Pump_Cooltime 3
#define HOT 25
#define WET 60
#define DRY 20

// 수위 센서 범위 : 0~4095
// 2. Water_Level 값을 설정한다.
#define Water_Level_Caution 40
#define Water_Level_Warning 30
#define Water_Level_Danger 20

#define COOLING_FAN_PIN 33
#define LED_PIN 5
#define DIRT_PIN A0
#define WATER_PIN 34
#define TEMP_PIN 22
#define DHTTYPE DHT11

#define PUMP_IN1 16
#define PUMP_IN2 17
#define PUMP_ENA 18

#define PUMP_PWM_FREQ 5000 // 주파수
#define PUMP_PWM_RESOLUTION 8 // 펌프 출력 범위를 8비트(0~255)로 설정함

// ★
// ===== Wi-Fi 설정 =====
const char* ssid = "JJY_WIFI";       // 와이파이 SSID
const char* password = "62935701";   // 와이파이 비밀번호
const char* serverUrl  = "http://116.124.191.174:15020/get";     // GET 폴링
const char* uploadUrl  = "http://116.124.191.174:15020/upload";  // POST 업로드
// ★

struct tm ntime;
const long gmtOffset_sec = 9 * 3600;
const int daylightOffset_sec = 0;
const char* ntpServer = "pool.ntp.org";
DHT dht(TEMP_PIN, DHTTYPE);

int waterRemain;
int pumpDelay, beepDelay;
int airTemp, airMoist, soilMoist, waterLevel;
float waterLevelPercent;
int edge_hour, edge_minute;

// 센서 데이터 보내는 함수
void sendSensorData() {
    // === sensor.txt 문자열 만들기 ===
    String sensorData = "";
    sensorData += "온도: " + String(airTemp) + " C\n";
    sensorData += "습도: " + String(airMoist) + " %\n";
    sensorData += "토양습도: " + String(soilMoist) + "\n";
    sensorData += "물수위: " + String(waterLevelPercent) + " %\n";

    Serial.println("전송할 데이터:\n" + sensorData);

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

void pump_active(float time){
	ledcWrite(PUMP_ENA, 255);
	delay(time * 1000);
	ledcWrite(PUMP_ENA, 0);
}

void pump(){
	if(pumpDelay >= Pump_Cooltime && soilMoist < DRY && waterRemain > 0){
		if(waterRemain >= Water_Per_Try){
			pump_active(Water_Per_Try/Water_Per_Sec);
			waterRemain -= Water_Per_Try;
			pumpDelay = 0;
		}
		else{
			pump_active(waterRemain/Water_Per_Sec);
			waterRemain = 0;
			pumpDelay = 0;
		}
	}
}

void fan(){
	if(digitalRead(COOLING_FAN_PIN) == LOW){ // 팬이 꺼져있을 때
    	if(airTemp >= HOT || airMoist >= WET){ // 덥거나 습할때
		  digitalWrite(COOLING_FAN_PIN, HIGH);
	  	}
  	}
  	else if(airTemp < HOT || airMoist < WET){
		digitalWrite(COOLING_FAN_PIN, LOW);
	}
}
// ★
void wifi_connect(){
    // === Wi-Fi 연결 ===
    WiFi.begin(ssid, password);
    Serial.print("WiFi connecting");
    int wifiFailCounter = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        wifiFailCounter++;
        if (wifiFailCounter == 10) {
            WiFi.begin(ssid, password);
            wifiFailCounter = 0;
        }
    }
    Serial.println(" connected!");
}
// ★

void setup() {
	// 3. 와이파이 연결 설정한다.
	Serial.begin(9600);
	dht.begin();
	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
	
	// 제어핀 설정
  	pinMode(COOLING_FAN_PIN, OUTPUT);
  	pinMode(LED_PIN, OUTPUT);
  	pinMode(PUMP_IN1, OUTPUT);
  	pinMode(PUMP_IN2, OUTPUT);

  	// 회전 방향: IN1=HIGH, IN2=LOW
  	digitalWrite(PUMP_IN1, HIGH);
  	digitalWrite(PUMP_IN2, LOW);

	ledcAttach(PUMP_ENA, PUMP_PWM_FREQ, PUMP_PWM_RESOLUTION); // 주파수와 출력 범위를 입력
  	ledcWrite(PUMP_ENA, 0); // 펌프 출력을 0으로 OFF

  	waterRemain = Water_Day_Limit;
	pumpDelay = 0, beepDelay = 0;
	edge_hour = 61; //0~60분 범위 밖의 값을 사용
    edge_minute = 61;

    wifi_connect();
}

void loop(){
	// 4. 시간 작동되는 지 테스트
	getLocalTime(&ntime);

    // 매일 0시
	if (ntime.tm_hour != edge_hour && ntime.tm_hour == 0) {
		waterRemain = Water_Day_Limit;
	}
	// 수정 요망 30분에서 오차가 추가되어야함
	if (ntime.tm_min != edge_minute && ntime.tm_min % 30 == 0) { // 30분마다
		// --- DHT11 온습도 ---
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
		Serial.print("물통 수위 값: ");
		Serial.print(waterLevel);
		Serial.print(" (");
		Serial.println(waterLevelPercent);
		Serial.print("%)");

		pumpDelay++;
		beepDelay++;
	}

	if (ntime.tm_hour != edge_hour) { // 1시간 마다 센서 전송
		// ★ 센서 값을 서버로 전송
		sendSensorData();
	}

	if(waterLevelPercent < Water_Level_Danger){
		pump();
	}

	
	if(waterLevelPercent < Water_Level_Caution && beepDelay > 0){
		if(waterLevelPercent < Water_Level_Danger) // 삐삐삐
		else if(waterLevelPercent < Water_Level_Warning) // 삐삐
		else // 삐
		beepDelay = 0;
	}

	if(ntime.tm_min != edge_hour && ntime.tm_hour == 6 && ntime.tm_min == 0){
		digitalWrite(LED_PIN, HIGH);
	}

	if(ntime.tm_min != edge_hour && ntime.tm_hour == 22 && ntime.tm_min == 0){
    digitalWrite(LED_PIN, LOW);
	}

	fan();

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
                // 즉시 센서값 전송
                sendSensorData();
        }
    } else {
        Serial.printf("연결 실패 : %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();

	// 사이클 쇼크 만들자리

	edge_hour = ntime.tm_hour; // 반드시 루프 마지막에 위치
    edge_minute = ntime.tm_min;
}
