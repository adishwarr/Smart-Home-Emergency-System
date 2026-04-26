/*
 * ============================================================
 *  SafeNest v5 — Smart Home Safety System
 *  - NO ArduinoJson
 *  - NO UniversalTelegramBot
 *  - NO WiFiClientSecure
 *  - Plain HTTPClient only
 *  - INMP441 mic (amplitude SOS)
 *  - Full sensor suite + Telegram control
 *  - WebServer /data endpoint (live sensor JSON)
 * ============================================================
 *
 *  ONLY LIBRARY TO INSTALL:
 *    - SparkFun LSM6DSO  (search in Library Manager)
 *
 *  EVERYTHING ELSE is part of ESP32 Arduino core.
 * ============================================================
 */

#include <Wire.h>
#include <SparkFunLSM6DSO.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <driver/i2s.h>

// ─────────────────────────────────────────
//  WiFi credentials
// ─────────────────────────────────────────
const char* WIFI_SSID = "wifi_name";
const char* WIFI_PASS = "wifi_password";

// ─────────────────────────────────────────
//  Telegram
// ─────────────────────────────────────────
const char* BOT_TOKEN = "bot_token";
const char* CHAT_ID   = "bot_chat_ID";

// ─────────────────────────────────────────
//  Pins
// ─────────────────────────────────────────
#define SDA_PIN     21
#define SCL_PIN     22
#define BUZZER_PIN  19
#define TOUCH_PIN   23
#define PIR_PIN     25
#define RCWL_PIN    26
#define FLAME_L_PIN 35
#define FLAME_R_PIN 34

#define I2S_WS   15
#define I2S_SD   32
#define I2S_SCK  14
#define I2S_PORT I2S_NUM_0

// ─────────────────────────────────────────
//  Parameters  (tune as needed)
// ─────────────────────────────────────────
#define VOICE_THRESHOLD    5000
#define FLAME_THRESHOLD    1500
#define IMU_THRESHOLD      0.05f
#define IMU_REQUIRED_HITS  3
#define TOUCH_HOLD_MS      3000
#define ALARM_DUR_MS       5000
#define TELEGRAM_POLL_MS   5000
#define MIC_COOLDOWN_MS    6000
#define SAMPLE_RATE        16000
#define MIC_BUF_LEN        128

// ─────────────────────────────────────────
//  Device flags  (toggled via Telegram)
// ─────────────────────────────────────────
bool en_pir   = true;
bool en_radar = true;
bool en_flame = true;
bool en_imu   = true;
bool en_mic   = true;
bool en_touch = true;

// ─────────────────────────────────────────
//  Runtime state
// ─────────────────────────────────────────
LSM6DSO imu;
bool imuReady = false;
bool i2sReady = false;

int           imuHits          = 0;
unsigned long touchStart       = 0;
bool          touchFired       = false;

bool          alarmOn          = false;
unsigned long alarmStart       = 0;
bool          sosOn            = false;
unsigned long sosStart         = 0;

unsigned long lastPoll         = 0;
unsigned long lastMicTrig      = 0;
long          lastUpdateId     = 0;

// ─────────────────────────────────────────
//  WebServer — live sensor data
// ─────────────────────────────────────────
WebServer server(80);

float  latestAx = 0.0f, latestAy = 0.0f, latestAz = 0.0f;
float  latestDeviation = 0.0f;
int    latestFlameL = 0, latestFlameR = 0;
int    latestTouch = LOW, latestPir = LOW, latestRadar = LOW;
String lastAlert = "SYSTEM STARTED";

void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin",          "*");
  server.sendHeader("Access-Control-Allow-Methods",         "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers",         "Content-Type");
  server.sendHeader("Access-Control-Allow-Private-Network", "true");
  server.sendHeader("Access-Control-Max-Age",               "86400");
}

String jsonEscape(const String& s) {
  String out = "";
  for (int i = 0; i < s.length(); i++) {
    char c = s[i];
    if      (c == '"')  out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else                out += c;
  }
  return out;
}

