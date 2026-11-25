// Salinan mandiri Smart Greenhouse untuk Arduino IDE.
#include <Arduino.h>
#include <stdint.h>
#include <freertos/semphr.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include <string.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <esp_timer.h>
#include <SD.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

#define WIFI_SSID       "jon"
#define WIFI_PASSWORD   "kkkkkkkk"
#define FIREBASE_API_KEY ""
#define DEVICE_ID       ""
#define DEVICE_EMAIL    ""
#define DEVICE_PASSWORD ""

struct Snapshot;
static void snapshotPublish(const Snapshot& s);
static Snapshot snapshotRead();

// ===== config.h =====
#ifndef CONFIG_H
#define CONFIG_H

// WiFi timeout (ms) + jeda percobaan reconnect berikutnya
#define WIFI_TIMEOUT    15000
#define WIFI_RETRY_MS   10000

#define FIREBASE_DB_URL       "https://smart-greenhouse-e214c-default-rtdb.asia-southeast1.firebasedatabase.app"
#define LOGS_PATH             "/zones/" DEVICE_ID "/sensor_logs"
#define DB_PUSH_MS            20000
#define DB_POLL_MS            5000

#define TOKEN_REFRESH_MS      3300000

#define DHT_PIN         4

#define SOIL_COUNT      3
#define SOIL_PINS       {34, 33, 39}

#define I2C_SDA         21
#define I2C_SCL         22
#define LCD_ADDR        0x27
#define LCD_COLS        16
#define LCD_ROWS        2

#define MIST_RELAY     26
#define BLOWER_PIN      25
#define RELAY_INVERTED  0   // ubah ke 1 untuk relay aktif-LOW
#define PUMP_FILL           27   // tandon air -> tandon pencampuran
#define PUMP_DOSE_A         13   // stok pupuk A -> tandon pencampuran
#define PUMP_DOSE_B         14   // stok pupuk B -> tandon pencampuran
#define PUMP_DIST           32   // tandon pencampuran -> tanaman

// Nilai debit ukur terbaru dari pengguna (belum verifikasi ulang pada alat).
#define FILL_FLOW_RATE       125.36f // L/hour
#define DOSE_A_FLOW_RATE      15.79f // L/hour
#define DOSE_B_FLOW_RATE      15.79f // L/hour
#define DIST_FLOW_RATE       125.36f // L/hour
#define FILL_FLOW_CORRECTION  1.0f
#define DOSE_A_FLOW_CORRECTION 1.0f
#define DOSE_B_FLOW_CORRECTION 1.0f
#define DIST_FLOW_CORRECTION  1.0f
#define L_HOUR_TO_ML_SECOND  (1000.0f / 3600.0f)
#define FILL_FLOW_ML_S       (FILL_FLOW_RATE * L_HOUR_TO_ML_SECOND * FILL_FLOW_CORRECTION)
#define DOSE_A_FLOW_ML_S     (DOSE_A_FLOW_RATE * L_HOUR_TO_ML_SECOND * DOSE_A_FLOW_CORRECTION)
#define DOSE_B_FLOW_ML_S     (DOSE_B_FLOW_RATE * L_HOUR_TO_ML_SECOND * DOSE_B_FLOW_CORRECTION)
#define DIST_FLOW_ML_S       (DIST_FLOW_RATE * L_HOUR_TO_ML_SECOND * DIST_FLOW_CORRECTION)

// Resep Hydro J sesuai instruksi produk (jangan dipakai untuk merek/formulasi lain).
#define DOSE_SEMAI_A_ML_PER_L      0.0f
#define DOSE_SEMAI_B_ML_PER_L      0.0f
#define DOSE_VEGETATIF_A_ML_PER_L  0.0f
#define DOSE_VEGETATIF_B_ML_PER_L  0.0f
#define DOSE_GENERATIF_A_ML_PER_L  0.0f
#define DOSE_GENERATIF_B_ML_PER_L  0.0f
#define FERTILIZER_DOSE_ENABLED 0 // tetap OFF sampai formulasi/dosis Hydro J dikonfirmasi
#define IRRIG_SETTLE_MS      30000UL
#define IRRIG_PHASE_TIMEOUT_MS 600000UL // 10 menit; 18 L pada rate nominal ≈517 s
#define IRRIG_MAX_CYCLE_ML   18000.0f
#define IRRIG_MAX_DOSE_ML    500.0f
#define MIX_TANK_CAPACITY_ML 18000.0f
#define STOCK_A_CAPACITY_ML  6216.0f
#define STOCK_B_CAPACITY_ML  6216.0f
#define IRRIG_CONTROL_TICK_MS 20UL
static_assert(PUMP_FILL != PUMP_DOSE_A && PUMP_FILL != PUMP_DOSE_B &&
              PUMP_FILL != PUMP_DIST && PUMP_DOSE_A != PUMP_DOSE_B &&
              PUMP_DOSE_A != PUMP_DIST && PUMP_DOSE_B != PUMP_DIST,
              "Fertigation pump GPIO assignments must be unique");
static_assert(FILL_FLOW_ML_S > 0 && DOSE_A_FLOW_ML_S > 0 &&
              DOSE_B_FLOW_ML_S > 0 && DIST_FLOW_ML_S > 0,
              "All configured pump flow rates must be positive");
static_assert(PUMP_FILL != MIST_RELAY && PUMP_FILL != BLOWER_PIN &&
              PUMP_DOSE_A != MIST_RELAY && PUMP_DOSE_A != BLOWER_PIN &&
              PUMP_DOSE_B != MIST_RELAY && PUMP_DOSE_B != BLOWER_PIN &&
              PUMP_DIST != MIST_RELAY && PUMP_DIST != BLOWER_PIN &&
              PUMP_FILL != I2C_SDA && PUMP_FILL != I2C_SCL &&
              PUMP_DOSE_A != I2C_SDA && PUMP_DOSE_A != I2C_SCL &&
              PUMP_DOSE_B != I2C_SDA && PUMP_DOSE_B != I2C_SCL &&
              PUMP_DIST != I2C_SDA && PUMP_DIST != I2C_SCL,
              "Pump GPIO conflicts with another configured actuator or I2C");
static_assert(PUMP_FILL != 34 && PUMP_FILL != 33 && PUMP_FILL != 39 &&
              PUMP_DOSE_A != 34 && PUMP_DOSE_A != 33 && PUMP_DOSE_A != 39 &&
              PUMP_DOSE_B != 34 && PUMP_DOSE_B != 33 && PUMP_DOSE_B != 39 &&
              PUMP_DIST != 34 && PUMP_DIST != 33 && PUMP_DIST != 39 &&
              PUMP_FILL != 5 && PUMP_FILL != 18 && PUMP_FILL != 19 && PUMP_FILL != 23 &&
              PUMP_DOSE_A != 5 && PUMP_DOSE_A != 18 && PUMP_DOSE_A != 19 && PUMP_DOSE_A != 23 &&
              PUMP_DOSE_B != 5 && PUMP_DOSE_B != 18 && PUMP_DOSE_B != 19 && PUMP_DOSE_B != 23 &&
              PUMP_DIST != 5 && PUMP_DIST != 18 && PUMP_DIST != 19 && PUMP_DIST != 23,
              "Pump GPIO conflicts with configured soil ADC or SD SPI");
#define IRRIG_AREA_CM2       9000.0f
#define IRRIG_CYCLES_PER_DAY 2
#define IRRIG_AUTO_ENABLED   1

#define SD_CS           5
#define SD_BACKLOG_MAX  100
#define SD_BACKLOG_FLUSH_PER_CALL 20
#define SD_FLUSH_MS     300000
#define SD_LOG_FLUSH_EVERY  30

// Periode kontrol dipertahankan 2 s untuk kestabilan push RTDB dan refresh LCD.
#define LCD_INTERVAL    1000

#define LCD_SCROLL_MS       350
// Jeda di awal dan akhir teks supaya sempat terbaca sebelum mulai bergeser.
#define LCD_SCROLL_HOLD_MS  1500
#define LCD_SCROLL_PRIO     1
#define LCD_SCROLL_STACK    3072   // Wire + LiquidCrystal_I2C + snprintf

static const float SUN_P_MONTH[12] = {
    0.284f, // Januari
    0.280f, // Februari
    0.280f, // Maret
    0.270f, // April
    0.266f, // Mei
    0.266f, // Juni
    0.266f, // Juli
    0.270f, // Agustus
    0.270f, // September
    0.280f, // Oktober
    0.280f, // November
    0.284f  // Desember
};
// Konstanta rumus Blaney-Criddle: ETo = p * (BC_T_COEF * Tmean + BC_T_CONST).
#define BC_T_COEF        0.46f
#define BC_T_CONST       8.0f
#define BC_TMEAN_BUCKET_MS   60000UL   // 1 menit per ember
#define BC_TMEAN_BUCKETS     1440     // 24 jam x 1 menit
// Batas atas ETo (mm/hari) supaya nilai ekstrem tidak dipakai.
#define MAX_ET0        12.0
#define IRRIG_EFFICIENCY 1.0f // tidak ada data efisiensi lapangan; kalibrasikan bila diketahui

#define KC_AWAL           0.40f
#define KC_VEGETATIF      0.75f
#define KC_PEMBUNGAAN     1.10f
#define KC_PEMULIAAN_BUAH 1.00f
#define KC_PANEN          0.90f
#define KC_DEFAULT        KC_VEGETATIF

#define SOIL_DRY_CAL    {2509, 3231, 3255}
#define SOIL_WET_CAL    {913, 1012, 1050}
#define SOIL_RAW_TOL 300
#define SOIL_SAMPLES   4
#define SOIL_SETTLE_MS 10

#define EMA_ALPHA_TEMP  0.2f
#define EMA_ALPHA_HUM   0.2f
#define EMA_ALPHA_SOIL  0.2f

#define VPD_BAND_KPA    0.2f
#define VPD_BOOST_MAX   0.6f
#define VPD_TARGET_MIN  0.3f
#define VPD_TARGET_MAX  2.5f

#define PHASE_DEFAULT_VPD       1.0f       // kPa
#define PHASE_DEFAULT_NAME      "Vegetatif"
#define PHASE_DEFAULT_DURATION  0u         // detik; 0 = tanpa batas
#define NVS_NAMESPACE           "smartgh"

#define MANUAL_MAX_MS           600000UL   // 10 menit

#define RESTART_COOLDOWN_MS     60000UL

// Durasi fase maksimum (detik) = 365 hari, biar tidak overflow uint32.
#define PHASE_MAX_DURATION_SEC  31536000UL

#endif

// ===== phase.h =====
#ifndef PHASE_H
#define PHASE_H

#include <Arduino.h>

#define PHASE_NAME_LEN 24

struct PhaseState {
    bool     active;                     // true bila VPD target sudah pernah di-set
    float    vpdTarget;                  // target VPD (kPa)
    char     name[PHASE_NAME_LEN];       // nama fase (mis. "Vegetatif")
    float    kc;                         // Kc fase ini (Tabel 2.1 proposal)
    uint32_t durationSec;                // durasi fase (detik); 0 = tanpa batas
    uint32_t elapsedSec;                 // sudah berjalan sejak durasi di-set (detik); bertahan setelah power loss bila NTP sudah sync
    uint32_t remainingSec;               // sisa durasi (detik); 0 bila tanpa batas/habis
    bool     clockOk;                    // false = jam belum sinkron tapi penghitung berbasis epoch -> elapsed/remaining tidak diketahui
};

void  phaseInit();                    // muat setelan dari NVS — panggil di setup()
bool  phaseSetVpd(float vpd);         // set VPD target (kPa) -> fase aktif; false bila di luar rentang
void  phaseSetName(const char* name); // set nama fase
void  phaseSetDuration(uint32_t seconds); // set durasi fase; reset penghitung
void  phaseReset();                   // mulai ulang penghitung durasi dari nol
void  phaseDeactivate();              // matikan mode setpoint/fase (kembali penuh ke fuzzy)
bool  phaseActive();                  // ada VPD target aktif?
float phaseVpdTarget();               // VPD target saat ini (kPa)
float phaseKc();                      // Kc fase sekarang (dari nama fase)
float phaseDoseAMlPerL();
float phaseDoseBMlPerL();
PhaseState phaseGet();                // snapshot lengkap (untuk telemetri/DB)

#endif

// ===== irrigation.h =====
#ifndef IRRIGATION_H
#define IRRIGATION_H

#include <Arduino.h>
#include <stdint.h>

// Empat pompa, satu pompa aktif pada satu waktu.
enum IrrigPhase { IRRIG_IDLE, IRRIG_FILL, IRRIG_DOSE_A, IRRIG_DOSE_B, IRRIG_SETTLE, IRRIG_DIST };

// Nama fase ("IDLE", "DIST") untuk log serial dan status_sistem.
const char* irrigationPhaseName();

// Status irigasi untuk telemetri (dashboard) & log.
struct IrrigStatus {
    const char*   phaseName;
    const char*   result;
    float         phaseTargetMl;
    unsigned long phaseDurationMs;
    float         phaseEstimatedMl;
    float         doseAMl;
    float         doseBMl;
    unsigned long fillDurationMs;
    unsigned long doseADurationMs;
    unsigned long doseBDurationMs;
    unsigned long distDurationMs;
    float         doseMl;          // dosis per siklus (mL)
    unsigned long doseMs;          // durasi fase DIST (ms)
    float         budgetMl;        // anggaran harian (mL)
    float         usedMl;          // volume yang sudah dialirkan hari ini (mL)
    unsigned long minIntervalMs;   // jeda minimum antar siklus (ms)
    const char*   holdReason;      // alasan permintaan ditahan ("" = tidak ditahan)
};

