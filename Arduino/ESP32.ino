#include <Arduino.h>
#include <time.h>
#include "DHT.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>

#define TIMEOUT 5

// ===== 시간 설정 =====
#define START_HOUR 6               // 하루 시작 시각
#define LIGHT_GOAL_HOURS 16        // 하루 채광 목표 (시간)

#define LED_PIN 5 // LED
#define TEMP_PIN 22 // 온습도 센서
#define COOLING_FAN_PIN 33 // Fan
#define WATER_PIN 34 // 수위 센서
#define LDR_PIN 35 // 조도 센서
#define DIRT_PIN 36 // 토양 센서
#define DHTTYPE DHT11 // 온습도 관련
#define PUMP_IN1 16 // 펌프
#define PUMP_IN2 17 // 펌프
#define PUMP_ENA 18 // 펌프

#define HOT 25
#define WET 60
#define DRY 20

// 수위 센서 범위 : 0~4095
// ★★★★★★★★ 팩트 기반 값 필요
#define Water_Level_Caution 40
#define Water_Level_Warning 30
#define Water_Level_Danger 20

// LEDC PWM 설정
#define PUMP_CH       0      // PWM 채널
#define PUMP_PWM_FREQ 5000   // 5 kHz
#define PUMP_PWM_RES  8      // 8-bit (0~255)
#define PUMP_FULL_DUTY 255   // 최대 듀티

// ===== 상태 변수 =====
float light_hours_count = 0;        // 채광 카운터
bool sunlight = false;              // 조도 센서 : 채광 여부

// ---- 펌프 유량 (실측 후 교체 요함) ----
float PUMP_FLOW_LPS = 0.20f; // 0.10 L/s = 100 mL/s

// 펌프 최소/최대 동작시간
const unsigned long PUMP_MIN_MS = 250;   // 너무 짧은 펄스 방지 : 0.25sec
const unsigned long PUMP_MAX_MS = 15000; // 한 번에 15초 이상은 분할

// ===== NTP 서버 설정 =====
const char* ntpServer = "pool.ntp.org";
const long GMT_OFFSET_SEC = 9 * 3600;    // 한국 UTC+9
const int DST_OFFSET_SEC = 0;

// ===== Wi-Fi 설정 =====
const char* ssid = "JJY_WIFI";       // 와이파이 SSID
const char* password = "62935701";   // 와이파이 비밀번호
const char* serverUrl  = "http://116.124.191.174:15020/get?id=ESP32-001";
const char* uploadUrl  = "http://116.124.191.174:15020/upload?id=ESP32-001";
const char* levelUrl   = "http://116.124.191.174:15020/level?id=ESP32-001";

struct tm ntime; // 현재 시간
time_t last_check_time; // LED & 조도 센서 관련
DHT dht(TEMP_PIN, DHTTYPE); // 온습도 관련

int airTemp, airMoist, soilMoist, waterLevel;
int edge_hour, edge_minute;
int hour, minute, second;
int plantLevel;
int loop_count;
float waterLevelPercent;

// 생장 단계 
Preferences prefs;
const char* NVS_NAMESPACE = "grow";
const char* NVS_KEY_PLANT = "plant_epoch";  // time_t(UTC seconds)

// 관수 스케줄: eventL을 며칠 간격으로 줄지 (printStatus에서 구한 daysPeriod 사용)
const char* NVS_KEY_LAST_IRRIG = "last_irrig"; // 마지막 관수 UTC epoch 저장

///////////////////// Pot/Model Parameters ///////////////////
// 화분/토양/모델 기본값. 현재는 재배키트 기준
const float POT_L        = 1.8;   // 배양토 부피 : 1.8L
const float POT_AREA_M2  = 0.24f * 0.24f; // 화분 면적 : 24x24cm
const float AWC_L_PER_L  = 0.20;  // 포팅믹스 유효수분함량 : 흙 자체 고유 값
const float P_ALLOW      = 0.40;  // 허용 소모율 : 흙이 가질 수 있는 물 함량. 흙 자체 고유 값
float       ET0_MM_DAY   = 2.5;   // 실내 기준 ET0. 계절에 맞게 바꿀 여지 있음

// 생육 단계(기간 기준) 구조체
struct Stage {
  const char* name;
  int start_day;   // 파종 후 시작일(포함)
  int end_day;     // 파종 후 종료일(포함)
  float Kc;        // 대표 작물계수
  float G;         // 대표 피복률(0~0.75 권장)
};

