#include <Arduino.h>
#include <time.h>
#include <WiFi.h>
#include <HTTPClient.h>


#define TIMEOUT 5
const long gmtOffset_sec = 9 * 3600;  // 한국 UTC+9
const int daylightOffset_sec = 0;

// ★
// ===== Wi-Fi 설정 =====
const char* ssid = "JJY_WIFI";       // 와이파이 SSID
const char* password = "62935701";   // 와이파이 비밀번호
const char* serverUrl  = "http://116.124.191.174:15020/get?id=ESP32-001";     
const char* uploadUrl  = "http://116.124.191.174:15020/upload?id=ESP32-001";  
const char* levelUrl   = "http://116.124.191.174:15020/level?id=ESP32-001";
// ★

struct tm ntime;
const char* ntpServer = "pool.ntp.org";

int airTemp, airMoist, soilMoist, waterLevel;
float waterLevelPercent;
int edge_hour, edge_minute;
int plantLevel = 0;

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
			plantLevel -= 300;
            Serial.printf("plantLevel = %d\n", plantLevel);
        } else {
            Serial.printf("통신 실패: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
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
	Serial.begin(115200);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

	edge_hour = 61; //0~60분 범위 밖의 값을 사용
  edge_minute = 61;

	//★
  wifi_connect();
  getPlantLevel();
  airTemp = 1;
  airMoist = 2;
  soilMoist = 3;
  waterLevel = 4;
  waterLevelPercent = 5;
}

void loop(){
	// 4. 시간 작동되는 지 테스트
	getLocalTime(&ntime);

	if (ntime.tm_hour != edge_hour) {
		sendSensorData();
	}

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
  } else {
    Serial.printf("연결 실패 : %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();

	edge_hour = ntime.tm_hour;
  edge_minute = ntime.tm_min;

  delay(5000);
}