void irrigationFeedTemp(float temp);

// Tmean = rerata 24 jam yang sah; NaN sebelum jendela penuh.
float irrigationTmean();

float calculateEtc();

float irrigationDailyBudgetMl();
float irrigationDoseMl();
unsigned long irrigationDoseMs();
unsigned long irrigationMinIntervalMs();
IrrigStatus irrigationStatus();

void irrigationUpdate(bool trigger, bool manual, uint32_t manualRequestId);
bool irrigationRunning();         // ada fase aktif (dipakai status_sistem)
void irrigationAbort();           // sensor tidak valid / darurat -> pompa mati

#endif

// ===== fuzzy.h =====
#ifndef FUZZY_H
#define FUZZY_H

#include <Arduino.h>

struct FuzzyOutput {
    bool mistOn;
    bool blowerOn;
    float blowerLevel;   // level fuzzy 0..1 sebelum diterjemahkan menjadi relay
    bool irigasiOn;      // keputusan WAKTU irigasi (soil x ETc) -> memicu siklus DIST
    const char* status;
};

FuzzyOutput fuzzyDecision(float temp, float humidity, float soilMoisture, float vpd, float etc);

#endif

// ===== control.h =====
#ifndef CONTROL_H
#define CONTROL_H

#include <Arduino.h>
#include <stdint.h>

enum : uint8_t { C_MIST = 1 << 0, C_IRIG = 1 << 1, C_BLOWER = 1 << 2 };

struct OverrideState {
    int8_t   mist, irig, blower;
    uint16_t sisaMist, sisaIrig, sisaBlower;
};

void controlInit();   // buat mutex — panggil sebelum task dibuat
void controlRelease();// semua aktuator kembali ke kendali fuzzy
void controlSet(uint8_t bit, bool on);   // on = paksa nyala, off = kembali ke fuzzy
void controlForce(uint8_t bit, int8_t mode);
void controlApply(bool* mist, bool* irigasi, bool* blower, float* blowerLevel = nullptr);
bool controlIsManual();
OverrideState controlOverrides();   // status override saat ini (untuk telemetri)
uint32_t controlIrrigationRequestId(); // bertambah tiap perintah eksplisit irigasi ON

#endif

// ===== sensor.h =====
#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>

struct SensorData {
    float temperature;
    float humidity;
    float soilMoisture;            // rata-rata probe yang valid (%)
    float soilPct[SOIL_COUNT];     // per probe (%), NAN bila probe tidak masuk akal
    float soilRaw[SOIL_COUNT];     // ADC mentah per probe (untuk kalibrasi)
    bool  valid;        // DHT22 terbaca & suhu/RH masuk rentang
    bool  soilValid;    // minimal satu probe tanah dalam rentang raw yang masuk akal
    uint8_t soilOk;     // bitmask probe valid: bit0=SOIL_PINS[0], bit1=..[1], dst
    uint8_t soilCount;  // jumlah probe valid (0..SOIL_COUNT)
};

void initSensor();
SensorData readSensors();
const char* getSensorError();

void i2cBusInit();                       // sekali di setup(), sebelum initLcd()
bool i2cLock(TickType_t timeout);
void i2cUnlock();

#endif

// ===== filter.h =====
#ifndef FILTER_H
#define FILTER_H

#include <Arduino.h>

struct EmaFilter {
    float alpha;   // 0..1; bobot sampel baru
    float value;   // nilai terfilter saat ini
    bool  seeded;  // false sampai sampel pertama masuk (hindari ramp dari 0)
};

void  emaInit(EmaFilter& f, float alpha);
float emaUpdate(EmaFilter& f, float sample);
void  emaReset(EmaFilter& f);   // mulai lagi dari sampel berikutnya

#endif

// ===== actuator.h =====
#ifndef ACTUATOR_H
#define ACTUATOR_H

#include <Arduino.h>

void initActuators();
void setActuators(bool mist, bool blower, float blowerLevel);

#endif

// ===== sd_logger.h =====
#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <Arduino.h>

void initSdLogger();
void sdLogRecord(const char* line);
void sdQueueBacklog(const char* body);
void sdFlushBacklog();

#endif

// ===== firebase_db.h =====
#ifndef FIREBASE_DB_H
#define FIREBASE_DB_H

#include <Arduino.h>

bool pushSensorLog(float temperature, float humidity, float soil, float soil2, float soil3,
                   uint8_t soilOk, uint8_t soilCount,
                   float vpd, float etc,
                   float blowerLevel, bool misting, bool irigasi,
                   const char* statusSistem, bool manualMode, const PhaseState& phase,
                   const OverrideState& ov, float tmean, const IrrigStatus& irig,
                   const FuzzyOutput& fuzzy,
                   String* bodyOut = nullptr);
// POST body JSON apa pun ke /sensor_logs (untuk backfill backlog MicroSD).
bool httpPostSensorLogs(const String& body);
void pollCommands();

#endif

// ===== lcd_display.h =====
#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include <Arduino.h>

void initLcd();
void updateLcd(float temp, float humidity, float soilMoisture, float vpd, float etc);

void lcdScrollStart();
void lcdScrollSetStatus(const char* status);

#endif

// ===== actuator.cpp =====

void setActuators(bool mist, bool blower, float blowerLevel) {
    (void)blowerLevel; // Relay tidak mendukung PWM.
    digitalWrite(MIST_RELAY, (mist ^ RELAY_INVERTED) ? HIGH : LOW);
    digitalWrite(BLOWER_PIN, (blower ^ RELAY_INVERTED) ? HIGH : LOW);
}

void initActuators() {
    const uint8_t relays[] = {MIST_RELAY, BLOWER_PIN, PUMP_FILL,
                              PUMP_DOSE_A, PUMP_DOSE_B, PUMP_DIST};
    for (uint8_t pin : relays) {
        digitalWrite(pin, RELAY_INVERTED ? HIGH : LOW);
        pinMode(pin, OUTPUT);
    }
    setActuators(false, false, 0.0f);
}

// ===== control.cpp =====

#include <freertos/semphr.h>

static SemaphoreHandle_t controlMux = nullptr;
static int8_t      ovMist = -1, ovIrig = -1, ovBlower = -1;
static unsigned long tMist = 0, tIrig = 0, tBlower = 0;   // millis override aktif
static uint32_t irrigationRequestId = 0;

static void controlLock()   { if (controlMux) xSemaphoreTake(controlMux, portMAX_DELAY); }
static void controlUnlock() { if (controlMux) xSemaphoreGive(controlMux); }

static bool anyOverride() {
    return ovMist >= 0 || ovIrig >= 0 || ovBlower >= 0;
}

static void expireManual() {
    unsigned long now = millis();
    const char* nama = "";
    if (ovMist >= 0 && now - tMist >= MANUAL_MAX_MS) {
        ovMist = -1; tMist = 0; nama = "misting";
    } else if (ovIrig >= 0 && now - tIrig >= MANUAL_MAX_MS) {
        ovIrig = -1; tIrig = 0; nama = "irigasi";
    } else if (ovBlower >= 0 && now - tBlower >= MANUAL_MAX_MS) {
        ovBlower = -1; tBlower = 0; nama = "blower";
    } else {
        return;
    }
    Serial.printf("[ctl] override manual %s habis (%lu menit) - kembali ke fuzzy\n",
                  nama, (unsigned long)(MANUAL_MAX_MS / 60000UL));
}

void controlInit() {
    if (!controlMux) controlMux = xSemaphoreCreateMutex();
}

void controlRelease() {
    controlLock();
    ovMist = ovIrig = ovBlower = -1;
    tMist = tIrig = tBlower = 0;
    controlUnlock();
    Serial.println("[ctl] auto: semua aktuator kembali ke kendali fuzzy");
}

void controlForce(uint8_t bit, int8_t mode) {
    if (mode < -1) mode = -1;
    if (mode > 1)  mode = 1;
    controlLock();
    unsigned long stamp = (mode == -1) ? 0 : millis();
    const char* nama = "";
    if (bit & C_MIST)   { ovMist = mode; tMist = stamp; nama = "misting"; }
    if (bit & C_IRIG)   {
        ovIrig = mode; tIrig = stamp; nama = "irigasi";
        if (mode == 1) irrigationRequestId++;
    }
    if (bit & C_BLOWER) { ovBlower = mode; tBlower = stamp; nama = "blower"; }
    controlUnlock();
    Serial.printf("[ctl] override %s -> %s\n", nama,
                  mode == 1 ? "paksa nyala"
                            : (mode == 0 ? "paksa mati" : "kembali ke fuzzy"));
}

void controlSet(uint8_t bit, bool on) {
    controlForce(bit, on ? 1 : -1);
}

void controlApply(bool* mist, bool* irigasi, bool* blower, float* blowerLevel) {
    controlLock();
    expireManual();
    if (ovMist >= 0)   *mist   = (ovMist == 1);
    if (ovIrig >= 0)   *irigasi = (ovIrig == 1);
    if (ovBlower >= 0) {
        *blower = (ovBlower == 1);
        if (blowerLevel) *blowerLevel = (ovBlower == 1) ? 1.0f : 0.0f;
    }
    controlUnlock();
}

bool controlIsManual() {
    controlLock();
    expireManual();
    bool r = anyOverride();
    controlUnlock();
    return r;
}

static uint16_t sisaDetik(int8_t ov, unsigned long t, unsigned long now) {
    if (ov < 0) return 0;
    unsigned long lewat = now - t;
    if (lewat >= MANUAL_MAX_MS) return 0;
    return (uint16_t)((MANUAL_MAX_MS - lewat) / 1000UL);
}

OverrideState controlOverrides() {
    controlLock();
    expireManual();
    unsigned long now = millis();
    OverrideState ov = {
        ovMist, ovIrig, ovBlower,
        sisaDetik(ovMist,   tMist,   now),
        sisaDetik(ovIrig,   tIrig,   now),
        sisaDetik(ovBlower, tBlower, now),
    };
    controlUnlock();
    return ov;
}

uint32_t controlIrrigationRequestId() {
    controlLock();
    uint32_t id = irrigationRequestId;
    controlUnlock();
    return id;
}

// ===== filter.cpp =====

void emaInit(EmaFilter& f, float alpha) {
    f.alpha = constrain(alpha, 0.0f, 1.0f);
    f.value = 0.0f;
    f.seeded = false;
}

void emaReset(EmaFilter& f) {
    f.value = 0.0f;
    f.seeded = false;
}

float emaUpdate(EmaFilter& f, float sample) {
    if (!f.seeded) {
        f.value = sample;      // sampel pertama mengisi filter (hindari ramp dari 0)
        f.seeded = true;
    } else {
        f.value += f.alpha * (sample - f.value);
    }
    return f.value;
}

// ===== firebase_db.cpp =====

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include <string.h>

static WiFiClientSecure client;
static HTTPClient http;

static String idToken;
static unsigned long tokenExpiryMs = 0;

static bool ensureToken() {
    if (idToken.length() > 0 && millis() < tokenExpiryMs) return true;
    if (WiFi.status() != WL_CONNECTED) return false;

    client.setInsecure();
    String url = String("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=")
                 + FIREBASE_API_KEY;
    JsonDocument req;
    req["email"] = DEVICE_EMAIL;
    req["password"] = DEVICE_PASSWORD;
    req["returnSecureToken"] = true;
    String body;
    serializeJson(req, body);

    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    String payload = http.getString();
    http.end();

    if (code != 200) {
        Serial.printf("[fb] login gagal: HTTP %d %s\n", code, payload.c_str());
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, payload) || doc["idToken"].isNull()) {
        Serial.println("[fb] login gagal: idToken tidak ada di respons");
        return false;
    }
    idToken = doc["idToken"].as<String>();
    // expiresIn dalam detik; kurangi margin supaya tidak expired saat dipakai.
    long expiresIn = doc["expiresIn"] | 3600L;
    tokenExpiryMs = millis() + (unsigned long)(expiresIn * 1000L) - TOKEN_REFRESH_MS;
    Serial.printf("[fb] login perangkat OK (%s), token %d menit\n",
                  DEVICE_EMAIL, (int)(expiresIn / 60));
    return true;
}

// Paksa login ulang (dipakai saat server menolak dengan 401).
static void invalidateToken() {
    idToken = "";
    tokenExpiryMs = 0;
}

static Preferences prefsCmd;
static bool     restartStateLoaded = false;
static time_t   lastRestartEpoch = 0;      // epoch detik restart terakhir
static unsigned long lastRestartMs = 0;     // fallback kalau jam belum sinkron

static String lastCmdKey;
static bool   lastCmdLoaded = false;

static void loadLastCmd() {
    if (lastCmdLoaded) return;
    lastCmdLoaded = true;
    prefsCmd.begin("smartgh_cmd", true);
    lastCmdKey = prefsCmd.getString("last_cmd", "");
    prefsCmd.end();
    if (lastCmdKey.length() < 10) lastCmdKey = "";
}

