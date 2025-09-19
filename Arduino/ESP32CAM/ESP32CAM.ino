#include <WiFi.h>
#include <HTTPClient.h>

#define TIMEOUT 60

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* serverUrl = "http://yourserver.com/hello"; // 서버 주소

void setup() {
  Serial.begin(115200);

  // Wi-Fi 연결
  WiFi.begin(ssid, password);
  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected!");
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
    Serial.println("응답 도착 : " + payload);

    // WELCOME 판별
    if (payload == "WELCOME") {
      while(1) {
        if(현재분 == 0 || 현재분 == 30){
        // n시 0분 또는 n시 30분일때{
          Send to Server 사진 전송
          서버로부터 사진 잘 받았다고 응답이 올때까지 대기 (최대 5분)
          if(응답이 없음){
            break;
          }
        }
        delay(60 * 1000);
        // ESP32는 빠르게 무한 루프가 실행되면 안전장치가 프로그램을 리셋 해버린다. 그래서 딜레이는 필수이다.
      }
    }
  } else {
    Serial.printf("http.GET 실패 : %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();

  // 실패했거나 WELCOME 아니면 30초 후 재시도
  delay(30 * 1000);
}
