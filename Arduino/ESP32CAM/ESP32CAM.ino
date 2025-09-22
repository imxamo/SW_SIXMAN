#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>

#define TIMEOUT 60  // seconds

// === 사용자 설정 ===
const char* ssid       = "YOUR_WIFI_SSID";
const char* password   = "YOUR_WIFI_PASSWORD";
const char* serverUrl  = "http://116.124.191.174:15020/get";   // GET 폴링
const char* uploadUrl  = "http://116.124.191.174:15020/upload";  // POST 업로드

int bfHour;  // 마지막으로 전송한 시간(중복 방지)

// 현재 시(hour) 얻기. 성공 시 0~23, 실패 시 -1
int getCurrentHour() {
  struct tm now;
  if (!getLocalTime(&now, 2000)) return -1;
  return now.tm_hour;
}

// 서버에 이미지 전송(자리표시자: text/plain POST) + 1회 재시도
int imageSend() {
  // 1차 시도
  {
    HTTPClient http;
    http.begin(uploadUrl);
    http.setTimeout(TIMEOUT * 1000);
    http.addHeader("Content-Type", "text/plain");
    int code = http.POST("IMAGE_DATA_PLACEHOLDER"); // 실제로는 멀티파트/바이너리로 교체
    http.end();
    if (code > 0 && code >= 200 && code < 300) return 0;
  }

  // 재시도 1회
  {
    HTTPClient http;
    http.begin(uploadUrl);
    http.setTimeout(TIMEOUT * 1000);
    http.addHeader("Content-Type", "text/plain");
    int code = http.POST("IMAGE_DATA_PLACEHOLDER_RETRY");
    http.end();
    if (code > 0 && code >= 200 && code < 300) return 0;
  }

  return 1; // 실패
}

void setup() {
  Serial.begin(115200);

  // Wi-Fi 연결
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

  // 시간 동기화 (Asia/Seoul)
  configTime(9 * 3600, 0, "pool.ntp.org");

  bfHour = -1;
}

void loop() {
  // 서버 접속 begin
  HTTPClient http;
  http.begin(serverUrl);
  http.setTimeout(TIMEOUT * 1000); // 타임아웃 시간 설정

  // GET 요청 보내기. 답장 올때까지 대기 상태로 전환한다.
  int httpCode = http.GET();

  if (httpCode > 0) {
    // 응답 본문 읽기
    String payload = http.getString();
    payload.trim(); // 공백 제거
    Serial.println("응답 도착 : " + payload);

    if (payload == "200") {
      while (1) {
        // 현재 시간(hour) 구하기
        int hour = getCurrentHour();

        // Wi-Fi 유지 확인 및 재접속(카운터 방식)
        if (WiFi.status() != WL_CONNECTED) {
          Serial.println("Wifi 연결 끊김. 재접속 시도.");
          WiFi.begin(ssid, password);
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
          Serial.println();
        }

        // 서버 폴링
        httpCode = http.GET();
        if (httpCode > 0) {
          String body = http.getString();
          body.trim(); // 공백 제거

          if (body == "200") {
            Serial.println("응답 도착 : " + body);
          } else if (body == "201") {
            Serial.println("응답 도착 : " + body);
            if (imageSend() == 1) break;
          }
        } else {
          Serial.printf("연결 실패 : %s\n", http.errorToString(httpCode).c_str());
          break;
        }

        // 4시간 간격 트리거 (하루 6회)
        if (hour >= 0) { // 시간 얻기 성공했을 때만 사용
          if (bfHour != hour && (hour % 4 == 0)) {
            if (imageSend() == 1) break;
          }
          bfHour = hour;
        }

        delay(60 * 1000);
        // ESP32는 빠르게 무한 루프가 실행되면 안전장치가 프로그램을 리셋 해버린다. 그래서 딜레이는 필수이다.
      }
    }
  } else {
    Serial.printf("연결 실패 : %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();

  // 실패했거나 WELCOME 아니면 30초 후 재시도
  delay(30 * 1000);
}