// Timestamp Firebase push-ID tersimpan di 8 karakter pertama.
static bool pushKeyTimestampMs(const char* key, uint64_t* timestamp) {
    static const char alphabet[] = "-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";
    if (!key || strlen(key) < 8) return false;
    uint64_t value = 0;
    for (int i = 0; i < 8; i++) {
        const char* digit = strchr(alphabet, key[i]);
        if (!digit) return false;
        value = (value << 6) | (uint64_t)(digit - alphabet);
    }
    *timestamp = value;
    return true;
}

static void repairFutureLastCmd(JsonObject cmds) {
    if (!lastCmdKey.length()) return;
    time_t now = time(nullptr);
    uint64_t savedMs;
    if (now < 100000 || !pushKeyTimestampMs(lastCmdKey.c_str(), &savedMs) ||
        savedMs <= ((uint64_t)now + 300) * 1000ULL) return;

    const char* newestStale = nullptr;
    uint64_t staleBefore = (uint64_t)now * 1000ULL;
    if (staleBefore > 60000ULL) staleBefore -= 60000ULL;
    for (JsonPair kv : cmds) {
        if (!kv.value().is<JsonObject>()) continue;
        const char* key = kv.key().c_str();
        uint64_t queuedMs;
        if (!pushKeyTimestampMs(key, &queuedMs) || queuedMs >= staleBefore) continue;
        if (!newestStale || strcmp(key, newestStale) > 0) newestStale = key;
    }

    lastCmdKey = newestStale ? newestStale : "";
    prefsCmd.begin("smartgh_cmd", false);
    prefsCmd.putString("last_cmd", lastCmdKey);
    prefsCmd.end();
    Serial.printf("[fb] lastCmdKey masa depan dipulihkan; baseline=%s (perintah lama dilewati)\n",
                  lastCmdKey.length() ? lastCmdKey.c_str() : "kosong");
}

static void loadRestartState() {
    if (restartStateLoaded) return;
    restartStateLoaded = true;
    prefsCmd.begin("smartgh_cmd", true);
    lastRestartEpoch = (time_t)prefsCmd.getULong("last_restart", 0);
    bool pending = prefsCmd.getBool("restart_pending", false);
    prefsCmd.end();
    if (pending && lastRestartEpoch < 100000) lastRestartMs = millis();
}

static bool restartBlocked() {
    loadRestartState();
    time_t now = time(nullptr);
    if (now >= 100000 && lastRestartEpoch > 0) {
        return (now - lastRestartEpoch) < (time_t)(RESTART_COOLDOWN_MS / 1000UL);
    }
    bool blocked = lastRestartMs != 0 && (millis() - lastRestartMs) < RESTART_COOLDOWN_MS;
    if (!blocked) {
        prefsCmd.begin("smartgh_cmd", false);
        prefsCmd.putBool("restart_pending", false);
        prefsCmd.end();
    }
    return blocked;
}

static void markRestart() {
    lastRestartMs = millis();
    lastRestartEpoch = time(nullptr);
    prefsCmd.begin("smartgh_cmd", false);
    prefsCmd.putULong("last_restart", (unsigned long)lastRestartEpoch);
    prefsCmd.putBool("restart_pending", true);
    prefsCmd.end();
}

static bool httpPost(const String& path, const String& body) {
    if (!ensureToken()) return false;
    client.setInsecure();
    String url = String(FIREBASE_DB_URL) + path + ".json?auth=" + idToken;
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    http.end();
    Serial.printf("[fb][diag] POST HTTP %d\n", code);
    if (code == 401) invalidateToken();   // token expired/kadaluarsa
    return code >= 200 && code < 300;
}

static void putNum(JsonDocument& doc, const char* key, float v) {
    if (isnan(v) || isinf(v)) return;
    doc[key] = v;
}

static const char* ovName(int8_t ov) {
    if (ov < 0) return "auto";
    return (ov == 1) ? "manual" : "forced";
}

bool pushSensorLog(float temperature, float humidity, float soil, float soil2, float soil3,
                   uint8_t soilOk, uint8_t soilCount,
                   float vpd, float etc,
                   float blowerLevel, bool misting, bool irigasi,
                   const char* statusSistem, bool manualMode, const PhaseState& phase,
                   const OverrideState& ov, float tmean, const IrrigStatus& irig,
                   const FuzzyOutput& fuzzy, String* bodyOut) {
    time_t ts = time(nullptr);
    bool timeOk = (ts >= 100000);

    JsonDocument doc;
    putNum(doc, "temperature", temperature);
    putNum(doc, "humidity", humidity);
    putNum(doc, "soil", soil);
    putNum(doc, "soil2", soil2);
    putNum(doc, "soil3", soil3);
    // Status per-probe eksplisit: 0/0 = semua probe mati, bukan record lama.
    doc["soil_ok"] = (int)soilOk;
    doc["soil_n"] = (int)soilCount;
    putNum(doc, "vpd", vpd);
    putNum(doc, "etc", etc);
    putNum(doc, "etc_mm_day", etc);
    float eto = phase.kc > 0.0f ? etc / phase.kc : NAN;
    putNum(doc, "et0_mm_day", eto);
    putNum(doc, "et0", etc); // legacy dashboard alias retained for compatibility
    putNum(doc, "blower_level", blowerLevel);
    doc["misting"] = misting;
    doc["irigasi"] = irigasi;
    doc["status_sistem"] = statusSistem;
    doc["manual_mode"] = manualMode;
    doc["fuzzy_mist"] = fuzzy.mistOn;
    doc["fuzzy_blower"] = fuzzy.blowerOn;
    doc["fuzzy_irigasi"] = fuzzy.irigasiOn;
    // Status override per aktuator (sakelar dashboard).
    doc["mist_ov"] = ovName(ov.mist);
    doc["irig_ov"] = ovName(ov.irig);
    doc["blower_ov"] = ovName(ov.blower);
    if (ov.mist   >= 0) doc["mist_ov_sisa_s"]   = ov.sisaMist;
    if (ov.irig   >= 0) doc["irig_ov_sisa_s"]   = ov.sisaIrig;
    if (ov.blower >= 0) doc["blower_ov_sisa_s"] = ov.sisaBlower;
    // Telemetri fase & VPD target (di-set admin dari dashboard).
    doc["fase"] = phase.name;
    doc["vpd_target"] = phase.vpdTarget;
    doc["fase_aktif"] = phase.active;
    doc["fase_durasi_s"] = phase.durationSec;
    if (phase.clockOk) {
        doc["fase_elapsed_s"] = phase.elapsedSec;
        doc["fase_sisa_s"] = phase.remainingSec;
    }
    doc["kc_aktif"] = phase.kc;
    putNum(doc, "tmean_c", tmean);
    doc["fertigation_phase"] = irig.phaseName ? irig.phaseName : "IDLE";
    doc["fertigation_result"] = irig.result ? irig.result : "IDLE";
    putNum(doc, "fertigation_target_ml", irig.phaseTargetMl);
    putNum(doc, "fertigation_phase_duration_ms", (float)irig.phaseDurationMs);
    putNum(doc, "fertigation_estimated_ml", irig.phaseEstimatedMl);
    putNum(doc, "fertigation_dose_a_ml", irig.doseAMl);
    putNum(doc, "fertigation_dose_b_ml", irig.doseBMl);
    putNum(doc, "fertigation_fill_duration_ms", (float)irig.fillDurationMs);
    putNum(doc, "fertigation_dose_a_duration_ms", (float)irig.doseADurationMs);
    putNum(doc, "fertigation_dose_b_duration_ms", (float)irig.doseBDurationMs);
    putNum(doc, "fertigation_dist_duration_ms", (float)irig.distDurationMs);
    putNum(doc, "fertigation_kc", phase.kc);
    putNum(doc, "fertigation_soil_pct", soil);
    putNum(doc, "irigasi_dosis_ml", irig.doseMl);
    putNum(doc, "irigasi_durasi_ms", (float)irig.doseMs);
    putNum(doc, "irigasi_anggaran_ml", irig.budgetMl);
    putNum(doc, "irigasi_terpakai_ml", irig.usedMl);
    putNum(doc, "irigasi_jeda_menit", (float)(irig.minIntervalMs / 60000UL));
    doc["irigasi_alasan_tahan"] = irig.holdReason ? irig.holdReason : "";
    doc["time_synced"] = timeOk;
    if (timeOk) doc["ts"] = ts;
    doc["device"] = DEVICE_ID;
    String body;
    serializeJson(doc, body);
    if (bodyOut) *bodyOut = body;

    // Body sudah jadi: netTask tetap bisa mengantrekannya ke SD saat offline.
    if (WiFi.status() != WL_CONNECTED) return false;

    bool ok = httpPost(LOGS_PATH, body);
    Serial.printf("[fb][diag] sensor POST result=%s\n", ok ? "accepted" : "failed");
    if (!ok) Serial.println("[fb] push log gagal (akan diantrekan ke SD)");
    return ok;
}

bool httpPostSensorLogs(const String& body) {
    if (WiFi.status() != WL_CONNECTED) return false;
    return httpPost(LOGS_PATH, body);
}

static void applyActuator(uint8_t bit, int8_t mode, bool on) {
    controlForce(bit, (mode == -2) ? (on ? 1 : -1) : mode);
}

void pollCommands() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[fb][diag] commands GET skipped: WiFi disconnected");
        return;
    }
    if (!ensureToken()) {
        Serial.println("[fb][diag] commands GET skipped: token unavailable");
        return;
    }

    client.setInsecure();
    String url = String(FIREBASE_DB_URL) + "/commands/" + DEVICE_ID + ".json?auth=" + idToken;
    http.begin(client, url);
    int code = http.GET();
    Serial.printf("[fb][diag] commands GET HTTP %d\n", code);
    if (code != 200) {
        if (code == 401) invalidateToken();
        http.end();
        return;
    }
    String payload = http.getString();
    http.end();
    if (payload == "null" || payload == "{}") {
        Serial.println("[fb][diag] commands parsed count=0");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    JsonObject cmds = doc.as<JsonObject>();
    if (err || cmds.size() == 0) {
        Serial.printf("[fb][diag] commands parse failed=%s count=0\n", err ? err.c_str() : "empty");
        return;
    }
    Serial.printf("[fb][diag] commands parsed count=%u\n", (unsigned)cmds.size());

    loadLastCmd();
    repairFutureLastCmd(cmds);
    const char* oldest = nullptr;
    JsonObject target;    for (JsonPair kv : cmds) {
        if (!kv.value().is<JsonObject>()) {
            Serial.printf("[fb][diag] command skipped key=%s reason=not_object\n", kv.key().c_str());
            continue;
        }
        const char* k = kv.key().c_str();
        if (lastCmdKey.length() && strcmp(k, lastCmdKey.c_str()) <= 0) {
            Serial.printf("[fb][diag] command skipped key=%s reason=lastCmdKey last=%s\n",
                          k, lastCmdKey.c_str());
            continue;
        }
        if (oldest == nullptr || strcmp(k, oldest) < 0) {
            oldest = k;
            target = kv.value().as<JsonObject>();
        }
    }

    if (oldest == nullptr) {
        Serial.println("[fb][diag] no eligible command after lastCmdKey");
        return;
    }

    const char* action = target["action"] | "";
    Serial.printf("[fb][diag] selected key=%s action=%s\n", oldest, action);
    JsonVariant value = target["value"];
    bool on = value | false;
    bool isRestart = strcmp(action, "restart") == 0;

    const char* modeStr = target["mode"] | "";
    int8_t mode = -2;
    if (strcmp(modeStr, "on") == 0)        mode = 1;
    else if (strcmp(modeStr, "off") == 0)  mode = 0;
    else if (strcmp(modeStr, "auto") == 0) mode = -1;
    else if (modeStr[0] != '\0') {
        Serial.printf("[fb] mode tidak dikenal: %s (pakai on/off/auto)\n", modeStr);
    }

    if (strcmp(action, "auto") == 0) {
        controlRelease();
    } else if (strcmp(action, "mist") == 0) {
        applyActuator(C_MIST, mode, on);
    } else if (strcmp(action, "irigasi") == 0) {
        applyActuator(C_IRIG, mode, on);
    } else if (strcmp(action, "blower") == 0) {
        applyActuator(C_BLOWER, mode, on);
    } else if (strcmp(action, "set_vpd") == 0) {
        if (value.is<int>() || value.is<float>()) {
            float v = value.as<float>();
            if (phaseSetVpd(v)) {
                Serial.printf("[fb] set VPD target: %.2f kPa\n", v);
            } else {
                Serial.printf("[fb] set_vpd ditolak: %.2f kPa di luar rentang %.1f-%.1f kPa\n",
                              v, VPD_TARGET_MIN, VPD_TARGET_MAX);
            }
        } else {
            Serial.println("[fb] set_vpd: nilai tidak valid");
        }
    } else if (strcmp(action, "set_fase") == 0 || strcmp(action, "set_phase") == 0) {
        const char* nm = value | "";
        if (value.isNull() || nm[0] == '\0') {
            phaseDeactivate();
            Serial.println("[fb] fase nonaktif - keputusan sepenuhnya kembali ke fuzzy");
        } else {
            phaseSetName(nm);
            Serial.printf("[fb] set fase: %s\n", nm);
        }
    } else if (strcmp(action, "set_durasi") == 0 || strcmp(action, "set_duration") == 0) {
        float num = value | 0.0f;
        const char* unit = target["unit"] | "detik";
        double sec = num;
        if (strcmp(unit, "menit") == 0)      sec = num * 60.0;
        else if (strcmp(unit, "jam") == 0)   sec = num * 3600.0;
        else if (strcmp(unit, "hari") == 0)  sec = num * 86400.0;
        // Cek overflow uint32 + nilai tidak masuk akal sebelum dikonversi.
        if (isnan(num) || num < 0.0f || sec > (double)PHASE_MAX_DURATION_SEC) {
            Serial.printf("[fb] set_durasi ditolak: %.2f %s (di luar 0..%lu detik)\n",
                          num, unit, (unsigned long)PHASE_MAX_DURATION_SEC);
        } else {
            phaseSetDuration((uint32_t)sec);
            Serial.printf("[fb] set durasi fase: %.0f %s (%lu detik)\n",
                          num, unit, (unsigned long)sec);
        }
    } else if (strcmp(action, "fase_reset") == 0 || strcmp(action, "phase_reset") == 0) {
        phaseReset();
        Serial.println("[fb] fase direset (penghitung durasi mulai dari nol)");
    } else if (!isRestart) {
        Serial.printf("[fb] perintah tidak dikenal: %s\n", action);
    }
    // Log ringkas hanya untuk perintah aktuator (set_* sudah mencetak sendiri).
    if (strcmp(action, "auto") == 0 || strcmp(action, "mist") == 0 ||
        strcmp(action, "irigasi") == 0 || strcmp(action, "blower") == 0) {
        const char* modeLog = (mode == 1) ? "on" : (mode == 0 ? "off" : "auto");
        Serial.printf("[fb] perintah diterima: %s %s\n", action, modeLog);
    }

    Serial.printf("[fb][diag] apply dispatch complete key=%s action=%s\n", oldest, action);

    client.setInsecure();
    String delUrl = String(FIREBASE_DB_URL) + "/commands/" + DEVICE_ID + "/" + oldest
                    + ".json?auth=" + idToken;
    http.begin(client, delUrl);
    int delCode = http.sendRequest("DELETE", "");
    http.end();
    Serial.printf("[fb][diag] commands DELETE key=%s HTTP %d\n", oldest, delCode);

    lastCmdKey = oldest;
    prefsCmd.begin("smartgh_cmd", false);
    prefsCmd.putString("last_cmd", lastCmdKey);
    prefsCmd.end();

    bool delOk = delCode >= 200 && delCode < 300;
    if (isRestart) {
        if (!delOk) {
            Serial.printf("[fb] restart dibatalkan: DELETE perintah gagal (HTTP %d)\n", delCode);
        } else if (restartBlocked()) {
            // Cooldown: reboot berulang memutus siklus irigasi.
            Serial.printf("[fb] restart ditolak: cooldown %lu s\n",
                          (unsigned long)(RESTART_COOLDOWN_MS / 1000UL));
        } else {
            markRestart();
            Serial.println("[fb] perintah restart, perangkat restart...");
            delay(200);   // beri waktu DELETE selesai di sisi server
            ESP.restart();
        }
    } else if (!delOk) {
        static bool deleteWarned = false;
        if (!deleteWarned) {
            deleteWarned = true;
            Serial.printf("[fb] DELETE perintah gagal (HTTP %d) - cek jaringan/rules /commands\n", delCode);
        }
    }
}

