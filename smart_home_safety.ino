/*
 * ============================================================
 *  Smart Home Safety System + TELEGRAM ALERT VERSION
 * ============================================================
 */

#include <Wire.h>
#include <SparkFunLSM6DSO.h>

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// ───────── WiFi + Telegram config ─────────

const char* ssid = "one";
const char* password = "12345678";

#define BOT_TOKEN "8690322306:AAHKOT3VrT_jnvS_K3S4HdsG0-WaWdMo-wk"
#define CHAT_ID "7687474461"

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);


// ───────── Pin Definitions ─────────

#define SDA_PIN        21
#define SCL_PIN        22

#define BUZZER_PIN     19

#define TOUCH_PIN      23
#define PIR_PIN        25
#define RCWL_PIN       26
#define FLAME_L_PIN    35
#define FLAME_R_PIN    34


// ───────── Parameters ─────────

const float IMU_THRESHOLD = 0.05f;
const int   IMU_REQUIRED_HITS = 3;
const int   FLAME_THRESHOLD = 1500;
const long  TOUCH_HOLD_MS = 3000;
const unsigned long ALARM_DUR = 5000;


// ───────── Global State ─────────

LSM6DSO myIMU;

int imuTriggerCount = 0;

unsigned long touchStartTime = 0;
bool emergencyTriggered = false;

bool alarmActive = false;
unsigned long alarmStartTime = 0;

bool emergencyAlarmActive = false;
unsigned long emergencyAlarmStart = 0;


// ───────── Alarm Helpers ─────────

void sendTelegram(String message)
{
  bot.sendMessage(CHAT_ID, message, "");
}


void triggerAlarm(const char* source)
{
  if (!alarmActive)
  {
    Serial.print("⚠️ ALARM: ");
    Serial.println(source);

    alarmActive = true;
    alarmStartTime = millis();

    sendTelegram("⚠️ ALERT: " + String(source));
  }
}


void triggerEmergency()
{
  if (!emergencyAlarmActive)
  {
    Serial.println("🆘 EMERGENCY ALERT TRIGGERED!");

    emergencyAlarmActive = true;
    emergencyAlarmStart = millis();

    sendTelegram("🆘 EMERGENCY BUTTON PRESSED!");
  }
}


// LOW-level trigger buzzer logic

void updateBuzzers()
{
  if (emergencyAlarmActive)
  {
    digitalWrite(BUZZER_PIN, LOW);

    if (millis() - emergencyAlarmStart >= ALARM_DUR)
    {
      digitalWrite(BUZZER_PIN, HIGH);
      emergencyAlarmActive = false;
    }
  }

  else if (alarmActive)
  {
    digitalWrite(BUZZER_PIN, LOW);

    if (millis() - alarmStartTime >= ALARM_DUR)
    {
      digitalWrite(BUZZER_PIN, HIGH);
      alarmActive = false;
    }
  }

  else
  {
    digitalWrite(BUZZER_PIN, HIGH);
  }
}


// ───────── setup() ─────────

void setup()
{
  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!myIMU.begin(0x6A))
    Serial.println("IMU not detected");
  else
    myIMU.initialize();


  pinMode(TOUCH_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(RCWL_PIN, INPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);


  // WiFi connect

  Serial.print("Connecting WiFi");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");

  client.setInsecure();

  configTime(0, 0, "pool.ntp.org");

  sendTelegram("✅ Smart Home Safety System ONLINE");
}


// ───────── loop() ─────────

void loop()
{

  // Earthquake detection

  float ax = myIMU.readFloatAccelX();
  float ay = myIMU.readFloatAccelY();
  float az = myIMU.readFloatAccelZ();

  float totalG = sqrt(ax*ax + ay*ay + az*az);
  float deviation = fabs(totalG - 1.0f);

  if (deviation > IMU_THRESHOLD)
    imuTriggerCount++;
  else
    imuTriggerCount = 0;

  if (imuTriggerCount >= IMU_REQUIRED_HITS)
  {
    triggerAlarm("EARTHQUAKE");
    imuTriggerCount = 0;
  }


  // Emergency button

  int touched = digitalRead(TOUCH_PIN);

  if (touched == HIGH)
  {
    if (touchStartTime == 0)
      touchStartTime = millis();

    if (!emergencyTriggered &&
        millis() - touchStartTime >= TOUCH_HOLD_MS)
    {
      triggerEmergency();
      emergencyTriggered = true;
    }
  }
  else
  {
    touchStartTime = 0;
    emergencyTriggered = false;
  }


  // PIR motion

  if (digitalRead(PIR_PIN))
    triggerAlarm("PIR MOTION");


  // Radar motion (no buzzer, telegram only optional)

  static bool radarTriggered = false;

  if (digitalRead(RCWL_PIN))
  {
    if (!radarTriggered)
    {
      Serial.println("RADAR MOTION DETECTED");
      radarTriggered = true;
    }
  }
  else
    radarTriggered = false;


  // Flame sensors

  int flameL = analogRead(FLAME_L_PIN);
  int flameR = analogRead(FLAME_R_PIN);

  if (flameL < FLAME_THRESHOLD)
    triggerAlarm("FIRE LEFT");

  if (flameR < FLAME_THRESHOLD)
    triggerAlarm("FIRE RIGHT");


  updateBuzzers();

  delay(100);
}