void handleData() {
  addCorsHeaders();
  server.sendHeader("Content-Type", "application/json");
  String json = "{";
  json += "\"ax\":"          + String(latestAx, 3)        + ",";
  json += "\"ay\":"          + String(latestAy, 3)        + ",";
  json += "\"az\":"          + String(latestAz, 3)        + ",";
  json += "\"deviation\":"   + String(latestDeviation, 3) + ",";
  json += "\"flameL\":"      + String(latestFlameL)       + ",";
  json += "\"flameR\":"      + String(latestFlameR)       + ",";
  json += "\"temperatureC\":null,"                                  ;
  json += "\"touch\":"       + String(latestTouch)        + ",";
  json += "\"pir\":"         + String(latestPir)          + ",";
  json += "\"radar\":"       + String(latestRadar)        + ",";
  json += "\"lastAlert\":\""  + jsonEscape(lastAlert)     + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleOptions() {
  addCorsHeaders();
  server.send(204, "text/plain", "");
}

void handleCors() {
  addCorsHeaders();
  if (server.method() == HTTP_OPTIONS) {
    server.send(204, "text/plain", "");
  } else {
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  }
}

void setupWebServer() {
  server.on("/data",    HTTP_GET,     handleData);
  server.on("/data",    HTTP_OPTIONS, handleOptions);
  server.on("/cors",    handleCors);
  server.onNotFound([]() {
    addCorsHeaders();
    server.send(404, "application/json", "{\"error\":\"Not found\"}");
  });
  server.begin();
  Serial.println("Web server started");
}

void tickWebServer() {
  server.handleClient();
}


// ═══════════════════════════════════════════════════════════
//  TELEGRAM  — raw HTTP, no SSL, no external JSON lib
// ═══════════════════════════════════════════════════════════

// URL-encode just enough for Telegram text messages
String urlEncode(String s) {
  s.replace("%",  "%25");
  s.replace(" ",  "%20");
  s.replace("\n", "%0A");
  s.replace("/",  "%2F");
  s.replace(":",  "%3A");
  s.replace("_",  "%5F");
  return s;
}

void tgSend(const String& msg)
{
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();   // skip certificate validation

  if (!client.connect("api.telegram.org", 443)) {
    Serial.println("[TG send] connect failed");
    return;
  }

  String url =
    "/bot" + String(BOT_TOKEN) +
    "/sendMessage?chat_id=" +
    String(CHAT_ID) +
    "&text=" +
    urlEncode(msg);

  client.print("GET " + url + " HTTP/1.1\r\n");
  client.print("Host: api.telegram.org\r\n");
  client.print("Connection: close\r\n\r\n");

  delay(50);

  while (client.available()) client.read();

  client.stop();
}
long jsonLong(const String& json, const String& key) {
  String search = "\"" + key + "\":";
  int i = json.indexOf(search);
  if (i < 0) return -1;
  i += search.length();
  int j = i;
  while (j < json.length() &&
         (isDigit(json[j]) || json[j] == '-')) j++;
  return json.substring(i, j).toInt();
}

String jsonString(const String& json, const String& key) {
  String search = "\"" + key + "\":\"";
  int i = json.indexOf(search);
  if (i < 0) return "";
  i += search.length();
  int j = json.indexOf("\"", i);
  if (j < 0) return "";
  return json.substring(i, j);
}
// ── Poll and process one command per call ──────────────────
void tgPoll() {

  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();   // skip TLS certificate validation (saves RAM)

  if (!client.connect("api.telegram.org", 443)) {
    Serial.println("[TG poll] connect failed");
    return;
  }

  String url = "/bot";
  url += BOT_TOKEN;
  url += "/getUpdates?offset=";
  url += String(lastUpdateId + 1);
  url += "&limit=1";

  client.print("GET " + url + " HTTP/1.1\r\n");
  client.print("Host: api.telegram.org\r\n");
  client.print("Connection: close\r\n\r\n");

  String body = "";

  unsigned long timeout = millis();

  while (client.connected() && millis() - timeout < 2500) {
    while (client.available()) {
      body += (char)client.read();
    }
  }

  client.stop();

  if (body.indexOf("\"update_id\"") < 0) return;

  long uid = jsonLong(body, "update_id");

  if (uid <= lastUpdateId) return;

  lastUpdateId = uid;

  String text = jsonString(body, "text");

  if (text == "") return;

  Serial.println("[CMD] " + text);


  // ── Commands (unchanged exactly as you requested) ─────────

  if      (text == "/pir_on")     { en_pir   = true;  tgSend("PIR ON");          }
  else if (text == "/pir_off")    { en_pir   = false; tgSend("PIR OFF");         }
  else if (text == "/radar_on")   { en_radar = true;  tgSend("Radar ON");        }
  else if (text == "/radar_off")  { en_radar = false; tgSend("Radar OFF");       }
  else if (text == "/flame_on")   { en_flame = true;  tgSend("Flame ON");        }
  else if (text == "/flame_off")  { en_flame = false; tgSend("Flame OFF");       }
  else if (text == "/imu_on")     { en_imu   = true;  tgSend("Earthquake ON");   }
  else if (text == "/imu_off")    { en_imu   = false; tgSend("Earthquake OFF");  }
  else if (text == "/mic_on")     { en_mic   = true;  tgSend("Mic ON");          }
  else if (text == "/mic_off")    { en_mic   = false; tgSend("Mic OFF");         }
  else if (text == "/touch_on")   { en_touch = true;  tgSend("SOS Button ON");   }
  else if (text == "/touch_off")  { en_touch = false; tgSend("SOS Button OFF");  }

  else if (text == "/buzzer_off") {
    alarmOn = false;
    sosOn   = false;
    digitalWrite(BUZZER_PIN, HIGH);
    tgSend("Buzzer silenced");
  }

  else if (text == "/all_on") {
    en_pir = en_radar = en_flame =
    en_imu = en_mic   = en_touch = true;
    tgSend("All sensors ON");
  }

  else if (text == "/all_off") {
    en_pir = en_radar = en_flame =
    en_imu = en_mic   = en_touch = false;
    tgSend("All sensors OFF");
  }

  else if (text == "/status") {

    String s;

    s += "SafeNest Status\n";
    s += "PIR:   " + String(en_pir   ? "ON":"OFF") + "\n";
    s += "Radar: " + String(en_radar ? "ON":"OFF") + "\n";
    s += "Flame: " + String(en_flame ? "ON":"OFF") + "\n";
    s += "IMU:   " + String(en_imu   ? "ON":"OFF") + "\n";
    s += "Mic:   " + String(en_mic   ? "ON":"OFF") + "\n";
    s += "Touch: " + String(en_touch ? "ON":"OFF") + "\n";
    s += "Heap:  " + String(ESP.getFreeHeap()) + " bytes\n";
    s += "Up:    " + String(millis()/1000) + "s";

    tgSend(s);
  }

  else if (text == "/start" || text == "/help") {

    tgSend(
      "SafeNest Commands\n"
      "/pir_on  /pir_off\n"
      "/radar_on  /radar_off\n"
      "/flame_on  /flame_off\n"
      "/imu_on  /imu_off\n"
      "/mic_on  /mic_off\n"
      "/touch_on  /touch_off\n"
      "/all_on  /all_off\n"
      "/buzzer_off\n"
      "/status"
    );
  }

  else {

    tgSend("Unknown cmd. Send /help");
  }
}

// ═══════════════════════════════════════════════════════════
//  ALARM
// ═══════════════════════════════════════════════════════════

void triggerAlarm(const char* src) {
  if (!alarmOn) {
    Serial.printf("[ALARM] %s\n", src);
    alarmOn    = true;
    alarmStart = millis();
    lastAlert  = String(src);           // ← update web data
    tgSend("ALERT: " + String(src));
  }
}

void triggerSOS() {
  if (!sosOn) {
    Serial.println("[SOS] Emergency triggered");
    sosOn    = true;
    sosStart = millis();
    lastAlert = "SOS EMERGENCY";        // ← update web data
    tgSend("SOS EMERGENCY TRIGGERED");
  }
}

void updateBuzzer() {
  if (sosOn) {
    digitalWrite(BUZZER_PIN, LOW);
    if (millis() - sosStart >= ALARM_DUR_MS) {
      digitalWrite(BUZZER_PIN, HIGH);
      sosOn = false;
    }
  }
  else if (alarmOn) {
    digitalWrite(BUZZER_PIN, LOW);
    if (millis() - alarmStart >= ALARM_DUR_MS) {
      digitalWrite(BUZZER_PIN, HIGH);
      alarmOn = false;
    }
  }
  else {
    digitalWrite(BUZZER_PIN, HIGH);
  }
}


// ═══════════════════════════════════════════════════════════
//  I2S  (INMP441)
// ═══════════════════════════════════════════════════════════

bool i2sInit() {
  i2s_driver_uninstall(I2S_PORT);       // safe even if not yet installed

  i2s_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));

  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;

  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT;

  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;

  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;

  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = MIC_BUF_LEN;

  cfg.use_apll = true;
  cfg.tx_desc_auto_clear = false;
  cfg.fixed_mclk = 0;

  i2s_pin_config_t pins;
  memset(&pins, 0, sizeof(pins));
  pins.bck_io_num   = I2S_SCK;
  pins.ws_io_num    = I2S_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num  = I2S_SD;

  if (i2s_driver_install(I2S_PORT, &cfg, 0, NULL) != ESP_OK) {
    Serial.println("[I2S] install FAILED");
    return false;
  }
  if (i2s_set_pin(I2S_PORT, &pins) != ESP_OK) {
    Serial.println("[I2S] pin FAILED");
    return false;
  }

  i2s_zero_dma_buffer(I2S_PORT);
  Serial.println("[I2S] OK");
  return true;
}