// ===== fuzzy.cpp =====

static float trimf(float x, float a, float b, float c) {
    if (x <= a || x >= c) return 0.0;
    if (x <= b) return (x - a) / (b - a);
    return (c - x) / (c - b);
}

static float tempCold(float t)    { return trimf(t, 10, 18, 25); }
static float tempOptimal(float t) { return trimf(t, 22, 27, 32); }
static float tempHot(float t)     { return trimf(t, 28, 34, 42); }

static float humLow(float h)     { return trimf(h, 0, 30, 55); }
static float humOptimal(float h) { return trimf(h, 45, 65, 80); }
static float humHigh(float h)    { return trimf(h, 70, 85, 100); }

static float fuzzySoilDry(float s)     { return trimf(s, 0, 15, 35); }
static float soilOptimal(float s) { return trimf(s, 30, 50, 70); }
static float fuzzySoilWet(float s)     { return trimf(s, 60, 80, 100); }

static float vpdLow(float v)  { return trimf(v, 0.0, 0.2, 0.5); }
static float vpdHigh(float v) { return trimf(v, 1.3, 1.6, 2.0); }

static float etcLow(float e)  { return trimf(e, 0.0, 4.0, 8.0); }
static float etcHigh(float e) { return trimf(e, 5.0, 9.0, 18.0); }

static const float MIST_LOW = 0.3, MIST_MED = 0.6, MIST_HIGH = 1.0;
static const float BLOWER_LOW = 0.4, BLOWER_MED = 0.7, BLOWER_HIGH = 1.0;
static const float IRR_LOW = 0.5, IRR_HIGH = 1.0;

enum { T_COLD, T_OPT, T_HOT, H_LOW, H_OPT, H_HIGH, S_DRY, S_OPT, S_WET, V_LOW, V_HIGH, E_LOW, E_HIGH };

struct Rule { int a, b; float lvl; };

static const Rule MIST_RULES[] = {
    { T_HOT, S_DRY, MIST_HIGH },
    { T_HOT, S_OPT, MIST_MED  },
    { T_HOT, S_WET, MIST_LOW  },
    { T_HOT, V_HIGH, MIST_HIGH },    // udara kering (VPD stres) + panas → mist penuh
    { T_OPT, S_DRY, MIST_MED  },
    { T_OPT, V_HIGH, MIST_MED  },
    { T_COLD, T_COLD, 0.0 },         // dingin → off
    { T_OPT, S_WET, 0.0 },           // tanah basah → off
    { V_LOW, V_LOW, 0.0 },           // udara sudah jenuh → off (anti jamur daun)
};

static const Rule BLOWER_RULES[] = {
    { T_HOT, H_HIGH, BLOWER_HIGH },
    { T_OPT, H_HIGH, BLOWER_MED  },
    { T_HOT, H_LOW,  BLOWER_LOW  },
    { T_HOT, H_OPT,  BLOWER_MED  },
    { T_OPT, H_OPT,  BLOWER_LOW  },
    { V_LOW, V_LOW,  BLOWER_LOW  },  // udara jenuh → ventilasi turunkan kelembapan
    { T_COLD, T_COLD, 0.0 },
};

static const Rule IRRIG_RULES[] = {
    { S_DRY, E_HIGH, IRR_HIGH },  // tanah kering + evapotranspirasi tinggi → siram penuh
    { S_DRY, E_LOW,  IRR_LOW  },  // tanah kering, ET rendah → siram sedang
    { S_OPT, E_HIGH, IRR_LOW  },  // tanah cukup, ET tinggi → siram sedang
    { S_OPT, E_LOW,  0.0 },
    { S_WET, S_WET,  0.0 },       // tanah basah → jangan siram (anti becek/leaching)
};

static float evalRules(const Rule* rules, size_t n, const float* mu);
static float evalRules(const Rule* rules, size_t n, const float* mu) {
    float sum = 0, weight = 0;
    for (size_t i = 0; i < n; i++) {
        float w = min(mu[rules[i].a], mu[rules[i].b]);
        sum += w * rules[i].lvl;
        weight += w;
    }
    return (weight > 0) ? sum / weight : 0.0;
}

FuzzyOutput fuzzyDecision(float temp, float humidity, float soilMoisture, float vpd, float etc) {
    float mu[] = {
        tempCold(temp), tempOptimal(temp), tempHot(temp),
        humLow(humidity), humOptimal(humidity), humHigh(humidity),
        fuzzySoilDry(soilMoisture), soilOptimal(soilMoisture), fuzzySoilWet(soilMoisture),
        vpdLow(vpd), vpdHigh(vpd),
        etcLow(etc), etcHigh(etc)
    };

    float mistLevel     = evalRules(MIST_RULES,   sizeof(MIST_RULES)   / sizeof(Rule), mu);
    float blowerLevel = evalRules(BLOWER_RULES, sizeof(BLOWER_RULES) / sizeof(Rule), mu);
    float irigasiLevel = evalRules(IRRIG_RULES,  sizeof(IRRIG_RULES)  / sizeof(Rule), mu);

    const char* status;
    if (temp > 34 || temp < 10) status = "Kritis";
    else if (temp > 28 || humidity > 80) status = "Warning";
    else if (temp >= 22 && temp <= 30 && humidity >= 45 && humidity <= 75) status = "Normal";
    else status = "Monitor";

    FuzzyOutput out;
    out.mistOn = mistLevel > 0.1;
    out.blowerOn = blowerLevel > 0.1;
    out.blowerLevel = blowerLevel;
    out.irigasiOn = irigasiLevel > 0.1;
    out.status = status;
    return out;
}

// ===== irrigation.cpp =====

#include <Preferences.h>
#include <time.h>

static IrrigPhase phase = IRRIG_IDLE;
static uint32_t phaseStart = 0;
static uint32_t phaseDurationMs = 0;
static float cycleWaterMl = 0.0f;
static float cycleDoseAMlPerL = 0.0f, cycleDoseBMlPerL = 0.0f;
static float usedMl = 0.0f;
static uint32_t usedDay = 0;
static bool budgetLoaded = false;
static uint32_t lastManualRequestId = 0;
static char holdReason[48] = "";
static const char* processResult = "IDLE";
static Preferences irrigationPrefs;

static float temperatures[BC_TMEAN_BUCKETS];
static float temperatureSum = 0;
static uint16_t temperatureCount = 0;
static uint32_t temperatureBucket = UINT32_MAX;
static bool temperatureInit = false;
static uint32_t lastCycleEpoch = 0;
static char lastPrintedHold[sizeof(holdReason)] = "";

static void setPump(uint8_t pin, bool on) {
    digitalWrite(pin, (on ^ RELAY_INVERTED) ? HIGH : LOW);
}
static void allPumpsOff() {
    setPump(PUMP_FILL, false); setPump(PUMP_DOSE_A, false);
    setPump(PUMP_DOSE_B, false); setPump(PUMP_DIST, false);
}
static float flowRate(IrrigPhase p) {
    switch (p) {
        case IRRIG_FILL: return FILL_FLOW_ML_S;
        case IRRIG_DOSE_A: return DOSE_A_FLOW_ML_S;
        case IRRIG_DOSE_B: return DOSE_B_FLOW_ML_S;
        case IRRIG_DIST: return DIST_FLOW_ML_S;
        default: return 0;
    }
}
static float durationSeconds(float targetMl, float flowMlPerSec) {
    if (!isfinite(targetMl) || !isfinite(flowMlPerSec) || targetMl < 0 || flowMlPerSec <= 0) return NAN;
    return targetMl / flowMlPerSec;
}
static unsigned long durationMs(float targetMl, float flowMlPerSec) {
    float ms = durationSeconds(targetMl, flowMlPerSec) * 1000.0f;
    return isfinite(ms) && ms >= 0 ? (unsigned long)ceilf(ms) : 0;
}
static float finalMixVolumeMl() {
    return cycleWaterMl + cycleWaterMl / 1000.0f * (cycleDoseAMlPerL + cycleDoseBMlPerL);
}
static float phaseVolume(IrrigPhase p) {
    if (p == IRRIG_FILL) return cycleWaterMl;
    if (p == IRRIG_DIST) return finalMixVolumeMl();
    if (p == IRRIG_DOSE_A) return cycleWaterMl / 1000.0f * cycleDoseAMlPerL;
    if (p == IRRIG_DOSE_B) return cycleWaterMl / 1000.0f * cycleDoseBMlPerL;
    return 0;
}
static uint32_t currentDay() {
    time_t now = time(nullptr);
    return now >= 100000 ? (uint32_t)((now + 7 * 3600) / 86400) : 0;
}
static void loadBudget() {
    if (budgetLoaded) return;
    budgetLoaded = true;
    irrigationPrefs.begin("smartgh_irig", false);
    usedMl = irrigationPrefs.getFloat("used_ml", 0);
    usedDay = irrigationPrefs.getUInt("used_day", 0);
    lastCycleEpoch = irrigationPrefs.getUInt("last_cycle", 0);
    if (!isfinite(usedMl) || usedMl < 0) usedMl = 0;
}
static void persistBudget() {
    irrigationPrefs.putFloat("used_ml", usedMl); irrigationPrefs.putUInt("used_day", usedDay);
    irrigationPrefs.putUInt("last_cycle", lastCycleEpoch);
}
static void rollBudget() {
    loadBudget();
    uint32_t day = currentDay();
    if (day && day > usedDay) { usedDay = day; usedMl = 0; persistBudget(); }
}
static float pFactorSaatIni() {
    time_t now = time(nullptr);
    int month = 1;
    if (now >= 100000) { struct tm local; localtime_r(&now, &local); month = local.tm_mon + 1; }
    return SUN_P_MONTH[(month >= 1 && month <= 12) ? month - 1 : 0];
}
static bool fertilizerConfigValid() {
    float a = phaseDoseAMlPerL(), b = phaseDoseBMlPerL();
    return FERTILIZER_DOSE_ENABLED && isfinite(a) && isfinite(b) && a > 0 && b > 0 &&
        a <= 100.0f && b <= 100.0f &&
        FILL_FLOW_ML_S > 0 && DOSE_A_FLOW_ML_S > 0 &&
        DOSE_B_FLOW_ML_S > 0 && DIST_FLOW_ML_S > 0;
}
static bool cycleFitsLimits(float waterMl, float doseAMlPerL, float doseBMlPerL) {
    const float aMl = waterMl / 1000.0f * doseAMlPerL;
    const float bMl = waterMl / 1000.0f * doseBMlPerL;
    const float finalMl = waterMl + aMl + bMl;
    const float volumes[] = {waterMl, aMl, bMl, finalMl};
    const float rates[] = {FILL_FLOW_ML_S, DOSE_A_FLOW_ML_S, DOSE_B_FLOW_ML_S, DIST_FLOW_ML_S};
    if (!(MIX_TANK_CAPACITY_ML > 0) || !isfinite(finalMl) || finalMl > MIX_TANK_CAPACITY_ML ||
        waterMl > IRRIG_MAX_CYCLE_ML || aMl <= 0 || bMl <= 0 ||
        aMl > IRRIG_MAX_DOSE_ML || bMl > IRRIG_MAX_DOSE_ML ||
        aMl > STOCK_A_CAPACITY_ML || bMl > STOCK_B_CAPACITY_ML) return false;
    for (uint8_t i = 0; i < 4; ++i) {
        float durationMs = durationSeconds(volumes[i], rates[i]) * 1000.0f;
        if (!(rates[i] > 0) || !isfinite(durationMs) || durationMs <= 0 ||
            durationMs > IRRIG_PHASE_TIMEOUT_MS) return false;
    }
    return true;
}

