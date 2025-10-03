// ESP32: Real-time growth stage scheduler (NTP + NVS)
// - 최초 1회: 시리얼에 'P' 입력 → 현재 시각을 파종시각으로 저장
// - 매 loop: 경과일수(days_since)로 단계/계수를 자동 업데이트
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <Preferences.h>

#define PUMP_IN1 16
#define PUMP_IN2 17
#define PUMP_ENA 18

// LEDC PWM 설정
#define PUMP_CH       0      // PWM 채널
#define PUMP_PWM_FREQ 5000   // 5 kHz
#define PUMP_PWM_RES  8      // 8-bit (0~255)
#define PUMP_FULL_DUTY 255   // 최대 듀티

// ---- 펌프 유량 (실측 후 교체 요함) ----
// 0.10 L/s = 100 mL/s
float PUMP_FLOW_LPS = 0.20f;

// 최소/최대 동작시간(안전)
const unsigned long PUMP_MIN_MS = 250;   // 너무 짧은 펄스 방지(예: 0.25초)
const unsigned long PUMP_MAX_MS = 15000; // 한 번에 15초 이상은 분할 권장


//////////////////////// Wi-Fi & Time ////////////////////////
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PW   = "YOUR_PASSWORD";
const long  GMT_OFFSET_SEC = 9 * 3600;   // KST = UTC+9
const int   DST_OFFSET_SEC = 0;

Preferences prefs;
const char* NVS_NAMESPACE = "grow";
const char* NVS_KEY_PLANT = "plant_epoch";  // time_t(UTC seconds)

///////////////////////////////////////////////////////////////

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

//////////////////// Derived – 계산 유틸 //////////////////////
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


///////////////////////////// setup ///////////////////////////

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PW);
  Serial.print("WiFi connecting");
  for (int i=0; i<40 && WiFi.status()!=WL_CONNECTED; ++i) {
    Serial.print(".");
    delay(250);
  }
  Serial.println();
  if (WiFi.status()==WL_CONNECTED) {
    Serial.print("WiFi OK: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi FAIL (시간 동기화 안될 수 있음)");
  }

  configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");

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
  // 회전 방향: IN1=HIGH, IN2=LOW
  digitalWrite(PUMP_IN1, HIGH);
  digitalWrite(PUMP_IN2, LOW);

  // LEDC 초기화
  ledcSetup(PUMP_CH, PUMP_PWM_FREQ, PUMP_PWM_RES);
  ledcAttachPin(PUMP_ENA, PUMP_CH);
  ledcWrite(PUMP_CH, 0); // 펌프 OFF
  
}

///////////////////////////// loop ////////////////////////////
unsigned long lastPrint = 0;

void loop() {
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
        // 초기 전원 인가 직후 바로 급수하고 싶지 않다면 위 9999.0을 0.0으로 바꾸세요.

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

  delay(200);
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
