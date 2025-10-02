#include <Arduino.h>
#include "DHT.h"
#include <time.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>

// ===== 제한값 설정 =====
#define Water_Day_Limit 100
#define Water_Per_Try 25.0
#define Water_Per_Sec 25.0
#define Pump_Cooltime 3
#define HOT 26
#define COOL 20
#define DRY 20

// ===== 수위 센서 임계값 =====
#define Water_Level_Caution 40
#define Water_Level_Warning 30
#define Water_Level_Danger 20

// ===== 조명 설정 =====
#define START_HOUR 6
#define LIGHT_GOAL_HOURS 16
#define LDR_THRESHOLD 1000

// ===== 핀 설정 =====
#define COOLING_FAN_PIN 33
#define LED_PIN 5
#define DIRT_PIN A0
#define WATER_PIN 34
#define LDR_PIN 35        // ✅ 수정: 34에서 35로 변경 (핀 충돌 해결)
#define TEMP_PIN 22
#define DHTTYPE DHT11

#define PUMP_IN1 16
#define PUMP_IN2 17
#define PUMP_ENA 18

#define PUMP_PWM_FREQ 5000
#define PUMP_PWM_RESOLUTION 8
#define PUMP_CH 0

// ===== Wi-Fi 설정 =====
const char* ssid = "JJY_WIFI";
const char* password = "62935701";
const char* serverUrl  = "http://116.124.191.174:15020/get";
const char* uploadUrl  = "http://116.124.191.174:15020/upload";
const char* levelUrl   = "http://116.124.191.174:15020/level";
#define TIMEOUT 10

// ===== NTP 설정 =====
struct tm ntime;
const long gmtOffset_sec = 9 * 3600;
const int daylightOffset_sec = 0;
const char* ntpServer = "pool.ntp.org";

// ===== DHT 센서 =====
DHT dht(TEMP_PIN, DHTTYPE);

// ===== 상태 변수 =====
int waterRemain;
int pumpDelay, beepDelay;
int airTemp, airMoist, soilMoist, waterLevel;
float waterLevelPercent;
int edge_hour, edge_minute;
int plantLevel = 0;

// ===== 조명 제어 변수 =====
float light_hours_count = 0;
bool sunlight = false;
time_t last_light_check = 0;

// ===== 급수 알고리즘 변수 (NVS) =====
Preferences prefs;
const char* NVS_NAMESPACE = "grow";
const char* NVS_KEY_PLANT = "plant_epoch";
const char* NVS_KEY_LAST_IRRIG = "last_irrig";

float PUMP_FLOW_LPS = 0.20f;
const float POT_L = 1.8;
const float POT_AREA_M2 = 0.24f * 0.24f;
const float AWC_L_PER_L = 0.20;
const float P_ALLOW = 0.40;
float ET0_MM_DAY = 2.5;

struct Stage {
  const char* name;
  int start_day;
  int end_day;
  float Kc;
  float G;
};

Stage STAGES[] = {
  {"초기(발아~본엽1-2)",   0, 10, 0.50f, 0.20f},
  {"생육중기(엽수 증가)", 11, 25, 0.70f, 0.50f},
  {"수확기(잎 최대)",     26, 45, 0.95f, 0.75f},
};
const int STAGE_COUNT = sizeof(STAGES)/sizeof(STAGES[0]);

// ===== 급수 알고리즘 함수들 =====
float calcDailyNeedL(float et0, float kc, float G) {
  float ETc = et0 * kc;
  float effective_area = POT_AREA_M2 * G;
  return ETc * effective_area;
}

float calcEventVolumeL() {
  return POT_L * AWC_L_PER_L * P_ALLOW;
}

bool getNow(time_t& nowUtc) {
  struct tm t;
  if (!getLocalTime(&t)) return false;
  time_t local = mktime(&t);
  nowUtc = local - gmtOffset_sec;
  return true;
}