float calculateEtc() {
    float tmean = irrigationTmean();
    if (!isfinite(tmean)) return 0;
    float eto = constrain(pFactorSaatIni() * (BC_T_COEF * tmean + BC_T_CONST), 0.0f, MAX_ET0);
    return eto * phaseKc();
}
void irrigationFeedTemp(float temp) {
    if (!isfinite(temp)) return;
    if (!temperatureInit) {
        for (uint16_t i = 0; i < BC_TMEAN_BUCKETS; ++i) temperatures[i] = NAN;
        temperatureBucket = millis() / BC_TMEAN_BUCKET_MS;
        temperatureInit = true;
    }
    uint32_t bucket = millis() / BC_TMEAN_BUCKET_MS;
    if (bucket != temperatureBucket) {
        uint32_t gap = bucket - temperatureBucket;
        uint32_t steps = min(gap, (uint32_t)BC_TMEAN_BUCKETS);
        for (uint32_t k = 1; k <= steps; ++k) {
            uint32_t idx = (temperatureBucket + k) % BC_TMEAN_BUCKETS;
            if (isfinite(temperatures[idx])) { temperatureSum -= temperatures[idx]; --temperatureCount; }
            temperatures[idx] = NAN;
        }
        temperatureBucket = bucket;
    }
    uint32_t idx = bucket % BC_TMEAN_BUCKETS;
    if (isfinite(temperatures[idx])) temperatureSum -= temperatures[idx];
    else ++temperatureCount;
    temperatures[idx] = temp; temperatureSum += temp;
}
float irrigationTmean() {
    if (temperatureCount < BC_TMEAN_BUCKETS) return NAN;
    return temperatureSum / temperatureCount;
}
float irrigationDailyBudgetMl() {
    float etc = calculateEtc();
    return etc > 0 && IRRIG_EFFICIENCY > 0 && IRRIG_EFFICIENCY <= 1.0f
        ? etc * IRRIG_AREA_CM2 * 0.1f / IRRIG_EFFICIENCY : 0;
}
float irrigationDoseMl() {
    return IRRIG_CYCLES_PER_DAY > 0 ? min(irrigationDailyBudgetMl() / IRRIG_CYCLES_PER_DAY, IRRIG_MAX_CYCLE_ML) : 0;
}
unsigned long irrigationDoseMs() { return durationMs(irrigationDoseMl(), DIST_FLOW_ML_S); }
unsigned long irrigationMinIntervalMs() { return IRRIG_CYCLES_PER_DAY > 0 ? 86400000UL / IRRIG_CYCLES_PER_DAY : 0; }
IrrigStatus irrigationStatus() {
    IrrigStatus s{}; s.budgetMl = irrigationDailyBudgetMl(); s.usedMl = usedMl;
    s.phaseName = irrigationPhaseName();
    s.result = processResult;
    s.phaseTargetMl = phase == IRRIG_IDLE || phase == IRRIG_SETTLE ? 0 : phaseVolume(phase);
    s.phaseDurationMs = phase == IRRIG_IDLE ? 0 : phaseDurationMs;
    s.phaseEstimatedMl = phase == IRRIG_SETTLE || phase == IRRIG_IDLE ? 0 :
        flowRate(phase) * ((float)phaseDurationMs / 1000.0f);
    s.doseAMl = cycleWaterMl > 0 ? cycleWaterMl / 1000.0f * cycleDoseAMlPerL :
        irrigationDoseMl() / 1000.0f * phaseDoseAMlPerL();
    s.doseBMl = cycleWaterMl > 0 ? cycleWaterMl / 1000.0f * cycleDoseBMlPerL :
        irrigationDoseMl() / 1000.0f * phaseDoseBMlPerL();
    float plannedWaterMl = cycleWaterMl > 0 ? cycleWaterMl : irrigationDoseMl();
    float plannedA = cycleWaterMl > 0 ? cycleDoseAMlPerL : phaseDoseAMlPerL();
    float plannedB = cycleWaterMl > 0 ? cycleDoseBMlPerL : phaseDoseBMlPerL();
    s.fillDurationMs = durationMs(plannedWaterMl, FILL_FLOW_ML_S);
    s.doseADurationMs = durationMs(plannedWaterMl / 1000.0f * plannedA, DOSE_A_FLOW_ML_S);
    s.doseBDurationMs = durationMs(plannedWaterMl / 1000.0f * plannedB, DOSE_B_FLOW_ML_S);
    s.distDurationMs = durationMs(plannedWaterMl + plannedWaterMl / 1000.0f * (plannedA + plannedB), DIST_FLOW_ML_S);
    s.minIntervalMs = irrigationMinIntervalMs();
    s.doseMl = phase == IRRIG_IDLE ? irrigationDoseMl() : cycleWaterMl;
    s.doseMs = phase == IRRIG_IDLE ? irrigationDoseMs() : durationMs(cycleWaterMl, DIST_FLOW_ML_S);
    s.holdReason = holdReason; return s;
}
const char* irrigationPhaseName() {
    switch (phase) {
        case IRRIG_FILL: return "FILL"; case IRRIG_DOSE_A: return "DOSE_A";
        case IRRIG_DOSE_B: return "DOSE_B"; case IRRIG_SETTLE: return "SETTLE";
        case IRRIG_DIST: return "DIST"; default: return "IDLE";
    }
}
bool irrigationRunning() { return phase != IRRIG_IDLE; }
static void startPhase(IrrigPhase next, uint32_t now) {
    allPumpsOff(); phase = next; phaseStart = now;
    if (next == IRRIG_SETTLE) phaseDurationMs = IRRIG_SETTLE_MS;
    else {
        float rate = flowRate(next), volume = phaseVolume(next);
        if (!(rate > 0) || !isfinite(volume) || volume < 0 || volume > IRRIG_MAX_DOSE_ML && (next == IRRIG_DOSE_A || next == IRRIG_DOSE_B)) {
            strncpy(holdReason, "konfigurasi volume/debit tidak aman", sizeof(holdReason)-1);
            phase = IRRIG_IDLE; allPumpsOff(); return;
        }
        phaseDurationMs = durationMs(volume, rate);
        if (!phaseDurationMs || phaseDurationMs > IRRIG_PHASE_TIMEOUT_MS) {
            strncpy(holdReason, "durasi pompa melewati batas", sizeof(holdReason)-1);
            phase = IRRIG_IDLE; allPumpsOff(); return;
        }
        setPump(next == IRRIG_FILL ? PUMP_FILL : next == IRRIG_DOSE_A ? PUMP_DOSE_A : next == IRRIG_DOSE_B ? PUMP_DOSE_B : PUMP_DIST, true);
    }
    Serial.printf("[irigasi] fase %s, target %.2f mL durasi %.2f s\n", irrigationPhaseName(), phaseVolume(next), phaseDurationMs / 1000.0f);
}
void irrigationAbort() {
    if (phase != IRRIG_IDLE) {
        processResult = "FAILED";
        strncpy(holdReason, "proses dibatalkan oleh sensor/override", sizeof(holdReason)-1);
        holdReason[sizeof(holdReason)-1] = '\0';
    }
    allPumpsOff(); phase = IRRIG_IDLE;
}
void irrigationUpdate(bool trigger, bool manual, uint32_t requestId) {
    uint32_t now = millis(); rollBudget();
    if (phase == IRRIG_IDLE && trigger) {
        bool manualNew = manual && requestId != lastManualRequestId;
        if (manual) lastManualRequestId = requestId;
        float water = irrigationDoseMl();
        uint32_t day = currentDay();
        time_t epoch = time(nullptr);
        bool clockOk = epoch >= 100000;
        uint32_t since = clockOk && lastCycleEpoch ? (uint32_t)epoch - lastCycleEpoch : UINT32_MAX;
        bool intervalOk = !lastCycleEpoch || (clockOk && since >= irrigationMinIntervalMs() / 1000UL);
        bool budgetOk = usedMl + water <= irrigationDailyBudgetMl() + 0.5f;
        if (!IRRIG_AUTO_ENABLED && !manual) strncpy(holdReason, "irigasi otomatis dinonaktifkan", sizeof(holdReason)-1);
        else if (!fertilizerConfigValid()) strncpy(holdReason, "dosis AB Mix belum divalidasi/diaktifkan", sizeof(holdReason)-1);
        else if (!clockOk) strncpy(holdReason, "menunggu sinkronisasi waktu", sizeof(holdReason)-1);
        else if (water <= 0 || water > IRRIG_MAX_CYCLE_ML || irrigationTmean() != irrigationTmean()) strncpy(holdReason, "ETc/Tmean 24 jam belum tersedia", sizeof(holdReason)-1);
        else if (!manualNew && (!budgetOk || !intervalOk)) strncpy(holdReason, !budgetOk ? "anggaran harian habis" : "jeda minimum belum lewat", sizeof(holdReason)-1);
        else if (!intervalOk) strncpy(holdReason, "jeda minimum belum lewat sejak siklus terakhir", sizeof(holdReason)-1);
        else if (!cycleFitsLimits(water, phaseDoseAMlPerL(), phaseDoseBMlPerL())) strncpy(holdReason, "volume/durasi melebihi batas atau kapasitas tangki belum diatur", sizeof(holdReason)-1);
        else {
            holdReason[0] = 0; cycleWaterMl = water;
            cycleDoseAMlPerL = phaseDoseAMlPerL(); cycleDoseBMlPerL = phaseDoseBMlPerL();
            lastCycleEpoch = (uint32_t)epoch;
            usedMl += water; if (day) usedDay = day; persistBudget();
            processResult = "RUNNING";
            startPhase(IRRIG_FILL, now);
        }
    }
    if (phase == IRRIG_IDLE && holdReason[0]) processResult = "HELD";
    if (phase == IRRIG_IDLE && holdReason[0] && strcmp(holdReason, lastPrintedHold) != 0) {
        strncpy(lastPrintedHold, holdReason, sizeof(lastPrintedHold) - 1);
        lastPrintedHold[sizeof(lastPrintedHold) - 1] = '\0';
        Serial.printf("[irigasi] siklus ditahan: %s\n", holdReason);
    } else if (!holdReason[0]) lastPrintedHold[0] = '\0';
    if (phase == IRRIG_IDLE) return;
    if (now - phaseStart >= IRRIG_PHASE_TIMEOUT_MS && phase != IRRIG_SETTLE) { irrigationAbort(); strncpy(holdReason, "timeout pompa; siklus dibatalkan", sizeof(holdReason)-1); return; }
    if (now - phaseStart < phaseDurationMs) return;
    switch (phase) {
        case IRRIG_FILL: startPhase(IRRIG_DOSE_A, now); break;
        case IRRIG_DOSE_A: startPhase(IRRIG_DOSE_B, now); break;
        case IRRIG_DOSE_B: startPhase(IRRIG_SETTLE, now); break;
        case IRRIG_SETTLE: startPhase(IRRIG_DIST, now); break;
        case IRRIG_DIST: allPumpsOff(); phase = IRRIG_IDLE; processResult = "SUCCESS"; Serial.println("[irigasi] siklus selesai"); break;
        default: irrigationAbort(); break;
    }
}

// ===== lcd_display.cpp =====

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

static LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// Teks baris status yang digeser. Ditulis controlTask, dibaca task scroll.
static char scrollText[96] = "";
static SemaphoreHandle_t textMux = nullptr;
static volatile bool scrollReset = true;   // teks berubah -> mulai dari awal

