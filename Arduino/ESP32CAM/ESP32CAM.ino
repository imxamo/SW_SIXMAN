#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>

// ===== 와이파이 정보 =====
const char* ssid = "JJY_WIFI";
const char* password = "62935701";

// ===== 서버 주소 (Flask 백엔드 주소) =====
const char* serverUrl = "http://192.168.96.251:3000";  // /upload는 아래에서 붙음

// ===== 카메라 설정 함수 =====
void startCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
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

  if (psramFound()) {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }
}

// ===== Wi-Fi 연결 및 시작 =====
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Wi-Fi 연결 시도
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n[ERROR] WiFi connection failed.");
    return;
  }

  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // 카메라 시작
  startCamera();

  // 서버 연결 확인
  if (checkServerConnected()) {
    sendPhoto();  // 확인 성공 시 사진 전송
  } else {
    Serial.println("[ERROR] Server not responding with 200 OK.");
  }
}

void loop() {
  delay(10000);  // 매 10초마다 대기 (필요 시 반복 구조로 확장 가능)
}

// ===== 서버 연결 확인 (User-Agent 검사 포함) =====
bool checkServerConnected() {
  HTTPClient http;
  http.begin(String(serverUrl) + "/upload");  // GET도 허용된 upload 경로 사용
  http.setUserAgent("esp32-cam");             // 서버에서 'esp32' 시작 확인용
  int httpCode = http.GET();
  delay(500);  // 응답 대기
  http.end();

  Serial.printf("Server response code: %d\n", httpCode);
  return httpCode == 200;
}

// ===== 이미지 캡처 및 서버로 전송 =====
void sendPhoto() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[ERROR] Camera capture failed");
    return;
  }

  HTTPClient http;
  http.begin(String(serverUrl) + "/upload");
  http.setUserAgent("esp32-cam");
  http.addHeader("Content-Type", "image/jpeg");

  int httpResponseCode = http.POST(fb->buf, fb->len);
  delay(500);  // 전송 대기
  http.end();
  esp_camera_fb_return(fb);

  Serial.printf("Photo sent, server response code: %d\n", httpResponseCode);
}