void handleMic() {
  if (!en_mic || !i2sReady) return;
  if (millis() - lastMicTrig < MIC_COOLDOWN_MS) return;

  int32_t raw[MIC_BUF_LEN];
  size_t  bytesRead = 0;

  if (i2s_read(I2S_PORT, raw, sizeof(raw),
               &bytesRead, pdMS_TO_TICKS(10)) != ESP_OK) return;
  if (bytesRead == 0) return;

  int32_t peak    = 0;
  int     samples = bytesRead / sizeof(int32_t);
  for (int i = 0; i < samples; i++) {
    int32_t s = abs(raw[i] >> 16);
    if (s > peak) peak = s;
  }

  // Uncomment to calibrate VOICE_THRESHOLD:
  // Serial.println(peak);

  if (peak > VOICE_THRESHOLD) {
   // Serial.printf("[MIC] peak=%d\n", peak);
    lastMicTrig = millis();
    triggerSOS();
    tgSend("MIC SOS: Loud sound detected");
  }
}


// ═══════════════════════════════════════════════════════════
//  WIFI WATCHDOG
// ═══════════════════════════════════════════════════════════

void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.println("[WiFi] reconnecting...");
  WiFi.disconnect();
  delay(200);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 12000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(
    WiFi.status() == WL_CONNECTED
      ? "\n[WiFi] back: " + WiFi.localIP().toString()
      : "\n[WiFi] failed — will retry"
  );
}