static void lcdLine(uint8_t row, const char* text) {
    char buf[LCD_COLS + 1];
    snprintf(buf, sizeof(buf), "%-*.*s", LCD_COLS, LCD_COLS, text);
    if (!i2cLock(pdMS_TO_TICKS(50))) return;
    lcd.setCursor(0, row);
    lcd.print(buf);
    i2cUnlock();
}

static void fmtVal(char* buf, size_t n, float v, int dec, const char* suffix) {
    if (isnan(v)) snprintf(buf, n, "--%s", suffix);
    else snprintf(buf, n, "%.*f%s", dec, v, suffix);
}

void initLcd() {
    if (!textMux) textMux = xSemaphoreCreateMutex();
    Wire.begin(I2C_SDA, I2C_SCL);
    lcd.begin(LCD_COLS, LCD_ROWS);
    lcd.backlight();
    lcdLine(0, "Smart Watering");
    lcdLine(1, "Starting...");
}

// Baris 0 untuk VPD/ETc; baris status ditulis task scroll (lcdScrollTask).
void updateLcd(float temp, float humidity, float soilMoisture, float vpd,
               float etc) {
    char line0[24];
    char vStr[12];
    char eStr[12];
    fmtVal(vStr, sizeof(vStr), vpd, 2, "");
    fmtVal(eStr, sizeof(eStr), etc, 1, "mm");
    snprintf(line0, sizeof(line0), "VPD%s ETc%s", vStr, eStr);
    lcdLine(0, line0);
}

void lcdScrollSetStatus(const char* status) {
    if (!status || !textMux) return;
    xSemaphoreTake(textMux, portMAX_DELAY);
    if (strcmp(scrollText, status) != 0) {
        snprintf(scrollText, sizeof(scrollText), "%s", status);
        scrollReset = true;
    }
    xSemaphoreGive(textMux);
}

static void lcdScrollTask(void* arg) {
    char text[96];
    char win[LCD_COLS + 1];
    char shown[LCD_COLS + 1] = "";   // isi baris status yang sudah ditulis
    int  off = 0;
    bool hold = true;
    int  holdTicks = 0;
    TickType_t lastWake = xTaskGetTickCount();

    for (;;) {
        xSemaphoreTake(textMux, portMAX_DELAY);
        snprintf(text, sizeof(text), "%s", scrollText);
        bool reset = scrollReset;
        scrollReset = false;
        xSemaphoreGive(textMux);

        int len = (int)strlen(text);
        if (reset) {
            off = 0;
            hold = true;
            holdTicks = 0;
        }
        if (off > 0 && off > len - LCD_COLS) off = 0;   // teks memendek

        if (len <= LCD_COLS) {
            snprintf(win, sizeof(win), "%-*.*s", LCD_COLS, LCD_COLS, text);
            off = 0;
            hold = true;
            holdTicks = 0;
        } else {
            snprintf(win, sizeof(win), "%-*.*s", LCD_COLS, off + LCD_COLS, text + off);
            if (hold) {
                if (++holdTicks * LCD_SCROLL_MS >= LCD_SCROLL_HOLD_MS) {
                    hold = false;
                    holdTicks = 0;
                }
            } else if (off >= len - LCD_COLS) {
                if (++holdTicks * LCD_SCROLL_MS >= LCD_SCROLL_HOLD_MS) {
                    off = 0;
                    hold = true;
                    holdTicks = 0;
                }
            } else {
                off++;
            }
        }

        if (strcmp(win, shown) != 0 && i2cLock(pdMS_TO_TICKS(50))) {
            lcd.setCursor(0, 1);
            lcd.print(win);
            i2cUnlock();
            snprintf(shown, sizeof(shown), "%s", win);
        }
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(LCD_SCROLL_MS));
    }
}

void lcdScrollStart() {
    if (!textMux) textMux = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(lcdScrollTask, "lcd", LCD_SCROLL_STACK, nullptr,
                            LCD_SCROLL_PRIO, nullptr, 0);
}

// ===== phase.cpp =====

#include <Preferences.h>
#include <esp_timer.h>
#include <freertos/semphr.h>
#include <string.h>
#include <time.h>

static Preferences phasePrefs;
static SemaphoreHandle_t phaseMux = nullptr;
static bool     s_active = false;
static float    s_vpdTarget = PHASE_DEFAULT_VPD;
static char     s_name[PHASE_NAME_LEN] = PHASE_DEFAULT_NAME;
static uint32_t s_durationSec = PHASE_DEFAULT_DURATION;
static uint32_t s_startEpoch = 0;      // epoch saat fase di-set/reset (0 = belum sync)
static int64_t  s_startUs = 0;

static void phaseLock()   { if (phaseMux) xSemaphoreTake(phaseMux, portMAX_DELAY); }
static void phaseUnlock() { if (phaseMux) xSemaphoreGive(phaseMux); }

static uint32_t nowEpoch() {
    time_t now = time(nullptr);
    return (now >= 100000) ? (uint32_t)now : 0;
}

static uint32_t elapsedSec(bool* known) {
    if (s_startEpoch >= 100000) {
        uint32_t now = nowEpoch();
        if (now >= 100000) {
            *known = true;
            // clamp ke UINT32_MAX (now selalu >= epoch secara wajar)
            return (now >= s_startEpoch) ? (now - s_startEpoch) : 0;
        }
        *known = false;
        return 0;
    }
    *known = true;
    if (s_startUs == 0) return 0;
    int64_t d = esp_timer_get_time() - s_startUs;
    if (d < 0) return 0;
    uint64_t sec = (uint64_t)d / 1000000ULL;
    return (sec > 0xFFFFFFFFULL) ? 0xFFFFFFFFUL : (uint32_t)sec;
}

static float phaseKcUnlocked() {
    const char* n = s_name;
    if (strcasestr(n, "semai") || strcasestr(n, "awal")) return KC_AWAL;
    if (strcasestr(n, "vegetatif"))return KC_VEGETATIF;
    if (strcasestr(n, "generatif")) return KC_PEMBUNGAAN;
    if (strcasestr(n, "pembungaan") || strcasestr(n, "bunga")) return KC_PEMBUNGAAN;
    if (strcasestr(n, "pemuliaan") || strcasestr(n, "buah"))  return KC_PEMULIAAN_BUAH;
    if (strcasestr(n, "panen"))    return KC_PANEN;
    return KC_DEFAULT;
}

void phaseInit() {
    if (!phaseMux) phaseMux = xSemaphoreCreateMutex();
    phasePrefs.begin(NVS_NAMESPACE, false);
    s_active      = phasePrefs.getBool("ph_active", false);
    s_vpdTarget   = phasePrefs.getFloat("ph_vpd", PHASE_DEFAULT_VPD);
    s_durationSec = phasePrefs.getULong("ph_dur", PHASE_DEFAULT_DURATION);
    String n      = phasePrefs.getString("ph_name", PHASE_DEFAULT_NAME);
    strncpy(s_name, n.c_str(), PHASE_NAME_LEN - 1);
    s_name[PHASE_NAME_LEN - 1] = '\0';
    // ph_epoch tidak ada di device lama -> 0, elapsed fallback ke uptime.
    s_startEpoch = phasePrefs.getULong("ph_epoch", 0);
    s_startUs = esp_timer_get_time();
}

bool phaseSetVpd(float vpd) {
    if (isnan(vpd) || vpd < VPD_TARGET_MIN || vpd > VPD_TARGET_MAX) return false;
    phaseLock();
    s_vpdTarget = vpd;
    s_active = true;
    phaseUnlock();
    phasePrefs.putFloat("ph_vpd", vpd);
    phasePrefs.putBool("ph_active", true);
    return true;
}

void phaseDeactivate() {
    // Mode setpoint/fase dimatikan -> keputusan sepenuhnya kembali ke fuzzy.
    phaseLock();
    s_active = false;
    phaseUnlock();
    phasePrefs.putBool("ph_active", false);
}

void phaseSetName(const char* name) {
    if (!name) return;
    char stored[PHASE_NAME_LEN];
    phaseLock();
    strncpy(s_name, name, PHASE_NAME_LEN - 1);
    s_name[PHASE_NAME_LEN - 1] = '\0';
    strncpy(stored, s_name, PHASE_NAME_LEN - 1);
    stored[PHASE_NAME_LEN - 1] = '\0';
    phaseUnlock();
    phasePrefs.putString("ph_name", stored);
    // Log Kc ikut berubah supaya bisa dibuktikan dari log port.
    Serial.printf("[fase] %s | Kc %.2f | dosis A/B %.2f/%.2f mL/L\n",
                  stored, phaseKc(), phaseDoseAMlPerL(), phaseDoseBMlPerL());
}

void phaseSetDuration(uint32_t seconds) {
    if (seconds > PHASE_MAX_DURATION_SEC) return;   // diamankan di sini juga
    uint32_t startEpoch = nowEpoch();
    phaseLock();
    s_durationSec = seconds;
    s_startUs = esp_timer_get_time();   // mulai hitung dari sekarang
    s_startEpoch = startEpoch;
    phaseUnlock();
    phasePrefs.putULong("ph_dur", seconds);
    if (startEpoch != 0) phasePrefs.putULong("ph_epoch", startEpoch);
}

void phaseReset() {
    uint32_t startEpoch = nowEpoch();
    phaseLock();
    s_startUs = esp_timer_get_time();
    s_startEpoch = startEpoch;
    phaseUnlock();
    if (startEpoch != 0) phasePrefs.putULong("ph_epoch", startEpoch);
}

bool phaseActive() {
    phaseLock();
    bool active = s_active;
    phaseUnlock();
    return active;
}

float phaseVpdTarget() {
    phaseLock();
    float target = s_vpdTarget;
    phaseUnlock();
    return target;
}

float phaseKc() {
    phaseLock();
    float kc = phaseKcUnlocked();
    phaseUnlock();
    return kc;
}

static float phaseDoseUnlocked(bool nutrientB) {
    const char* n = s_name;
    if (strcasestr(n, "semai") || strcasestr(n, "awal"))
        return nutrientB ? DOSE_SEMAI_B_ML_PER_L : DOSE_SEMAI_A_ML_PER_L;
    if (strcasestr(n, "vegetatif"))
        return nutrientB ? DOSE_VEGETATIF_B_ML_PER_L : DOSE_VEGETATIF_A_ML_PER_L;
    if (strcasestr(n, "generatif") || strcasestr(n, "pembungaan") ||
        strcasestr(n, "bunga") || strcasestr(n, "buah") || strcasestr(n, "panen") ||
        strcasestr(n, "pemasakan"))
        return nutrientB ? DOSE_GENERATIF_B_ML_PER_L : DOSE_GENERATIF_A_ML_PER_L;
    return NAN; // unknown phase must never pick an assumed fertilizer dose
}

float phaseDoseAMlPerL() { phaseLock(); float dose = phaseDoseUnlocked(false); phaseUnlock(); return dose; }
float phaseDoseBMlPerL() { phaseLock(); float dose = phaseDoseUnlocked(true); phaseUnlock(); return dose; }

PhaseState phaseGet() {
    phaseLock();
    bool perluStempel = (s_startEpoch == 0);
    phaseUnlock();
    if (perluStempel) {
        uint32_t now = nowEpoch();
        if (now >= 100000) {
            phaseLock();
            if (s_startEpoch == 0) s_startEpoch = now;
            phaseUnlock();
            phasePrefs.putULong("ph_epoch", now);   // NVS di luar phaseLock
            Serial.printf("[fase] ph_epoch 0 diperbaiki -> %lu (penghitung tidak reset tiap boot)\n",
                          (unsigned long)now);
        }
    }

    PhaseState st;
    phaseLock();
    st.active = s_active;
    st.vpdTarget = s_vpdTarget;
    strncpy(st.name, s_name, PHASE_NAME_LEN - 1);
    st.name[PHASE_NAME_LEN - 1] = '\0';
    st.kc = phaseKcUnlocked();
    st.durationSec = s_durationSec;
    bool known = true;
    uint32_t el = elapsedSec(&known);
    if (s_durationSec > 0 && el > s_durationSec) el = s_durationSec;
    st.elapsedSec = el;
    st.remainingSec = (s_durationSec > el) ? (s_durationSec - el) : 0;
    st.clockOk = known;
    phaseUnlock();
    return st;
}

// ===== sd_logger.cpp =====

#include <SD.h>
#include <freertos/semphr.h>

static const char* LOG_FILE = "/log.csv";
static const char* BACKLOG_FILE = "/backlog.txt";
static bool sdReady = false;

static File logFile;
static int linesSinceFlush = 0;
static unsigned long lastBacklogFlush = 0;

// SD diakses dua task (kontrol: log CSV; jaringan: backlog) — satu mutex.
static SemaphoreHandle_t sdMux = nullptr;

static void sdLock()   { if (sdMux) xSemaphoreTake(sdMux, portMAX_DELAY); }
static void sdUnlock() { if (sdMux) xSemaphoreGive(sdMux); }

void initSdLogger() {
    sdMux = xSemaphoreCreateMutex();
    if (SD.begin(SD_CS)) {
        sdReady = true;
        logFile = SD.open(LOG_FILE, FILE_APPEND);
        Serial.println("[sd] MicroSD siap (backup lokal aktif)");
    } else {
        Serial.println("[sd] MicroSD tidak terdeteksi - lanjut tanpa backup");
    }
}

