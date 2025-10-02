// ESP32: Real-time growth stage scheduler (NTP + NVS)
#include <WiFi.h>
#include <time.h>
#include <Preferences.h>

// ===== 펌프 핀 설정 =====
#define PUMP_IN1 16
#define PUMP_IN2 17
#define PUMP_ENA 18

// ===== LEDC PWM 설정 =====
#define PUMP_CH       0      // PWM 채널
#define PUMP_PWM_FREQ 5000   // 5 kHz
#define PUMP_PWM_RES  8      // 8-bit (0~255)
#define PUMP_FULL_DUTY 255   // 최대 듀티

// ===== 펌프 유량 =====
float PUMP_FLOW_LPS = 0.20f; // 0.20 L/s = 200 mL/s (실측 후 교체)

// ===== 안전 제한 =====
const unsigned long PUMP_MIN_MS = 250;   // 최소 동작시간
const unsigned long PUMP_MAX_MS = 15000; // 최대 동작시간

// ===== NVS 저장소 =====
Preferences prefs;
const char* NVS_NAMESPACE = "grow";
const char* NVS_KEY_PLANT = "plant_epoch";
const char* NVS_KEY_LAST_IRRIG = "last_irrig";

// ===== Wi-Fi 설정 =====
const char* WIFI_SSID = "JJY_WIFI";
const char* WIFI_PW   = "62935701";
const long  GMT_OFFSET_SEC = 9 * 3600;   // KST = UTC+9
const int   DST_OFFSET_SEC = 0;

// ===== 화분/모델 파라미터 =====
const float POT_L        = 1.8;   // 배양토 부피 : 1.8L
const float POT_AREA_M2  = 0.24f * 0.24f; // 화분 면적 : 24x24cm
const float AWC_L_PER_L  = 0.20;  // 포팅믹스 유효수분함량
const float P_ALLOW      = 0.40;  // 허용 소모율
float       ET0_MM_DAY   = 2.5;   // 실내 기준 ET0

// ===== 생육 단계 구조체 =====
struct Stage {
  const char* name;
  int start_day;
  int end_day;
  float Kc;
  float G;
};

// ===== 생육 단계 정의 =====
Stage STAGES[] = {
  {"초기(발아~본엽1-2)",   0, 10, 0.50f, 0.20f},
  {"생육중기(엽수 증가)", 11, 25, 0.70f, 0.50f},
  {"수확기(잎 최대)",     26, 45, 0.95f, 0.75f},
};
const int STAGE_COUNT = sizeof(STAGES)/sizeof(STAGES[0]);

// ===== 계산 함수 =====
float calcDailyNeedL(float et0, float kc, float G) {
  float ETc = et0 * kc;
  float effective_area = POT_AREA_M2 * G;
  return ETc * effective_area;
}

float calcEventVolumeL() {
  return POT_L * AWC_L_PER_L * P_ALLOW;
}

// ===== 시간 함수 =====
bool getNow(time_t& nowUtc) {
  struct tm t;
  if (!getLocalTime(&t)) return false;
  time_t local = mktime(&t);
  nowUtc = local - GMT_OFFSET_SEC;
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

// ===== 펌프 제어 =====
unsigned long msForLiters(float liters) {
  if (PUMP_FLOW_LPS <= 0.0f) return 0;
  float sec = liters / PUMP_FLOW_LPS;
  unsigned long ms = (unsigned long)(sec * 1000.0f);
  if (ms < PUMP_MIN_MS) ms = PUMP_MIN_MS;
  if (ms > PUMP_MAX_MS) ms = PUMP_MAX_MS;
  return ms;
}

void pumpRunMs(unsigned long ms) {
  ledcWrite(PUMP_CH, PUMP_FULL_DUTY);
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

// ===== NVS 함수 =====
void setLastIrrigUtc(time_t utc) {
  prefs.putLong64(NVS_KEY_LAST_IRRIG, (long long)utc);
}

time_t getLastIrrigUtc() {
  return (time_t)prefs.getLong64(NVS_KEY_LAST_IRRIG, 0);
}

// ===== 상태 출력 =====
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

void setup() {
  Serial.begin(115200);
  delay(200);

  // WiFi 연결
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

  // 펌프 핀 설정
  pinMode(PUMP_IN1, OUTPUT);
  pinMode(PUMP_IN2, OUTPUT);
  digitalWrite(PUMP_IN1, HIGH);
  digitalWrite(PUMP_IN2, LOW);

  // LEDC 초기화
  ledcSetup(PUMP_CH, PUMP_PWM_FREQ, PUMP_PWM_RES);
  ledcAttachPin(PUMP_ENA, PUMP_CH);
  ledcWrite(PUMP_CH, 0);
  
  Serial.println("===== 급수 알고리즘 테스트 시작 =====");
}

unsigned long lastPrint = 0;

void loop() {
  // 시리얼 명령 처리
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

  // 주기적 상태 출력
  if (millis() - lastPrint > 5000) {
    printStatus();
    lastPrint = millis();
  }

  // 관수 스케줄러
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

        if (elapsedDays >= daysPeriod) {
          Serial.println("[스케줄] 관수 주기 도달 → 관수 실행");
          irrigateLiters(eventL);
          setLastIrrigUtc(nowUtc);
        }
      }
    }
  }

  delay(200);
}
