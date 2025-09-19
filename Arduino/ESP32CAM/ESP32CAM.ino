#include <WiFi.h>
#include <HTTPClient.h>

#define TIMEOUT 60

int bfHour;

int imageSend(){
    서버에 이미지 전송
    서버로부터 이미지 잘 받았는지 응답 대기
    if(응답이 없음){
        서버에 이미지 전송 재시도
        서버로부터 이미지 잘 받았는지 응답 대기
        if(응답 없음) return 1;
    }
    return 0;
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
    if(wifiFailCounter == 10){
        WiFi.begin(ssid, password);
        wifiFailCounter = 0;
    }
  }
  Serial.println(" connected!");

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
        while(1) {
            현실 hour = 인터넷에서 현재 hour 획득;
            if(WiFi.status() != WL_CONNECTED){
                Serial.println("Wifi 연결 끊김. 재접속 시도.");
                WiFi.begin(ssid, password);
                int wifiFailCounter = 0;
                while (WiFi.status() != WL_CONNECTED) {
                    delay(500);
                    Serial.print(".");
                    wifiFailCounter++;
                    if(wifiFailCounter == 10){
                        WiFi.begin(ssid, password);
                        wifiFailCounter = 0;
                    }
                }
            }
            httpCode = http.GET();
            if (httpCode > 0) {
                String payload = http.getString();
                payload.trim(); // 공백 제거
                if(payload == "200") {
                    Serial.println("응답 도착 : " + payload);
                }
                else if(payload == "201"){
                    Serial.println("응답 도착 : " + payload);
                    if(imageSend() == 1) break;
                }
            }
            else {
                Serial.printf("연결 실패 : %s\n", http.errorToString(httpCode).c_str());
                break;
            }
            if(bfHour != 현재 hour && 현재 hour%4 == 0){
                if(imageSend() == 1) break;
            }
            bfHour = 현재 hour;
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
