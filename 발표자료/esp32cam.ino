#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>

// ===== Wi-Fi 정보 설정 =====
// ESP32-CAM이 연결할 Wi-Fi 네트워크 이름과 비밀번호
const char* ssid = "JJY_WIFI";
const char* password = "62935701";

// ===== 백엔드 서버 주소 =====
// 사진을 전송할 Flask 서버의 주소
const char* serverUrl = "http://192.168.96.240:3000";  // '/upload' 경로는 코드 내에서 추가됨

// ===== 동작 상태 관리 변수 =====
bool serverReady = false;               // 서버가 200 OK 응답을 보냈는지 여부
unsigned long lastSendTime = 0;        // 마지막으로 사진을 보낸 시간
const unsigned long sendInterval = 5000; // 사진 전송 주기: 5초

// ===== 카메라 설정 함수 =====
// 카메라 핀 배치 및 화질 설정 후 초기화 수행
void startCamera() {
  camera_config_t config;

  // LEDC(PWM) 설정
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  // 카메라 핀 설정 (ESP32-CAM 기준 핀매핑)
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
  config.pin_reset = -1;  // 리셋 핀 없음

  // 카메라 기본 설정
  config.xclk_freq_hz = 20000000;        // 클럭 주파수
  config.pixel_format = PIXFORMAT_JPEG;  // JPEG로 저장

  // PSRAM 여부에 따라 화질, 버퍼 수 조절
  if (psramFound()) {
    config.frame_size = FRAMESIZE_QVGA;  // 320x240 해상도
    config.jpeg_quality = 10;            // 품질 (낮을수록 고화질)
    config.fb_count = 2;                 // 프레임 버퍼 2개
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // 카메라 초기화
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }
}

// ===== ESP32 시작 시 실행되는 setup 함수 =====
void setup() {
  Serial.begin(115200);  // 시리얼 통신 시작
  delay(1000);

  // Wi-Fi 연결 시도
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  // 연결 시도 타임아웃 설정 (10초)
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n[ERROR] WiFi connection failed.");
    return;
  }

  // 연결 성공 시 IP 출력
  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // 카메라 초기화
  startCamera();

  // 서버 연결 테스트
  serverReady = checkServerConnected();
  if (!serverReady) {
    Serial.println("[ERROR] Server not responding with 200 OK.");
  }
}

// ===== 루프: 일정 주기로 사진 전송 =====
void loop() {
  if (!serverReady) return;  // 서버가 준비되지 않으면 무시

  unsigned long now = millis();
  if (now - lastSendTime >= sendInterval) {
    lastSendTime = now;
    sendPhoto();  // 사진 촬영 및 서버 전송
  }
}

// ===== 서버 연결 상태 확인 함수 =====
bool checkServerConnected() {
  HTTPClient http;
  http.begin(String(serverUrl) + "/upload");  // 테스트용으로 /upload 경로 요청
  http.setUserAgent("esp32-cam");            // 클라이언트 식별 문자열 설정
  int httpCode = http.GET();                 // GET 요청 (응답 코드 확인용)
  delay(500);
  http.end();

  Serial.printf("Server response code: %d\n", httpCode);
  return httpCode == 200;  // 서버가 정상 응답(200 OK)을 보냈는지 여부 반환
}

// ===== 사진 촬영 및 서버 전송 함수 =====
void sendPhoto() {
  camera_fb_t* fb = esp_camera_fb_get();  // 사진 캡처

  if (!fb) {
    Serial.println("[ERROR] Camera capture failed");
    return;
  }

  // HTTP 클라이언트 설정
  HTTPClient http;
  http.begin(String(serverUrl) + "/upload");  // 업로드 엔드포인트로 요청
  http.setUserAgent("esp32-cam");
  http.addHeader("Content-Type", "image/jpeg");  // 이미지 전송 MIME 타입 설정

  // POST 요청: 사진 데이터 전송
  int httpResponseCode = http.POST(fb->buf, fb->len);
  delay(500);  // 안정적인 전송을 위한 지연
  http.end();  // 연결 종료

  esp_camera_fb_return(fb);  // 프레임 버퍼 반환 (메모리 해제)

  Serial.printf("Photo sent, server response code: %d\n", httpResponseCode);
}