void sdLogRecord(const char* line) {
    if (!sdReady || !logFile) return;
    sdLock();
    logFile.println(line);
    if (++linesSinceFlush >= SD_LOG_FLUSH_EVERY) {
        logFile.flush();
        linesSinceFlush = 0;
    }
    sdUnlock();
}

// Hitung jumlah baris (dipisah '\n') dalam string backlog.
static int countLines(const String& s) {
    int lines = 0;
    for (unsigned int i = 0; i < s.length(); i++) if (s[i] == '\n') lines++;
    if (s.length() > 0 && !s.endsWith("\n")) lines++;   // baris terakhir tanpa newline
    return lines;
}

// Buang n baris paling depan (data paling lama) dari string backlog.
static void dropOldestLines(String& s, int n) {
    for (int i = 0; i < n; i++) {
        int idx = s.indexOf('\n');
        if (idx < 0) { s = ""; return; }
        s.remove(0, idx + 1);
    }
}

void sdQueueBacklog(const char* body) {
    if (!sdReady) return;
    sdLock();
    // Hitung baris antrean (baca saja, tidak menulis kartu).
    String all;
    File f = SD.open(BACKLOG_FILE, FILE_READ);
    if (f) {
        all = f.readString();
        f.close();
    }
    int lines = countLines(all);

    if (lines < SD_BACKLOG_MAX) {
        // Normal: append (murah, tidak menulis ulang file).
        File w = SD.open(BACKLOG_FILE, FILE_APPEND);
        if (w) {
            w.println(body);
            w.close();
            Serial.printf("[sd] push gagal, antrekan backlog (%d)\n", lines + 1);
        }
        sdUnlock();
        return;
    }

    int drop = lines - (SD_BACKLOG_MAX - 1);   // sisakan MAX-1 supaya baris baru muat
    dropOldestLines(all, drop);
    Serial.printf("[sd] backlog penuh, %d baris terlama dibuang\n", drop);

    File w = SD.open(BACKLOG_FILE, FILE_WRITE);   // truncate
    if (w) {
        w.print(all);
        if (all.length() > 0 && !all.endsWith("\n")) w.println();
        w.println(body);
        w.close();
        Serial.printf("[sd] push gagal, antrekan backlog (%d)\n", SD_BACKLOG_MAX);
    }
    sdUnlock();
}

void sdFlushBacklog() {
    if (!sdReady) return;
    // Tulis ulang seluruh file setiap flush — jangan terlalu sering.
    if (millis() - lastBacklogFlush < SD_FLUSH_MS) return;
    lastBacklogFlush = millis();

    String all;
    sdLock();
    File f = SD.open(BACKLOG_FILE, FILE_READ);
    if (f) {
        all = f.readString();
        f.close();
    }
    sdUnlock();

    if (all.length() == 0) return;

    // Kirim line per line (limit per panggilan); yang gagal/tak sempat tetap disimpan.
    String rest;
    int start = 0, idx, pushed = 0;
    while ((idx = all.indexOf('\n', start)) >= 0) {
        String line = all.substring(start, idx);
        start = idx + 1;
        line.trim();
        if (line.length() == 0) continue;
        if (pushed < SD_BACKLOG_FLUSH_PER_CALL && httpPostSensorLogs(line)) {
            pushed++;
        } else {
            rest += line + "\n";
        }
    }
    // Baris terakhir tanpa newline (jarang, tapi jangan hilang diam-diam).
    if (start < (int)all.length()) {
        String line = all.substring(start);
        line.trim();
        if (line.length() > 0) {
            if (pushed < SD_BACKLOG_FLUSH_PER_CALL && httpPostSensorLogs(line)) {
                pushed++;
            } else {
                rest += line + "\n";
            }
        }
    }

    if (pushed > 0) {
        sdLock();
        File w = SD.open(BACKLOG_FILE, FILE_WRITE);   // truncate
        if (w) {
            w.print(rest);
            w.close();
        }
        sdUnlock();
    }
    if (pushed > 0) Serial.printf("[sd] backlog terkirim: %d\n", pushed);
}

// ===== sensor.cpp =====

#include <DHT.h>

static DHT dht(DHT_PIN, DHT22);
static bool dhtReadOk = false;
static SemaphoreHandle_t i2cMux = nullptr;

void i2cBusInit() {
    if (!i2cMux) i2cMux = xSemaphoreCreateMutex();
}

bool i2cLock(TickType_t timeout) {
    if (!i2cMux) return true;
    return xSemaphoreTake(i2cMux, timeout) == pdTRUE;
}

void i2cUnlock() {
    if (i2cMux) xSemaphoreGive(i2cMux);
}

static const uint8_t soilPins[SOIL_COUNT] = SOIL_PINS;
static const int     soilDry[SOIL_COUNT]  = SOIL_DRY_CAL;
static const int     soilWet[SOIL_COUNT]  = SOIL_WET_CAL;

void initSensor() {
    dht.begin();
    Serial.printf("[sensor] DHT22 mulai pada GPIO %u\n", DHT_PIN);
    for (uint8_t ch = 0; ch < SOIL_COUNT; ch++) {
        pinMode(soilPins[ch], INPUT);
    }
}

static float readSoilRaw(uint8_t ch) {
    analogRead(soilPins[ch]);
    float sum = 0.0f;
    for (int s = 0; s < SOIL_SAMPLES; s++) sum += (float)analogRead(soilPins[ch]);
    return sum / (float)SOIL_SAMPLES;
}

static bool soilPercent(uint8_t ch, float raw, float* out) {
    float lo = (float)soilWet[ch] - (float)SOIL_RAW_TOL;
    float hi = (float)soilDry[ch] + (float)SOIL_RAW_TOL;
    if (isnan(raw) || raw < lo || raw > hi) return false;
    float clamped = constrain(raw, (float)soilWet[ch], (float)soilDry[ch]);
    *out = (float)map((long)clamped, soilDry[ch], soilWet[ch], 0, 100);
    return true;
}

SensorData readSensors() {
    SensorData data;
    data.temperature = dht.readTemperature();
    data.humidity = dht.readHumidity();
    dhtReadOk = isfinite(data.temperature) && isfinite(data.humidity) &&
                data.temperature > -20.0f && data.temperature < 60.0f &&
                data.humidity >= 0.0f && data.humidity <= 100.0f;

    float   sum = 0.0f;
    uint8_t n = 0;
    uint8_t okMask = 0;
    for (uint8_t ch = 0; ch < SOIL_COUNT; ch++) {
        float raw = readSoilRaw(ch);
        float pct = NAN;
        data.soilRaw[ch] = raw;
        if (soilPercent(ch, raw, &pct)) {
            sum += pct;
            okMask |= (uint8_t)(1u << ch);
            n++;
        }
        data.soilPct[ch] = pct;
        if (ch + 1 < SOIL_COUNT) delay(SOIL_SETTLE_MS);
    }
    data.soilOk = okMask;
    data.soilCount = n;
    data.soilValid = (n > 0);
    data.soilMoisture = (n > 0) ? (sum / (float)n) : 0.0f;
    data.valid = dhtReadOk;
    return data;
}

const char* getSensorError() {
    return dhtReadOk ? "OK" : "DHT22 tidak terbaca / data di luar rentang";
}

// ===== RumahHijau.ino =====
#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include <freertos/semphr.h>

static const int CONTROL_TASK_STACK = 4096;
static const int NET_TASK_STACK = 10240;   // TLS mbedTLS butuh stack besar
static const int CONTROL_TASK_PRIO = 3;
static const int NET_TASK_PRIO = 2;
static const int CONTROL_TASK_CORE = 1;    // core 0 dipakai WiFi/lwIP
static const int NET_TASK_CORE = 0;

struct Snapshot {
    float temperature, humidity, soilMoisture, soil2, soil3, vpd, etc, tmean;
    bool  manual, mist, blower, irigasiRunning;
    float blowerLevel;  // kondisi relay blower aktual: 0 = mati, 1 = nyala
    char  status[96];   // label aksi (buildStatus) + penanda SOILERR/NOSYNC/...
    uint8_t soilOk;     // bitmask probe tanah valid (0 = semua mati)
    uint8_t soilCount;  // jumlah probe tanah valid
    PhaseState phase;   // setelan fase + VPD target (untuk telemetri DB)
    OverrideState ov;   // status override manual per aktuator (sakelar dashboard)
    IrrigStatus irig;   // dosis/durasi/anggaran irigasi (telemetri DB)
    bool fuzzyMist, fuzzyBlower, fuzzyIrigasi;
};

static Snapshot gSnap;
static SemaphoreHandle_t snapMux = nullptr;

// Filter EMA sinyal sensor (lihat filter.h). Alfa dari config.h.
static EmaFilter tempFilter, humFilter;
static EmaFilter soilFilter[SOIL_COUNT];   // satu EMA per probe tanah

// VPD (Tetens): kPa = SVP - AVP.
static float calcVpd(float temp, float humidity) {
    float svp = 0.6108 * exp((17.27 * temp) / (temp + 237.3));
    return svp - svp * (humidity / 100.0);
}

static void snapshotPublish(const Snapshot& s) {
    xSemaphoreTake(snapMux, portMAX_DELAY);
    gSnap = s;
    xSemaphoreGive(snapMux);
}

static Snapshot snapshotRead() {
    Snapshot s;
    xSemaphoreTake(snapMux, portMAX_DELAY);
    s = gSnap;
    xSemaphoreGive(snapMux);
    return s;
}

// Tambahkan sufiks status (mis. " SOILERR") tanpa overflow.
static void statusAdd(char* buf, size_t n, const char* suffix) {
    size_t len = strlen(buf);
    size_t add = strlen(suffix);
    if (len + add + 1 < n) strcat(buf, suffix);
}

static void buildStatus(char* buf, size_t n, const char* cond,
                        bool irig, bool mist, bool blower) {
    const char* act[3];
    int k = 0;
    if (irig)   act[k++] = "Irigasi";
    if (mist)   act[k++] = "Misting";
    if (blower) act[k++] = "Blower";
    if (k == 0)      snprintf(buf, n, "%s", cond);
    else if (k == 1) snprintf(buf, n, "%s ON", act[0]);
    else if (k == 2) snprintf(buf, n, "%s + %s ON", act[0], act[1]);
    else             snprintf(buf, n, "%s + %s + %s ON", act[0], act[1], act[2]);
}

// Alasan reset perangkat supaya reboot tak terduga kelihatan di log boot.
static const char* resetReasonName(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:   return "power-on";
        case ESP_RST_SW:        return "software (restart perintah / restart manual)";
        case ESP_RST_PANIC:     return "panic (exception)";
        case ESP_RST_INT_WDT:   return "interrupt WDT";
        case ESP_RST_TASK_WDT:  return "TASK WDT (panic-reboot Task Watchdog)";
        case ESP_RST_WDT:       return "WDT lain";
        case ESP_RST_BROWNOUT:  return "brownout (jatuh tegangan PSU)";
        case ESP_RST_DEEPSLEEP: return "deep sleep";
        case ESP_RST_EXT:       return "ext pin";
        default:                return "tidak diketahui";
    }
}

// ==== WiFi ====
static unsigned long lastReconnect = 0;

void connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false);
    WiFi.setTxPower(WIFI_POWER_11dBm);
    WiFi.setSleep(false);

    Serial.printf("[wifi] connecting to %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT) {
        vTaskDelay(pdMS_TO_TICKS(100));
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[wifi] connected!");
        Serial.print("[wifi] SSID: ");
        Serial.println(WiFi.SSID());
        Serial.print("[wifi] IP: ");
        Serial.println(WiFi.localIP());
        Serial.print("[wifi] Gateway: ");
        Serial.println(WiFi.gatewayIP());
    } else {
        Serial.println("[wifi] gagal; berjalan offline");
    }
    lastReconnect = millis();
}

void syncTime() {
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
}

