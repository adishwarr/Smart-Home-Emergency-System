# 🏠 Smart Home Safety System using ESP32 (Multi-Sensor + Telegram Alerts)

A **real-time IoT-based Smart Home Safety Monitoring System** built using **ESP32** that detects **earthquakes, fire hazards, motion intrusion, radar presence detection, and emergency touch activation**, and instantly sends **Telegram alerts** to the homeowner.

This system integrates **multiple environmental safety layers into a single embedded platform**, enabling proactive monitoring and rapid response to hazards.

---

# 📌 Project Overview

Traditional safety systems typically monitor only one threat at a time.
This project combines **multi-sensor hazard detection + wireless alerting + local alarm triggering** into a single intelligent IoT safety solution.

The system continuously monitors:

* Earthquake vibrations
* Fire presence
* Human motion
* Radar-based proximity detection
* Emergency touch-trigger activation

When abnormal activity is detected:

1️⃣ Local buzzer alarm activates
2️⃣ Telegram alert is sent instantly
3️⃣ Event logged via Serial Monitor

---

# 🚀 Features

✅ Earthquake detection using **LSM6DSO IMU sensor**
✅ Dual flame sensors for **fire detection (left + right zone)**
✅ PIR-based **motion detection**
✅ RCWL radar-based **presence detection**
✅ Emergency **touch button (3-second hold)** activation
✅ Instant **Telegram alert notifications**
✅ WiFi-enabled IoT architecture
✅ Local buzzer alarm system
✅ False-trigger reduction logic implemented

---

# 🧠 System Architecture

```
Sensors → ESP32 → Detection Logic → Alert Engine
                        ↓
                Buzzer Activation
                        ↓
                Telegram Notification
```

The ESP32 acts as the central processing unit handling:

* Sensor polling
* Signal filtering
* Event detection logic
* Alert generation
* Notification delivery

---

# 🧰 Hardware Components Required

| Component               | Quantity | Purpose              |
| ----------------------- | -------- | -------------------- |
| ESP32 Dev Board         | 1        | Main controller      |
| LSM6DSO IMU             | 1        | Earthquake detection |
| PIR Motion Sensor       | 1        | Motion detection     |
| RCWL Radar Sensor       | 1        | Presence detection   |
| Flame Sensors           | 2        | Fire detection       |
| Capacitive Touch Sensor | 1        | Emergency trigger    |
| Active Buzzer           | 1        | Alarm output         |
| Jumper Wires            | —        | Connections          |
| Breadboard              | Optional | Prototyping          |

---

# ⚙️ Pin Configuration

| Device             | ESP32 Pin |
| ------------------ | --------- |
| SDA                | GPIO 21   |
| SCL                | GPIO 22   |
| Touch Sensor       | GPIO 23   |
| PIR Sensor         | GPIO 25   |
| RCWL Sensor        | GPIO 26   |
| Flame Sensor Left  | GPIO 35   |
| Flame Sensor Right | GPIO 34   |
| Buzzer             | GPIO 19   |

---

# 🔌 Circuit Wiring Instructions

### LSM6DSO IMU

| IMU Pin | ESP32 Pin |
| ------- | --------- |
| SDA     | GPIO 21   |
| SCL     | GPIO 22   |
| VCC     | 3.3V      |
| GND     | GND       |

---

### PIR Sensor

| PIR Pin | ESP32   |
| ------- | ------- |
| OUT     | GPIO 25 |
| VCC     | 5V      |
| GND     | GND     |

---

### RCWL Radar Sensor

| RCWL Pin | ESP32   |
| -------- | ------- |
| OUT      | GPIO 26 |
| VCC      | 5V      |
| GND      | GND     |

---

### Flame Sensors

| Flame Sensor | ESP32   |
| ------------ | ------- |
| Left         | GPIO 35 |
| Right        | GPIO 34 |
| VCC          | 3.3V    |
| GND          | GND     |

---

### Touch Sensor

| Touch Pin | ESP32   |
| --------- | ------- |
| OUT       | GPIO 23 |
| VCC       | 3.3V    |
| GND       | GND     |

---

### Buzzer