int daysSince(time_t plantEpochUtc) {
  time_t nowUtc;
  if (!getNow(nowUtc)) return -1;
  double sec = difftime(nowUtc, plantEpochUtc);
  return (int)(sec / 86400.0);
}

const Stage* currentStage(int days_since) {
  for (int i=0; i<STAGE_COUNT; ++i) {
    if (days_since >= STAGES[i].start_day && days_since <= STAGES[i].end_day) {
      return &STAGES[i];
    }
  }
  return &STAGES[STAGE_COUNT-1];
}

void stageWithLerp(int days_since, float& kc_out, float& g_out, const char*& name_out) {
  const Stage* s = currentStage(days_since);
  name_out = s->name;
  float span = max(1, s->end_day - s->start_day);
  float prog = constrain((days_since - s->start_day) / span, 0.0f, 1.0f);

  int idx = 0;
  for (int i=0; i<STAGE_COUNT; ++i) if (s == &STAGES[i]) { idx = i; break; }
  if (idx < STAGE_COUNT - 1) {
    const Stage& next = STAGES[idx + 1];
    kc_out = s->Kc + (next.Kc - s->Kc) * prog;
    g_out  = s->G  + (next.G  - s->G ) * prog;
  } else {
    kc_out = s->Kc;
    g_out  = s->G;
  }
}

unsigned long msForLiters(float liters) {
  if (PUMP_FLOW_LPS <= 0.0f) return 0;
  float sec = liters / PUMP_FLOW_LPS;
  unsigned long ms = (unsigned long)(sec * 1000.0f);
  if (ms < 250) ms = 250;
  if (ms > 15000) ms = 15000;
  return ms;
}

void pumpRunMs(unsigned long ms) {
  ledcWrite(PUMP_CH, 255);
  delay(ms);
  ledcWrite(PUMP_CH, 0);
}

void irrigateLiters(float liters) {
  if (liters <= 0.0f) return;

  const unsigned long SPLIT_THRESH_MS = 10000;
  unsigned long ms = msForLiters(liters);

  Serial.print("[관수] 목표량 ");
  Serial.print(liters, 3);
  Serial.print(" L → ");
  Serial.print(ms);
  Serial.println(" ms 가동");

  if (ms <= SPLIT_THRESH_MS) {
    pumpRunMs(ms);
  } else {
    unsigned long half = ms / 2;
    pumpRunMs(half);
    delay(1500);
    pumpRunMs(ms - half);
  }
}

void setLastIrrigUtc(time_t utc) {
  prefs.putLong64(NVS_KEY_LAST_IRRIG, (long long)utc);
}

time_t getLastIrrigUtc() {
  return (time_t)prefs.getLong64(NVS_KEY_LAST_IRRIG, 0);
}

