#include <WiFi.h>
#include "time.h"

// ===== WiFi 설정 =====
const char* ssid = "JJY_WIFI";
const char* password = "62935701";

// ===== 시간 설정 =====
#define START_HOUR 6               // 하루 시작 시각
#define LIGHT_GOAL_HOURS 16        // 하루 채광 목표 (시간)

// ===== 핀 설정 =====
#define LED_PIN 5
#define LDR_PIN 34                 // 조도센서 아날로그 핀
#define LDR_THRESHOLD 1000         // 조도 임계값 (환경에 맞게 조정)

// ===== 상태 변수 =====
float light_hours_count = 0;        // 채광 카운터
bool sunlight = false;              // 조도 센서 : 채광 여부

// ===== NTP 서버 설정 =====
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 9 * 3600;    // 한국 UTC+9
const int daylightOffset_sec = 0;

time_t last_check_time;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  // WiFi 연결
  WiFi.begin(ssid, password);
  Serial.print("WiFi 연결 중");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi 연결 완료!");

  // NTP 동기화
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // 초기 시간 기록
  last_check_time = time(nullptr);
  
  Serial.println("===== 조명 제어 테스트 시작 =====");
  Serial.printf("작동 시간: %d시 ~ %d시\n", START_HOUR, START_HOUR + LIGHT_GOAL_HOURS);
  Serial.printf("목표 채광: %d시간\n", LIGHT_GOAL_HOURS);
}

void loop() {
  // 현재 시각 불러오기
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("시간 불러오기 실패");
    delay(1000);
    return;
  }

  int hour = timeinfo.tm_hour;
  int minute = timeinfo.tm_min;
  int second = timeinfo.tm_sec;

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

    // 조도센서 판정
    int ldr_value = analogRead(LDR_PIN);
    sunlight = (ldr_value > LDR_THRESHOLD);

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

  delay(1000); // 1초 간격 체크
}