// ═══════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetWidth(12);
  analogSetAttenuation(ADC_11db);
  delay(500);
  Serial.println("\n=== SafeNest v5 ===");
  Serial.printf("Heap at start: %d\n", ESP.getFreeHeap());

  // ── GPIO ──
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);   // buzzer off (active LOW)
  pinMode(TOUCH_PIN, INPUT);
  pinMode(PIR_PIN,   INPUT);
  pinMode(RCWL_PIN,  INPUT);

  analogReadResolution(12);
  analogSetPinAttenuation(FLAME_L_PIN, ADC_11db);
  analogSetPinAttenuation(FLAME_R_PIN, ADC_11db);
  // ── I2C + IMU ──
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(150);

  if (!imu.begin(0x6A)) {
    Serial.println("[IMU] not found — disabled");
    en_imu  = false;
    imuReady = false;
  } else {
    imu.initialize();
    imuReady = true;
    Serial.println("[IMU] OK");
  }

  // ── WiFi (blocking, 20s timeout then reboot) ──
  Serial.print("[WiFi] connecting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t0 > 20000) {
      Serial.println("\n[WiFi] timeout — rebooting");
      ESP.restart();
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] OK: " + WiFi.localIP().toString());
  Serial.printf("Heap after WiFi: %d\n", ESP.getFreeHeap());

  // ── Web server ──
  setupWebServer();

  // ── I2S — always after WiFi ──
  i2sReady = i2sInit();
  if (!i2sReady) en_mic = false;
  Serial.printf("Heap after I2S:  %d\n", ESP.getFreeHeap());

  tgSend("SafeNest ONLINE\n/help for commands");
  Serial.println("[Boot] complete\n");
}