// 생육 단계 정의
Stage STAGES[] = {
  {"초기(발아~본엽1-2)",   0, 10, 0.50f, 0.20f},
  {"생육중기(엽수 증가)", 11, 25, 0.70f, 0.50f},
  {"수확기(잎 최대)",     26, 45, 0.95f, 0.75f},
};

const int STAGE_COUNT = sizeof(STAGES)/sizeof(STAGES[0]);

//////////////////// 급수량 계산 유틸 //////////////////////
// 일일 급수량(L) 계산
// ETc(mm/day) × 유효면적(m²) = L/day, 1mm = 1L/m²
float calcDailyNeedL(float et0, float kc, float G) {
  float ETc = et0 * kc; // ETc(작물 증발산량) : 상추가 하루에 실제로 소모하는 물의 양
  float effective_area = POT_AREA_M2 * G;
  return ETc * effective_area;
}
// Event Volume : 흙이 저장할 수 있는 물의 양만큼 물 제공량 설정
float calcEventVolumeL() {
  return POT_L * AWC_L_PER_L * P_ALLOW;
}

///////////////////// 시간/단계 관련 함수 /////////////////////
bool getNow(time_t& nowUtc) {
  struct tm t;
  if (!getLocalTime(&t)) return false; // 로컬타임(UTC+9) struct
  // time_t(UTC seconds)를 얻으려면 mktime(&t) - GMT_OFFSET이 필요
  // mktime은 로컬기준이므로 오프셋을 빼서 UTC로 보정
  time_t local = mktime(&t);
  nowUtc = local - GMT_OFFSET_SEC;
  return true;
}

int daysSince(time_t plantEpochUtc) {
  time_t nowUtc;
  if (!getNow(nowUtc)) return -1;
  double sec = difftime(nowUtc, plantEpochUtc);
  return (int)(sec / 86400.0); // 경과 일수(내림)
}

// 경과일에 맞는 단계 찾기
const Stage* currentStage(int days_since) {
  for (int i=0; i<STAGE_COUNT; ++i) {
    if (days_since >= STAGES[i].start_day && days_since <= STAGES[i].end_day) {
      return &STAGES[i];
    }
  }
  // 범위를 벗어나면 마지막 단계 유지
  return &STAGES[STAGE_COUNT-1];
}

// 생육 단계 변화에 따라 뚝뚝 끊기는 계수(Kc, G)를 보간(stageWithLerp)
void stageWithLerp(int days_since, float& kc_out, float& g_out, const char*& name_out) {
  const Stage* s = currentStage(days_since);
  name_out = s->name;
  // 현재 단계의 진행률(0~1)
  float span = max(1, s->end_day - s->start_day);
  float prog = constrain((days_since - s->start_day) / span, 0.0f, 1.0f);

  // 이웃 단계 찾기(다음 단계와 약간 보간)
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

/////////////////// L(리터)을 ms로 변환 : 실제 펌프 급수량 연동 ////////////////////
unsigned long msForLiters(float liters) {
  if (PUMP_FLOW_LPS <= 0.0f) return 0;
  float sec = liters / PUMP_FLOW_LPS;
  unsigned long ms = (unsigned long)(sec * 1000.0f);
  if (ms < PUMP_MIN_MS) ms = PUMP_MIN_MS;
  if (ms > PUMP_MAX_MS) ms = PUMP_MAX_MS; // 너무 길면 분할 권장
  return ms;
}

// 펌프 ON/OFF (PWM 최대 듀티로 단순 구동)
void pumpRunMs(unsigned long ms) {
  // 방향핀은 setup에서 이미 설정
  ledcWrite(PUMP_CH, PUMP_FULL_DUTY);
  delay(ms);
  ledcWrite(PUMP_CH, 0);
}

// L 단위로 관수 한 번 실행 (필요시 분할 관수)
void irrigateLiters(float liters) {
  if (liters <= 0.0f) return;

  // 길면 분할(예: 10초 이상은 2회로 쪼갬)
  const unsigned long SPLIT_THRESH_MS = 10000; // 10초
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
    delay(1500); // 1.5초 휴지
    pumpRunMs(ms - half);
  }
}

// 마지막 관수시각 저장/조회 (UTC epoch)
void setLastIrrigUtc(time_t utc) {
  prefs.putLong64(NVS_KEY_LAST_IRRIG, (long long)utc);
}
time_t getLastIrrigUtc() {
  return (time_t)prefs.getLong64(NVS_KEY_LAST_IRRIG, 0);
}

