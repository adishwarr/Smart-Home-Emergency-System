# SafeNest v5 — Firmware Documentation

> **Smart Home Safety & Emergency SOS System**  
> ESP32-based | Telegram-controlled | Live WebServer API

---

## Table of Contents

1. [Overview](#overview)
2. [Hardware Requirements](#hardware-requirements)
3. [Pin Configuration](#pin-configuration)
4. [Library Dependencies](#library-dependencies)
5. [Configuration](#configuration)
6. [System Architecture](#system-architecture)
7. [Sensor Modules](#sensor-modules)
8. [Telegram Bot Commands](#telegram-bot-commands)
9. [WebServer API](#webserver-api)
10. [Alarm & SOS Logic](#alarm--sos-logic)
11. [Tunable Parameters](#tunable-parameters)
12. [Boot Sequence](#boot-sequence)
13. [Main Loop Flow](#main-loop-flow)
14. [Troubleshooting](#troubleshooting)

---

## Overview

SafeNest v5 is a real-time home safety firmware for the **ESP32** microcontroller. It integrates a multi-sensor network for detecting motion, fire, earthquakes, and voice-triggered SOS events. The system communicates alerts via **Telegram** and exposes a lightweight **HTTP WebServer** that streams live sensor data as JSON — enabling integration with dashboards, mobile apps, and Mini Apps.

### Key Features

| Feature | Description |
|---|---|
| Telegram Bot Control | Toggle sensors, silence alarms, query status remotely |
| Live JSON API | `GET /data` returns all sensor readings in real time |
| Voice SOS | INMP441 I²S microphone detects loud sound amplitude |
| Earthquake Detection | LSM6DSO IMU monitors accelerometer deviation |
| Fire Detection | Dual flame sensors (left + right) with ADC thresholds |
| PIR Motion | HC-SR501 passive infrared motion detection |
| Radar Motion | RCWL-0516 microwave radar for secondary motion coverage |
| Touch SOS | Capacitive touch button — hold 3 seconds to trigger SOS |
| WiFi Watchdog | Auto-reconnects on network loss without reboot |
| No external JSON library | Custom lightweight JSON parser; no ArduinoJson dependency |

---

## Hardware Requirements

- **Microcontroller:** ESP32 (DevKit or equivalent)
- **IMU:** SparkFun LSM6DSO (I²C, address `0x6A`)
- **Microphone:** INMP441 (I²S digital MEMS microphone)
- **Motion Sensors:** HC-SR501 PIR + RCWL-0516 Radar
- **Fire Sensors:** Dual analog flame sensor modules
- **Input:** Capacitive touch sensor (TTP223 or similar)
- **Output:** Active buzzer (active LOW)

---

## Pin Configuration

| Signal | GPIO | Notes |
|---|---|---|
| I²C SDA | 21 | LSM6DSO IMU |
| I²C SCL | 22 | LSM6DSO IMU |
| Buzzer | 19 | Active LOW — HIGH = OFF |
| Touch | 23 | Digital input, HIGH = pressed |
| PIR | 25 | Digital input, HIGH = motion |
| Radar (RCWL) | 26 | Digital input, HIGH = motion |
| Flame Left | 35 | ADC input (input-only GPIO) |
| Flame Right | 34 | ADC input (input-only GPIO) |
| I²S WS (LRCLK) | 15 | INMP441 |
| I²S SD (Data) | 32 | INMP441 |
| I²S SCK (BCLK) | 14 | INMP441 |

> **Note:** GPIO 34 and 35 are input-only pins on the ESP32 and do not have internal pull-up/down resistors.

---

## Library Dependencies

| Library | Source | Required |
|---|---|---|
| `SparkFun LSM6DSO` | Arduino Library Manager | ✅ Must install |
| `WiFi.h` | ESP32 Arduino Core | ✅ Built-in |
| `WiFiClientSecure.h` | ESP32 Arduino Core | ✅ Built-in |
| `WebServer.h` | ESP32 Arduino Core | ✅ Built-in |
| `driver/i2s.h` | ESP32 IDF (included in core) | ✅ Built-in |
| `Wire.h` | ESP32 Arduino Core | ✅ Built-in |

**Only one library needs to be manually installed:** `SparkFun LSM6DSO`  
Search for it in **Arduino IDE → Tools → Manage Libraries**.

---

## Configuration

All credentials and identifiers are defined near the top of the sketch:

```cpp
// WiFi
const char* WIFI_SSID = "your_ssid";
const char* WIFI_PASS = "your_password";

// Telegram Bot
const char* BOT_TOKEN = "your_bot_token";
const char* CHAT_ID   = "your_chat_id";
```

### Obtaining Telegram Credentials

1. **Bot Token** — Message `@BotFather` on Telegram → `/newbot` → copy the token.
2. **Chat ID** — Message `@userinfobot` on Telegram, or send a message to your bot and check `getUpdates`.

---

## System Architecture

```
┌─────────────────────────────────────────────────────────┐
│                        ESP32                            │
│                                                         │
│  ┌──────────┐   ┌──────────┐   ┌──────────────────┐   │
│  │  Sensors │   │ Telegram │   │  WebServer :80   │   │
│  │  Suite   │──▶│   Bot    │   │  GET /data (JSON)│   │
│  └──────────┘   └──────────┘   └──────────────────┘   │
│       │               │                  │              │
│       └───────────────┴──────────────────┘              │
│                       │                                  │
│              ┌────────▼────────┐                         │
│              │  Alarm & SOS   │                         │
│              │  State Machine │                         │
│              └────────────────┘                         │
└─────────────────────────────────────────────────────────┘
```

The firmware runs a cooperative single-loop design. Each subsystem is polled every `100ms`. Telegram polling occurs every `5000ms` to stay within rate limits without blocking the loop.

---

## Sensor Modules

### 1. PIR Motion Sensor

Detects infrared motion. Triggers an `ALARM` when the GPIO goes HIGH.

```
Enable/Disable: /pir_on  /pir_off
Telegram Alert: "ALERT: PIR MOTION"
```

### 2. RCWL-0516 Radar

Microwave radar for motion detection. Fires a Serial log on positive edge. Telegram alerts are **commented out by default** to reduce noise — uncomment `tgSend("Radar motion")` in the loop to enable.

```
Enable/Disable: /radar_on  /radar_off
```

### 3. Flame Sensors (Dual)

Two analog flame sensors read via ADC (12-bit). A reading **below** `FLAME_THRESHOLD` (1500) indicates flame detection.

```
Enable/Disable: /flame_on  /flame_off
Telegram Alert: "ALERT: FIRE LEFT" or "ALERT: FIRE RIGHT"
```

> Lower ADC values = higher IR intensity = fire detected. Adjust `FLAME_THRESHOLD` to account for ambient IR in your environment.

### 4. IMU — Earthquake Detection (LSM6DSO)

Reads 3-axis accelerometer data. Computes the magnitude deviation from 1g (earth's gravity). If deviation exceeds `IMU_THRESHOLD` for `IMU_REQUIRED_HITS` consecutive readings, an earthquake alarm fires.

```
Deviation = |√(ax² + ay² + az²) − 1.0|

Enable/Disable: /imu_on  /imu_off
Telegram Alert: "ALERT: EARTHQUAKE"
I²C Address:    0x6A
```

### 5. INMP441 Microphone (Voice SOS)

Reads audio samples via I²S. Computes peak amplitude in each buffer. If the peak exceeds `VOICE_THRESHOLD` (5000), a SOS is triggered. A `MIC_COOLDOWN_MS` (6s) prevents repeated triggers.

```
Enable/Disable: /mic_on  /mic_off
Telegram Alert: "MIC SOS: Loud sound detected"

Calibration:
  Uncomment → Serial.println(peak);
  in handleMic() to monitor raw peak values and adjust VOICE_THRESHOLD.
```

### 6. Capacitive Touch (SOS Button)

Hold the touch pin HIGH for `TOUCH_HOLD_MS` (3000ms) to trigger an SOS. Releases reset the timer.

```
Enable/Disable: /touch_on  /touch_off
Telegram Alert: "SOS EMERGENCY TRIGGERED"
```

---

## Telegram Bot Commands

All commands are case-sensitive and must begin with `/`.

### Sensor Toggle Commands

| Command | Action |
|---|---|
| `/pir_on` | Enable PIR motion sensor |
| `/pir_off` | Disable PIR motion sensor |
| `/radar_on` | Enable RCWL radar |
| `/radar_off` | Disable RCWL radar |
| `/flame_on` | Enable flame sensors |
| `/flame_off` | Disable flame sensors |
| `/imu_on` | Enable earthquake detection |
| `/imu_off` | Disable earthquake detection |
| `/mic_on` | Enable voice SOS microphone |
| `/mic_off` | Disable voice SOS microphone |
| `/touch_on` | Enable touch SOS button |
| `/touch_off` | Disable touch SOS button |

### Global Commands

| Command | Action |
|---|---|
| `/all_on` | Enable all sensors simultaneously |
| `/all_off` | Disable all sensors simultaneously |
| `/buzzer_off` | Immediately silence the buzzer and clear alarm/SOS state |
| `/status` | Print current sensor states, free heap, and uptime |
| `/start` or `/help` | Print the command reference |

### `/status` Response Example

```
SafeNest Status
PIR:   ON
Radar: ON
Flame: ON
IMU:   ON
Mic:   ON
Touch: ON
Heap:  142320 bytes
Up:    374s
```

---

## WebServer API

The ESP32 runs an HTTP server on **port 80**. It is accessible on the local network at the ESP32's IP address (printed to Serial on boot).

### Endpoints

| Method | Path | Description |
|---|---|---|
| `GET` | `/data` | Returns all live sensor readings as JSON |
| `OPTIONS` | `/data` | CORS preflight response |
| `GET/OPTIONS` | `/cors` | CORS test endpoint |

### `GET /data` — Response Schema

```json
{
  "ax": 0.012,
  "ay": -0.005,
  "az": 0.997,
  "deviation": 0.003,
  "flameL": 2843,
  "flameR": 2901,
  "temperatureC": null,
  "touch": 0,
  "pir": 0,
  "radar": 0,
  "lastAlert": "SYSTEM STARTED"
}
```

| Field | Type | Description |
|---|---|---|
| `ax`, `ay`, `az` | `float` | Accelerometer axes in g (from LSM6DSO) |
| `deviation` | `float` | Magnitude deviation from 1g |
| `flameL` | `int` | Left flame sensor ADC value (0–4095); lower = more IR |
| `flameR` | `int` | Right flame sensor ADC value (0–4095); lower = more IR |
| `temperatureC` | `null` | Reserved; not yet implemented |
| `touch` | `int` | Touch pin state: `0` = not pressed, `1` = pressed |
| `pir` | `int` | PIR state: `0` = no motion, `1` = motion |
| `radar` | `int` | Radar state: `0` = no motion, `1` = motion |
| `lastAlert` | `string` | Text of the most recent alert or `"SYSTEM STARTED"` |

### CORS Headers

All responses include:

```
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, OPTIONS
Access-Control-Allow-Private-Network: true
```

This enables the API to be consumed by web dashboards and Mini Apps running on any origin, including local network apps accessing private IP addresses.

---

## Alarm & SOS Logic

There are two independent alert states:

| State | Trigger | Buzzer | Duration |
|---|---|---|---|
| `alarmOn` | Sensor threshold exceeded (PIR, Flame, IMU) | Active LOW | `ALARM_DUR_MS` (5s) |
| `sosOn` | Touch hold or Voice SOS | Active LOW | `ALARM_DUR_MS` (5s) |

**Priority:** `sosOn` takes precedence over `alarmOn` in `updateBuzzer()`.  
**Manual override:** `/buzzer_off` clears both states immediately and drives the buzzer HIGH (off).  
**Auto-clear:** Both states auto-clear after `ALARM_DUR_MS` elapses.  
**`lastAlert`** is updated on every `triggerAlarm()` and `triggerSOS()` call and is reflected in the `/data` JSON endpoint.

---

## Tunable Parameters

All thresholds are defined as `#define` macros at the top of the sketch.

| Parameter | Default | Description |
|---|---|---|
| `VOICE_THRESHOLD` | `5000` | Peak I²S amplitude to trigger voice SOS |
| `FLAME_THRESHOLD` | `1500` | ADC value below which flame is detected |
| `IMU_THRESHOLD` | `0.05f` | g-deviation to count as an earthquake hit |
| `IMU_REQUIRED_HITS` | `3` | Consecutive hits required to fire earthquake alarm |
| `TOUCH_HOLD_MS` | `3000` | Milliseconds to hold touch for SOS |
| `ALARM_DUR_MS` | `5000` | Duration buzzer stays active per alarm event |
| `TELEGRAM_POLL_MS` | `5000` | Interval between Telegram `getUpdates` calls |
| `MIC_COOLDOWN_MS` | `6000` | Minimum time between consecutive mic SOS triggers |
| `SAMPLE_RATE` | `16000` | I²S sample rate in Hz |
| `MIC_BUF_LEN` | `128` | I²S DMA buffer length in samples |

---

## Boot Sequence

```
1. Serial init (115200 baud)
2. ADC resolution → 12-bit, 11dB attenuation
3. GPIO config (buzzer, touch, PIR, radar, flame pins)
4. I²C init → LSM6DSO IMU detection
5. WiFi connect (blocking, 20s timeout → ESP.restart() on failure)
6. WebServer routes registered → server.begin()
7. I²S driver install → INMP441 microphone init
8. Telegram: "SafeNest ONLINE" boot message sent
```

If the IMU is not detected, `en_imu` is set to `false` automatically and the system continues without it.  
If I²S initialization fails, `en_mic` is set to `false` automatically.

---

## Main Loop Flow

```
loop() every ~100ms
│
├── ensureWiFi()           — reconnect if disconnected
├── tickWebServer()        — serve pending HTTP requests
├── tgPoll() [every 5s]   — fetch and handle one Telegram command
│
├── IMU read               — update latestAx/Ay/Az/Deviation
│   └── imuHits >= 3  →  triggerAlarm("EARTHQUAKE")
│
├── Touch read             — update latestTouch
│   └── held >= 3s    →  triggerSOS()
│
├── PIR read               — update latestPir
│   └── HIGH          →  triggerAlarm("PIR MOTION")
│
├── Radar read             — update latestRadar
│   └── rising edge   →  Serial log (Telegram optional)
│
├── Flame ADC read         — update latestFlameL / latestFlameR
│   └── < threshold   →  triggerAlarm("FIRE LEFT/RIGHT")
│
├── handleMic()            — I²S peak amplitude check
│   └── peak > 5000   →  triggerSOS()
│
└── updateBuzzer()         — drive buzzer pin per alarm/SOS state
```

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| WiFi timeout → reboot loop | Wrong SSID/password or weak signal | Verify credentials; move ESP32 closer to router |
| `[IMU] not found — disabled` | Wiring issue or wrong I²C address | Check SDA/SCL connections; verify LSM6DSO address is `0x6A` |
| `[I2S] install FAILED` | Pin conflict or driver already installed | Power cycle the board; verify I²S pin assignments |
| Flame alarm fires continuously | `FLAME_THRESHOLD` too high for ambient IR | Lower `FLAME_THRESHOLD` or shield sensors from sunlight |
| Mic SOS fires on normal speech | `VOICE_THRESHOLD` too low | Uncomment `Serial.println(peak)` in `handleMic()` to calibrate |
| Telegram commands not received | `lastUpdateId` mismatch or network issue | Check bot token and chat ID; restart device |
| `/data` endpoint unreachable | CORS, firewall, or wrong IP | Ensure client and ESP32 are on same network; use IP from Serial |
| Earthquake false alarms | Vibration from appliances | Increase `IMU_THRESHOLD` or `IMU_REQUIRED_HITS` |

---

## Notes

- **TLS Certificate Validation** is disabled (`client.setInsecure()`) for Telegram HTTPS connections to reduce memory usage. This is acceptable for a local IoT device but note the trade-off.
- **Telegram polling** is sequential and blocking for up to 2.5s per call. The `tickWebServer()` call before the poll ensures HTTP clients are not dropped during this window.
- **`temperatureC`** in the `/data` response is reserved as `null`. It can be populated by adding an I²C temperature sensor (e.g., SHT31) and assigning the reading to a `latestTemperatureC` variable.
- The **`lastAlert`** field in `/data` is also updated from `triggerAlarm()` and `triggerSOS()`, making it a reliable last-event indicator for dashboard displays.

---

*SafeNest v5 — Combined Firmware | ESP32 Arduino Core*