// ═══════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════

void loop() {

  // ── WiFi watchdog ────────────────────────────────────────
  ensureWiFi();

  // ── Web server tick ──────────────────────────────────────
  tickWebServer();

  // ── Telegram poll ────────────────────────────────────────
  if (millis() - lastPoll > TELEGRAM_POLL_MS) {
    lastPoll = millis();
    tgPoll();
  }

  // ── Earthquake ───────────────────────────────────────────
  if (en_imu && imuReady) {
    float ax  = imu.readFloatAccelX();
    float ay  = imu.readFloatAccelY();
    float az  = imu.readFloatAccelZ();
    float dev = fabs(sqrtf(ax*ax + ay*ay + az*az) - 1.0f);

    latestAx        = ax;               // ← update web data
    latestAy        = ay;
    latestAz        = az;
    latestDeviation = dev;

    if (dev > IMU_THRESHOLD) imuHits++;
    else                     imuHits = 0;

    if (imuHits >= IMU_REQUIRED_HITS) {
      triggerAlarm("EARTHQUAKE");
      imuHits = 0;
    }
  }

  // ── SOS touch button (hold 3s) ───────────────────────────
  if (en_touch) {
    latestTouch = digitalRead(TOUCH_PIN); // ← update web data
    if (latestTouch == HIGH) {
      if (touchStart == 0) touchStart = millis();
      if (!touchFired && millis() - touchStart >= TOUCH_HOLD_MS) {
        triggerSOS();
        touchFired = true;
      }
    } else {
      touchStart = 0;
      touchFired = false;
    }
  }

  // ── PIR ──────────────────────────────────────────────────
  latestPir = digitalRead(PIR_PIN);       // ← update web data
  if (en_pir && latestPir == HIGH)
    triggerAlarm("PIR MOTION");

  // ── Radar ────────────────────────────────────────────────
  if (en_radar) {
    static bool radarLast = false;
    bool radarNow = digitalRead(RCWL_PIN) == HIGH;
    latestRadar = radarNow ? HIGH : LOW;  // ← update web data
    if (radarNow && !radarLast)
      Serial.println("[RADAR] motion");
      // tgSend("Radar motion");   // uncomment if you want Telegram radar alerts
    radarLast = radarNow;
  }

  // ── Flame ────────────────────────────────────────────────
  latestFlameL = analogRead(FLAME_L_PIN); // ← update web data
  latestFlameR = analogRead(FLAME_R_PIN);
  if (en_flame) {
    if (latestFlameL < FLAME_THRESHOLD) triggerAlarm("FIRE LEFT");
    if (latestFlameR < FLAME_THRESHOLD) triggerAlarm("FIRE RIGHT");
  }

  // ── Mic / Voice SOS ──────────────────────────────────────
  handleMic();

  // ── Buzzer ───────────────────────────────────────────────
  updateBuzzer();

  delay(100);
}