void led() {

    hour = ntime.tm_hour;
    minute = ntime.tm_min;
    second = ntime.tm_sec;

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

        // 조도센서 판정 (밝기 임계값은 환경에 맞게 조정 필요)
        int ldr_value = analogRead(LDR_PIN);
        sunlight = (ldr_value < 1000); // ★★★★★★★ 팩트 기반 값 필요
    

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
}       

void fan(){
	if(digitalRead(COOLING_FAN_PIN) == LOW){
    	if(airTemp >= HOT || airMoist >= WET){
		  digitalWrite(COOLING_FAN_PIN, HIGH);
	  	}
  	}
  	else if(airTemp < HOT && airMoist < WET){
		digitalWrite(COOLING_FAN_PIN, LOW);
	}
}

void wifi(){
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
    }
    else {
        Serial.printf("연결 실패 : %s\n", http.errorToString(httpCode).c_str());
    }

    if(loop_count == 5){
        Serial.println("센서값 송신");
        sendSensorData();
        Serial.println("식물 생장 단계 수신");
        getPlantLevel();
        loop_count = 0;
    }
    http.end();
}

// 센서 데이터 보내는 함수
void sendSensorData() {
  // === sensor.txt 문자열 만들기 ===
  String sensorData = "";
  sensorData += "온도: " + String(airTemp) + " C\n";
  sensorData += "습도: " + String(airMoist) + " %\n";
  sensorData += "토양습도: " + String(soilMoist) + "\n";
  sensorData += "물수위: " + String(waterLevelPercent) + " %\n";

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
            Serial.printf("plantLevel = %d\n", plantLevel);
        } else {
            Serial.printf("통신 실패: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
    }
    else {
        Serial.println("wifi 연결 실패로 plantLevel 초기화");
        plantLevel = 0;
    }
}

void wifi_connect(){
    WiFi.begin(ssid, password);
    Serial.print("WiFi connecting");
    int wifi_counter = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        wifi_counter++;
        if(wifi_counter == 2 * 10){
            Serial.println(" failed");
            return;
        }
    }
    Serial.println(" connected!");
}

void sensor(){
    // --- DHT11 온습도 ---
    delay(1000); // 안정화
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
    Serial.printf("물통 수위: %d (%.1f%%)\n", waterLevel, waterLevelPercent);
}

///////////////////////////// setup ///////////////////////////

void setup() {
	Serial.begin(115200);
    dht.begin();
    
    // 제어핀 설정
  	pinMode(COOLING_FAN_PIN, OUTPUT);
  	pinMode(LED_PIN, OUTPUT);

    Serial.println("wifi 연결하는 중");
    wifi_connect();
    delay(10000);

    Serial.println("시간 불러오는 중");
    // configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, ntpServer); // 순서 변경 : wifi_connect 이후
    configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");

    Serial.println("식물 성장 단계 불러오는 중");
    getPlantLevel();
    last_check_time = time(nullptr);
    edge_hour = 61; //0~60분 범위 밖의 값을 사용
    edge_minute = 61;
    loop_count = 0;

	// 급수 파트
    prefs.begin(NVS_NAMESPACE, false);
    time_t plantEpoch = prefs.getLong64(NVS_KEY_PLANT, 0);
    if (plantEpoch == 0) {
      Serial.println("\n[안내] 파종 시각이 저장되어 있지 않습니다.");
      Serial.println("시리얼 창에 대문자 P 입력 → 현재 시각을 파종 시각으로 저장합니다.");
    } else {
      Serial.print("[불러옴] 저장된 파종 UTC epoch: "); Serial.println((long long)plantEpoch);
    }

    Serial.println("\n명령어:");
    Serial.println("  P : 현재 시각을 파종 시각으로 저장");
    Serial.println("  R : 저장된 파종 시각 삭제");
    Serial.println("  E : 현재 파라미터/계산값 출력");

    // 제어핀 설정
    pinMode(PUMP_IN1, OUTPUT);
    pinMode(PUMP_IN2, OUTPUT);
    // 펌프 회전
    digitalWrite(PUMP_IN1, HIGH);
    digitalWrite(PUMP_IN2, LOW);

    // LEDC 초기화
    // 2.x.x 버전 코드
    // ledcSetup(PUMP_CH, PUMP_PWM_FREQ, PUMP_PWM_RES); // 컴파일 오류 시 주석처리
    // ledcAttachPin(PUMP_ENA, PUMP_CH);

    // 3.3.1 버전 코드
    ledcAttach(PUMP_ENA, PUMP_PWM_FREQ, PUMP_PWM_RES);

    ledcWrite(PUMP_CH, 0); // 펌프 OFF
}