// ===== 서버 통신 함수 =====
void sendSensorData() {
  String sensorData = "";
  sensorData += "온도: " + String(airTemp) + " C\n";
  sensorData += "습도: " + String(airMoist) + " %\n";
  sensorData += "토양습도: " + String(soilMoist) + "\n";
  sensorData += "물수위: " + String(waterLevelPercent) + " %\n";

  Serial.println("전송할 데이터:\n" + sensorData);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(uploadUrl);
    http.addHeader("Content-Type", "text/plain");
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

    int httpCode = http.GET();
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

// ===== 제어 함수 =====
// ✅ 수정: 긴급 급수 전용 함수 (기존 pump 함수)
void emergencyPump() {
  if (pumpDelay >= Pump_Cooltime && soilMoist < DRY && waterRemain > 0) {
    if (waterRemain >= Water_Per_Try) {
      ledcWrite(PUMP_CH, 255);
      delay((Water_Per_Try / Water_Per_Sec) * 1000);
      ledcWrite(PUMP_CH, 0);
      waterRemain -= Water_Per_Try;
      pumpDelay = 0;
      Serial.println("[긴급급수] 실행됨");
    }
    else {
      ledcWrite(PUMP_CH, 255);
      delay((waterRemain / Water_Per_Sec) * 1000);
      ledcWrite(PUMP_CH, 0);
      waterRemain = 0;
      pumpDelay = 0;
      Serial.println("[긴급급수] 남은 물 모두 사용");
    }
  }
}

void fan() {
  if (digitalRead(COOLING_FAN_PIN) == LOW) {
    if (airTemp >= HOT) {
      digitalWrite(COOLING_FAN_PIN, HIGH);
      Serial.println("🌀 팬 ON");
    }
  }
  else {
    if (airTemp < COOL) {
      digitalWrite(COOLING_FAN_PIN, LOW);
      Serial.println("⏸️ 팬 OFF");
    }
  }
}

void wifi_connect() {
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

// ✅ 수정: 조명 제어 주기를 5초마다로 제한
unsigned long last_light_control = 0;
void light_control() {
  // 5초마다만 실행
  if (millis() - last_light_control < 5000) return;
  last_light_control = millis();

  if (!getLocalTime(&ntime)) return;

  int hour = ntime.tm_hour;

  if (hour < START_HOUR) {
    light_hours_count = 0;
    digitalWrite(LED_PIN, LOW);
  }
  else if (hour >= START_HOUR && hour < START_HOUR + LIGHT_GOAL_HOURS) {
    time_t now = time(nullptr);
    int delta = difftime(now, last_light_check);
    last_light_check = now;

    int ldr_value = analogRead(LDR_PIN);
    sunlight = (ldr_value > LDR_THRESHOLD);

    if (light_hours_count < LIGHT_GOAL_HOURS) {
      if (sunlight) {
        digitalWrite(LED_PIN, LOW);
        light_hours_count += delta / 3600.0;
      }
      else {
        digitalWrite(LED_PIN, HIGH);
        light_hours_count += delta / 3600.0;
      }
    }
    else {
      digitalWrite(LED_PIN, LOW);
    }
    
    // 디버그 출력 (5초마다만)
    Serial.printf("💡 조명: %.2fh/16h | LED %s | LDR %d\n",
                  light_hours_count,
                  digitalRead(LED_PIN) ? "ON" : "OFF",
                  ldr_value);
  }
  else {
    digitalWrite(LED_PIN, LOW);
  }
}

// ✅ 수정: 급수 알고리즘 (긴급 급수와 분리)
void water_algorithm() {
  time_t plantEpoch = prefs.getLong64(NVS_KEY_PLANT, 0);
  if (plantEpoch == 0) return;

  int d = daysSince(plantEpoch);
  if (d < 0) return;

  const char* name;
  float Kc, G;
  stageWithLerp(d, Kc, G, name);

  float dailyL = calcDailyNeedL(ET0_MM_DAY, Kc, G);
  float eventL = calcEventVolumeL();
  float daysPeriod = (dailyL > 0) ? (eventL / dailyL) : 0;

  time_t nowUtc;
  if (getNow(nowUtc) && daysPeriod > 0.0f) {
    time_t lastUtc = getLastIrrigUtc();
    double elapsedDays = (lastUtc > 0) ? (difftime(nowUtc, lastUtc) / 86400.0) : 9999.0;

    // ✅ 수정: 알고리즘 급수는 수위가 위험하지 않을 때만
    if (elapsedDays >= daysPeriod && 
        soilMoist < DRY && 
        waterRemain > 0 &&
        waterLevelPercent >= Water_Level_Danger) {  // 위험 수위가 아닐 때만
      Serial.println("[스케줄급수] 관수 주기 도달 → 관수 실행");
      irrigateLiters(eventL);
      setLastIrrigUtc(nowUtc);
    }
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(9600);
  dht.begin();
  
  pinMode(COOLING_FAN_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(PUMP_IN1, OUTPUT);
  pinMode(PUMP_IN2, OUTPUT);

  digitalWrite(PUMP_IN1, HIGH);
  digitalWrite(PUMP_IN2, LOW);

  ledcSetup(PUMP_CH, PUMP_PWM_FREQ, PUMP_PWM_RESOLUTION);
  ledcAttachPin(PUMP_ENA, PUMP_CH);
  ledcWrite(PUMP_CH, 0);

  waterRemain = Water_Day_Limit;
  pumpDelay = 0;
  beepDelay = 0;
  edge_hour = 61;
  edge_minute = 61;

  prefs.begin(NVS_NAMESPACE, false);

  wifi_connect();
  
  // ✅ 수정: NTP 동기화 대기
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.print("NTP 동기화 대기중");
  int retry = 0;
  while (time(nullptr) < 100000 && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }
  Serial.println();
  
  last_light_check = time(nullptr);
  
  getPlantLevel();

  Serial.println("===== 시스템 초기화 완료 =====");
  
  // ✅ 추가: 파종 시각 확인
  time_t plantEpoch = prefs.getLong64(NVS_KEY_PLANT, 0);
  if (plantEpoch == 0) {
    Serial.println("⚠️ 파종 시각 미설정! 시리얼로 'P' 입력하여 설정하세요.");
  } else {
    Serial.printf("✅ 파종 후 %d일 경과\n", daysSince(plantEpoch));
  }
}

// ===== LOOP =====
void loop() {
  getLocalTime(&ntime);

  // ✅ 추가: 시리얼 명령 처리 (파종 시각 설정용)
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'P' || c == 'p') {
      time_t nowUtc;
      if (getNow(nowUtc)) {
        prefs.putLong64(NVS_KEY_PLANT, (long long)nowUtc);
        Serial.println("[저장] 파종 시각 저장 완료!");
      }
    }
  }

  // 매일 0시 - 일일 제한 리셋
  if (ntime.tm_hour != edge_hour && ntime.tm_hour == 0) {
    waterRemain = Water_Day_Limit;
    Serial.println("🔄 일일 급수량 리셋");
  }

  // 30분마다 센서 읽기
  if (ntime.tm_min != edge_minute && ntime.tm_min % 30 == 0) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(h) && !isnan(t)) {
      airTemp = (int)t;
      airMoist = (int)h;
      Serial.printf("🌡️ 온도: %d°C, 습도: %d%%\n", airTemp, airMoist);
    }
    else {
      Serial.println("❌ DHT11 READ ERROR");
    }

    soilMoist = analogRead(DIRT_PIN);
    Serial.printf("🌱 토양습도: %d\n", soilMoist);

    waterLevel = analogRead(WATER_PIN);
    waterLevelPercent = waterLevel / 40.95;
    Serial.printf("💧 물수위: %d (%.1f%%)\n", waterLevel, waterLevelPercent);

    pumpDelay++;
    beepDelay++;
  }

  // ✅ 수정: 1시간 마다 정각에 센서 전송
  if (ntime.tm_hour != edge_hour && ntime.tm_min == 0) {
    sendSensorData();
  }

  // 긴급 급수 (수위 위험 시)
  if (waterLevelPercent < Water_Level_Danger) {
    emergencyPump();
  }

  // 팬 제어
  fan();

  // 조명 제어 (5초 주기)
  light_control();

  // 급수 알고리즘 (정상 급수)
  water_algorithm();

  // WiFi 재연결
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("📡 WiFi 연결 끊김. 재접속 시도.");
    wifi_connect();
  }

  // 서버 폴링 (1초에 1번)
  static unsigned long lastPoll = 0;
  if (millis() - lastPoll > 1000) {
    lastPoll = millis();
    
    HTTPClient http;
    http.begin(serverUrl);
    http.setTimeout(TIMEOUT * 1000);

    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      payload.trim();
      if (payload == "201") {
        Serial.println("📤 서버 요청: 센서 데이터 전송");
        sendSensorData();
      }
    }
    http.end();
  }

  edge_hour = ntime.tm_hour;
  edge_minute = ntime.tm_min;

  delay(1000); 
}
