#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>

#define TIMEOUT 60  // seconds

// === 사용자 설정 ===
// const char* ssid       = "SK_WiFiGIGA0A38_5G";
// const char* password   = "1704020165";
const char* ssid       = "JJY_WIFI";
const char* password   = "62935701";
const char* serverUrl  = "http://116.124.191.174:15020/get";   // GET 폴링
const char* uploadUrl  = "http://116.124.191.174:15020/upload";  // POST 업로드

int bfHour;  // 마지막으로 전송한 시간(중복 방지)

// ===================== 카메라 핀맵 =====================
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
// ======================================================

// 현재 시(hour) 얻기. 성공 시 0~23, 실패 시 -1
int getCurrentHour() {
  struct tm now;
  if (!getLocalTime(&now, 2000)) return -1;
  return now.tm_hour;
}

// === 실제 사진 찍고 서버로 전송 ===
int imageSend() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return 1;
  }

  HTTPClient http;
  http.begin(uploadUrl);
  http.setTimeout(TIMEOUT * 1000);
  http.addHeader("Content-Type", "application/octet-stream");

  int code = http.POST(fb->buf, fb->len);
  http.end();

  esp_camera_fb_return(fb);  // 프레임 버퍼 반환 (메모리 해제)

  if (code > 0 && code >= 200 && code < 300) {
    Serial.printf("Image uploaded successfully, code=%d\n", code);
    return 0;
  } else {
    Serial.printf("Image upload failed, code=%d\n", code);
    return 1;
  }
}

void setup() {
  Serial.begin(115200);

  // === 카메라 초기화 ===
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; 

  // VGA(640x480) 또는 QVGA 사용 가능
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 10;   // 낮을수록 고화질
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    while (true) delay(1000);
  }
  Serial.println("Camera init success");

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

  // === 시간 동기화 (Asia/Seoul) ===
  configTime(9 * 3600, 0, "pool.ntp.org");

  bfHour = -1;
}

void loop() {
  // 서버 접속 begin
  HTTPClient http;
  http.begin(serverUrl);
  http.setTimeout(TIMEOUT * 1000);

  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    payload.trim();
    Serial.println("응답 도착 : " + payload);

    if (payload == "200") {
      while (1) {
        int hour = getCurrentHour();

        // Wi-Fi 체크
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
          body.trim();

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

        // 4시간 간격 자동 업로드
        if (hour >= 0) {
          if (bfHour != hour && (hour % 4 == 0)) {
            if (imageSend() == 1) break;
          }
          bfHour = hour;
        }

        delay(60 * 1000);
      }
    }
  } else {
    Serial.printf("연결 실패 : %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
  delay(30 * 1000);
}