| Buzzer Pin | ESP32   |
| ---------- | ------- |
| Positive   | GPIO 19 |
| Negative   | GND     |

---

# 📡 Telegram Alert Integration

The system sends alerts when:

* Fire detected
* Motion detected
* Earthquake vibration detected
* Radar presence detected
* Emergency button pressed
* Device starts successfully

Example alert:

```
⚠️ ALERT: FIRE LEFT
```

---

# 🤖 Creating Telegram Bot (Step-by-Step)

### Step 1

Open Telegram → Search:

```
@BotFather
```

---

### Step 2

Send command:

```
/start
```

---

### Step 3

Create new bot

```
/newbot
```

---

### Step 4

Copy generated BOT TOKEN

Example:

```
123456789:ABCDEFxxxxxxxxxxxxxxxx
```

---

### Step 5

Get Chat ID

Open browser:

```
https://api.telegram.org/bot<BOT_TOKEN>/getUpdates
```

Send message to your bot once
Then reload link → copy Chat ID

---

# 🔧 Software Installation

Install these libraries inside **Arduino IDE**

```
Wire
SparkFun LSM6DSO
WiFi
WiFiClientSecure
UniversalTelegramBot
```

---

# 🛠️ Configuration Steps

Inside code update:

### WiFi Credentials

```cpp
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
```

---

### Telegram Credentials

```cpp
#define BOT_TOKEN "YOUR_BOT_TOKEN"
#define CHAT_ID "YOUR_CHAT_ID"
```

---

# ▶️ Upload Instructions

1️⃣ Connect ESP32
2️⃣ Open Arduino IDE
3️⃣ Select board:

```
ESP32 Dev Module
```

4️⃣ Select correct COM Port
5️⃣ Upload sketch

---

# 📊 Detection Logic Explained

## Earthquake Detection

Triggered when acceleration deviation crosses threshold multiple times:

```
Threshold = 0.05g
Required hits = 3
```

Reduces false positives caused by vibration noise.

---

## Fire Detection

Triggered when flame sensor value drops below:

```
1500
```

Separate detection zones:

* Left sensor
* Right sensor

---

## Motion Detection

Triggered instantly when PIR output HIGH detected.

---

## Radar Presence Detection

Detects subtle movement missed by PIR sensors.

Useful for:

* intruder detection
* human presence monitoring
* blind-spot coverage

---

## Emergency Touch Activation

Touch sensor must remain pressed for:

```
3 seconds
```

Prevents accidental triggers.

---

## Alarm Duration

Buzzer active duration:

```
5 seconds
```

---

# 📂 Project Folder Structure

```
Smart-Home-Safety-System/
│
├── smart_home_safety.ino
├── README.md
└── circuit_diagram.png (optional)
```

---

# 📈 Applications

This system can be used in:

* Smart homes
* Hostel safety monitoring
* Elderly emergency alert systems
* Research labs
* Industrial prototype safety units
* IoT learning projects

---

# 🔮 Future Improvements

Possible upgrades:

* Firebase cloud logging
* Mobile monitoring dashboard
* SMS backup alerts
* Battery backup monitoring
* Camera-based intrusion capture
* OTA firmware updates

---

# 🧪 Testing Procedure

Test each module individually:

### Motion Test

Walk in front of PIR sensor → verify alert received

### Fire Test

Bring lighter near flame sensor → verify alert

### Earthquake Test

Shake IMU module → verify vibration detection

### Radar Test

Move hand slowly near sensor → verify detection

### Touch Test

Hold touch pad 3 seconds → emergency alert triggered

---

# ⚠️ Troubleshooting Guide

| Issue                         | Solution               |
| ----------------------------- | ---------------------- |
| Telegram alerts not working   | Check BOT TOKEN        |
| WiFi not connecting           | Verify SSID/password   |
| IMU not detected              | Check SDA/SCL wiring   |
| Flame sensor always triggered | Adjust threshold       |
| Touch sensor false triggers   | Increase hold duration |

---

# 👨‍💻 Author

**Adishwar Singh**

Embedded Systems • IoT • Smart Safety Systems

---

# ⭐ Contribution

Contributions welcome!

If this project helped you:

```
Star ⭐ the repository
```