///////////////////////////// loop ////////////////////////////
unsigned long lastPrint = 0;
void printStatus();

void loop(){
    if (!getLocalTime(&ntime)) {
        Serial.println("시간 불러오기 실패. 이전 시간 유지.");
        return;
    }

    sensor();
    fan();
    led();
    wifi();

    edge_hour = ntime.tm_hour;
    edge_minute = ntime.tm_min;
    loop_count++;

    delay(60*1000);

    // ===== 시리얼 명령 처리 (P/R/E) =====
    if (Serial.available()) {
      char c = Serial.read();
      if (c=='P') {
        time_t nowUtc;
        if (getNow(nowUtc)) {
          prefs.putLong64(NVS_KEY_PLANT, (long long)nowUtc);
          Serial.println("[저장] 파종 시각 저장 완료.");
        } else {
          Serial.println("[오류] 현재 시각을 얻지 못했습니다(NTP 미동기)");
        }
      } else if (c=='R') {
        prefs.remove(NVS_KEY_PLANT);
        Serial.println("[삭제] 파종 시각 삭제 완료.");
      } else if (c=='E') {
        printStatus();
      }
    }

    // ===== 주기적 상태 출력 =====
  if (millis() - lastPrint > 5000) {
    printStatus();
    lastPrint = millis();
  }

  // ===== 관수 스케줄러 =====
  time_t plantEpoch = prefs.getLong64(NVS_KEY_PLANT, 0);
  if (plantEpoch != 0) {
    int d = daysSince(plantEpoch);
    if (d >= 0) {
      const char* name;
      float Kc, G;
      stageWithLerp(d, Kc, G, name);

      float dailyL    = calcDailyNeedL(ET0_MM_DAY, Kc, G);
      float eventL    = calcEventVolumeL();
      float daysPeriod = (dailyL > 0) ? (eventL / dailyL) : 0;

      time_t nowUtc;
      if (getNow(nowUtc) && daysPeriod > 0.0f) {
        time_t lastUtc = getLastIrrigUtc();
        double elapsedDays = (lastUtc > 0) ? (difftime(nowUtc, lastUtc) / 86400.0) : 9999.0;
        // 초기 전원 인가 직후 바로 급수하고 싶지 않다면 위 9999.0을 0.0으로 바꿀 것.

        // (선택) 토양/수위 조건 넣기: soilMoist, DRY, waterRemain 등의 변수 사용 중이라면 아래 주석 해제
        // bool okSoil  = (soilMoist < DRY);
        // bool okWater = (waterRemain > 0);

        if (elapsedDays >= daysPeriod /* && okSoil && okWater */) {
          Serial.println("[스케줄] 관수 주기 도달 → 관수 실행");
          irrigateLiters(eventL);
          setLastIrrigUtc(nowUtc);
        }
      }
    }
  }

}

///////////////////////////// 출력 ////////////////////////////
void printStatus() {
  time_t plantEpoch = prefs.getLong64(NVS_KEY_PLANT, 0);
  if (plantEpoch == 0) {
    Serial.println("파종 시각 미설정 → 'P' 입력으로 설정하세요.");
    return;
  }
  int d = daysSince(plantEpoch);
  if (d < 0) {
    Serial.println("시간 동기화 전입니다(NTP/RTC 필요).");
    return;
  }

  const char* name;
  float Kc, G;
  stageWithLerp(d, Kc, G, name);

  float dailyL = calcDailyNeedL(ET0_MM_DAY, Kc, G);
  float eventL = calcEventVolumeL();
  // 관수 주기
  float daysPeriod = (dailyL > 0) ? (eventL / dailyL) : 0;

  Serial.println("===== 생육 단계 자동 전환 =====");
  Serial.print("경과일수: "); Serial.println(d);
  Serial.print("현재 단계: "); Serial.println(name);
  Serial.print("Kc="); Serial.print(Kc, 2);
  Serial.print(", G="); Serial.println(G, 2);
  Serial.print("ET0="); Serial.print(ET0_MM_DAY, 2); Serial.println(" mm/day");
  Serial.print("하루 필요수량: "); Serial.print(dailyL, 3); Serial.println(" L/화분");
  Serial.print("권장 1회 급수량: "); Serial.print(eventL, 3); Serial.println(" L/화분");
  Serial.print("관수 주기: "); Serial.print(daysPeriod, 1); Serial.println(" 일/회");
}