// ==== Task kontrol (core 1, prio 3) ====
static void controlTask(void* arg) {
    SensorData sensorData;
    float vpdValue = 0;
    float etcValue = 0;
    const char* systemStatus = "Starting";
    FuzzyOutput fuzzyOut;
    unsigned long lastLcd = 0;
    bool alertActive = false;
    unsigned long lastFailPrint = 0;
    unsigned long lastGatePrint = 0;
    uint8_t lastSoilOk = 0xFF;

    for (;;) {
        sensorData = readSensors();          // T/RH + tiga probe tanah (SOIL_PINS)
        if (sensorData.valid) {
            sensorData.temperature = emaUpdate(tempFilter, sensorData.temperature);
            sensorData.humidity    = emaUpdate(humFilter, sensorData.humidity);
        }
        float soilSum = 0.0f;
        int   soilN = 0;
        for (int i = 0; i < SOIL_COUNT; i++) {
            if (isnan(sensorData.soilPct[i])) continue;
            sensorData.soilPct[i] = emaUpdate(soilFilter[i], sensorData.soilPct[i]);
            soilSum += sensorData.soilPct[i];
            soilN++;
        }
        if (soilN > 0) {
            sensorData.soilMoisture = soilSum / (float)soilN;
        } else {
            float lastSum = 0.0f;
            int   lastN = 0;
            for (int i = 0; i < SOIL_COUNT; i++) {
                if (soilFilter[i].seeded) { lastSum += soilFilter[i].value; lastN++; }
            }
            sensorData.soilMoisture = (lastN > 0) ? (lastSum / (float)lastN) : 50.0f;
        }

        if (sensorData.soilOk != lastSoilOk) {
            lastSoilOk = sensorData.soilOk;
            char okList[24] = "";
            char badList[80] = "";
            for (uint8_t ch = 0; ch < SOIL_COUNT; ch++) {
                char item[24];
                if (sensorData.soilOk & (uint8_t)(1u << ch)) {
                    snprintf(item, sizeof(item), "%s%u", okList[0] ? "," : "", ch + 1);
                    strncat(okList, item, sizeof(okList) - strlen(okList) - 1);
                } else {
                    snprintf(item, sizeof(item), "%s%u (raw %.0f)",
                             badList[0] ? ", " : "", ch + 1, sensorData.soilRaw[ch]);
                    strncat(badList, item, sizeof(badList) - strlen(badList) - 1);
                }
            }
            if (!sensorData.soilOk) {
                Serial.printf("[soil] semua probe tidak terbaca: %s\n", badList);
            } else if (!badList[0]) {
                Serial.printf("[soil] semua probe OK (%s)\n", okList);
            } else {
                Serial.printf("[soil] probe %s OK · probe %s tidak terbaca\n",
                              okList, badList);
            }
        }

        if (sensorData.valid) {
            irrigationFeedTemp(sensorData.temperature);
            vpdValue = calcVpd(sensorData.temperature, sensorData.humidity);
            etcValue = calculateEtc();
            fuzzyOut = fuzzyDecision(sensorData.temperature, sensorData.humidity,
                                     sensorData.soilMoisture, vpdValue, etcValue);
            systemStatus = fuzzyOut.status;

            // Peringatan suhu ekstrem.
            bool nowAlert = sensorData.temperature > 38.0 || sensorData.temperature < 10.0;
            if (nowAlert && !alertActive) {
                Serial.printf("[!] ALERT: suhu ekstrem %.1f C\n", sensorData.temperature);
            }
            alertActive = nowAlert;
        } else {
            systemStatus = "Sensor Error";
        }

        if ((!sensorData.valid || !sensorData.soilValid) &&
            millis() - lastFailPrint > 5000) {
            lastFailPrint = millis();
            if (!sensorData.valid) {
                Serial.println("[!] Sensor DHT22 tidak terbaca - cek wiring/pull-up");
                Serial.printf("[!] Sensor error: %s\n", getSensorError());
            }
            if (!sensorData.soilValid) {
                Serial.printf("[!] Probe tanah di luar rentang (raw %.0f/%.0f/%.0f) - irigasi fuzzy DITAHAN (override manual tetap berlaku)\n",
                              sensorData.soilRaw[0], sensorData.soilRaw[1], sensorData.soilRaw[2]);
            }
        }

        if (!IRRIG_AUTO_ENABLED && sensorData.valid && sensorData.soilValid &&
            fuzzyOut.irigasiOn && millis() - lastGatePrint > 30000) {
            lastGatePrint = millis();
            Serial.println("[irigasi] irigasi otomatis DITAHAN: debit pompa & luas media belum dikalibrasi (IRRIG_AUTO_ENABLED=0) - manual tetap bisa");
        }

        bool mist = sensorData.valid && fuzzyOut.mistOn;
        bool irigasi = IRRIG_AUTO_ENABLED && sensorData.valid && sensorData.soilValid
                       && fuzzyOut.irigasiOn;
        bool blower = sensorData.valid && fuzzyOut.blowerOn;
        float blowerLevel = sensorData.valid ? fuzzyOut.blowerLevel : 0.0f;

        if (sensorData.valid && phaseActive()) {
            float target = phaseVpdTarget();
            if (vpdValue > target + VPD_BAND_KPA) {
                mist = true;
            } else if (vpdValue < target - VPD_BAND_KPA) {
                float deficit = (target - VPD_BAND_KPA) - vpdValue;   // kPa di bawah band
                float boost = constrain(deficit / VPD_BAND_KPA, 0.0f, 1.0f) * VPD_BOOST_MAX;
                blower = true;
                if (boost > blowerLevel) blowerLevel = boost;
            }
        }

        bool fuzzyMist = mist, fuzzyBlower = blower, fuzzyIrigasi = irigasi;

        // Override manual tetap menang.
        if (sensorData.valid) controlApply(&mist, &irigasi, &blower, &blowerLevel);
        blowerLevel = blower ? 1.0f : 0.0f; // nilai aktual relay ON/OFF
        bool manual = controlIsManual();
        OverrideState ov = controlOverrides();
        setActuators(mist, blower, blowerLevel);

        if (!sensorData.valid) {
            irrigationAbort();
        } else if (ov.irig == 0) {
            irrigationAbort();
        } else {
            irrigationUpdate(irigasi, ov.irig == 1, controlIrrigationRequestId());
        }
        bool irigasiRunning = irrigationRunning();
        IrrigStatus irig = irrigationStatus();

        float rTemp = sensorData.valid ? sensorData.temperature : NAN;
        float rHum  = sensorData.valid ? sensorData.humidity : NAN;
        float rSoil = sensorData.soilValid ? sensorData.soilMoisture : NAN;
        float rSoil2 = sensorData.soilPct[1];
        float rSoil3 = sensorData.soilPct[2];
        float rVpd  = sensorData.valid ? vpdValue : NAN;
        float rEtc  = sensorData.valid ? etcValue : NAN;
        float rTmean = irrigationTmean();

        char statusTxt[96];
        buildStatus(statusTxt, sizeof(statusTxt), systemStatus,
                    irigasiRunning, mist, blower);

        Serial.printf("T:%.1fC H:%.1f%% Soil:%.1f%% VPD:%.2fkPa ETc:%.1fmm Tmean:%.1f [%s]",
                      rTemp, rHum, rSoil, rVpd, rEtc, rTmean, statusTxt);
        Serial.printf(" Mist:%s Irig:%s Blower:%s(lv%.0f%%) Fase:%s Kc:%.2f VPDtgt:%.2f",
                      mist ? "ON" : "OFF", irigasiRunning ? "RUN" : "OFF",
                      blower ? "ON" : "OFF", blowerLevel * 100.0,
                      phaseGet().name, phaseKc(), phaseVpdTarget());
        Serial.printf(" OV[m%c i%c b%c] SoilRaw:%.0f/%.0f/%.0f\n",
                      ov.mist < 0 ? '-' : (ov.mist ? '+' : '0'),
                      ov.irig < 0 ? '-' : (ov.irig ? '+' : '0'),
                      ov.blower < 0 ? '-' : (ov.blower ? '+' : '0'),
                      sensorData.soilRaw[0], sensorData.soilRaw[1], sensorData.soilRaw[2]);

        if (sensorData.valid) {
            // Backup lokal: seluruh pembacaan valid dicatat ke MicroSD.
            time_t ts = time(nullptr);
            if (ts >= 100000) {
                char line[128];
                snprintf(line, sizeof(line), "%ld,%.1f,%.1f,%.1f,%.2f,%.1f,%s,%d",
                         (long)ts, sensorData.temperature, sensorData.humidity,
                         sensorData.soilMoisture, vpdValue, etcValue,
                         statusTxt, (int)manual);
                sdLogRecord(line);
            }
        }

        unsigned long now = millis();
        if (now - lastLcd >= LCD_INTERVAL) {
            lastLcd = now;
            updateLcd(rTemp, rHum, rSoil, rVpd, rEtc);
        }

        Snapshot s = {};
        s.temperature = rTemp;
        s.humidity = rHum;
        s.soilMoisture = rSoil;
        s.soil2 = rSoil2;
        s.soil3 = rSoil3;
        s.soilOk = sensorData.soilOk;
        s.soilCount = sensorData.soilCount;
        s.vpd = rVpd;
        s.etc = rEtc;
        s.tmean = rTmean;
        s.manual = manual;
        s.mist = mist;
        s.blower = blower;
        s.irigasiRunning = irigasiRunning;
        s.blowerLevel = blowerLevel;   // kondisi relay blower aktual
        s.ov = ov;
        s.irig = irig;
        s.fuzzyMist = fuzzyMist;
        s.fuzzyBlower = fuzzyBlower;
        s.fuzzyIrigasi = fuzzyIrigasi;
        snprintf(s.status, sizeof(s.status), "%s", statusTxt);
        if (!sensorData.soilValid) statusAdd(s.status, sizeof(s.status), " SOILERR");
        if (time(nullptr) < 100000) statusAdd(s.status, sizeof(s.status), " NOSYNC");
        if (!IRRIG_AUTO_ENABLED) statusAdd(s.status, sizeof(s.status), " IRIGOFF");
        lcdScrollSetStatus(s.status);
        s.phase = phaseGet();
        snapshotPublish(s);

        for (uint8_t tick = 1; tick < 2000 / IRRIG_CONTROL_TICK_MS; ++tick) {
            vTaskDelay(pdMS_TO_TICKS(IRRIG_CONTROL_TICK_MS));
            if (!sensorData.valid || ov.irig == 0) irrigationAbort();
            else irrigationUpdate(irigasi, ov.irig == 1, controlIrrigationRequestId());
        }
        vTaskDelay(pdMS_TO_TICKS(IRRIG_CONTROL_TICK_MS));
    }
}

// ==== Task jaringan (core 0, prio 2) ====
static void netTask(void* arg) {
    connectWiFi();
    if (WiFi.status() == WL_CONNECTED) syncTime();

    unsigned long lastPush = 0;
    unsigned long lastCmd = 0;

    for (;;) {
        unsigned long now = millis();

        if (WiFi.status() != WL_CONNECTED && now - lastReconnect >= WIFI_RETRY_MS) {
            Serial.println("[wifi] putus, mencoba reconnect...");
            connectWiFi();
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[wifi] tersambung kembali");
                syncTime();
            }
        }

        Snapshot s = snapshotRead();

        if (WiFi.status() == WL_CONNECTED && now - lastCmd >= DB_POLL_MS) {
            lastCmd = now;
            unsigned long pollStarted = millis();
            pollCommands();
            Serial.printf("[fb][diag] netTask pollCommands elapsed_ms=%lu\n",
                          (unsigned long)(millis() - pollStarted));
        }

        if (now - lastPush >= DB_PUSH_MS) {
            lastPush = now;
            String body;
            FuzzyOutput fz = { s.fuzzyMist, s.fuzzyBlower, s.blowerLevel,
                               s.fuzzyIrigasi, nullptr };
            bool ok = pushSensorLog(s.temperature, s.humidity, s.soilMoisture,
                                    s.soil2, s.soil3, s.soilOk, s.soilCount,
                                    s.vpd, s.etc, s.blowerLevel, s.mist,
                                    s.irigasiRunning, s.status, s.manual, s.phase,
                                    s.ov, s.tmean, s.irig, fz, &body);
            if (ok) {
                sdFlushBacklog();   // pulih: kirim antrean yang tertunda
            } else if (body.length() > 0) {
                sdQueueBacklog(body.c_str());  // gagal/offline: antre ke MicroSD
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void setup() {
    Serial.begin(115200);
    esp_err_t wdtErr = esp_task_wdt_deinit();
    Serial.printf("[wdt] simulasi: deinit=%s\n", esp_err_to_name(wdtErr));
    delay(1000);

    Serial.println("=== Smart Watering System ===");
    Serial.println("Greenhouse IoT - Chili Cultivation");
    Serial.print("Device ID: ");
    Serial.println(DEVICE_ID);
    // Jejak boot: alasan reset supaya reboot tak terduga bisa dibedakan dari restart disengaja.
    Serial.printf("[boot] alasan reset: %s | uptime %lu ms\n",
                  resetReasonName(esp_reset_reason()), (unsigned long)millis());

    initActuators();
    controlInit();
    controlRelease();
    // Task WDT dimatikan khusus simulasi; aktifkan untuk hardware.

    i2cBusInit();     // mutex bus I2C LCD (dua task)
    initLcd();
    initSdLogger();
    initSensor();
    lcdScrollStart(); // task marquee baris status (core 0, prio 1)
    emaInit(tempFilter, EMA_ALPHA_TEMP);
    emaInit(humFilter, EMA_ALPHA_HUM);
    for (int i = 0; i < SOIL_COUNT; i++) emaInit(soilFilter[i], EMA_ALPHA_SOIL);
    phaseInit();
    // Jejak boot: fase + Kc aktif + status irigasi otomatis (gate kalibrasi).
    Serial.printf("[boot] fase: %s (Kc %.2f) | irigasi otomatis %s\n",
                  phaseGet().name, phaseKc(),
                  IRRIG_AUTO_ENABLED ? "AKTIF"
                                      : "MATI (debit pompa & luas media belum dikalibrasi)");
    delay(2000);

    snapMux = xSemaphoreCreateMutex();

    Serial.println("System initialized!");
    xTaskCreatePinnedToCore(controlTask, "control", CONTROL_TASK_STACK, nullptr,
                            CONTROL_TASK_PRIO, nullptr, CONTROL_TASK_CORE);
    xTaskCreatePinnedToCore(netTask, "net", NET_TASK_STACK, nullptr,
                            NET_TASK_PRIO, nullptr, NET_TASK_CORE);
}

// Semua kerja ada di task; loop() tinggal idle.
void loop() {
    vTaskDelay(portMAX_DELAY);
}
