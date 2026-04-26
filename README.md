# Smart Home Safety Android App

Native Android app using Kotlin + Jetpack Compose.

It reads live sensor data directly from ESP32 endpoint:
- `http://<esp32-ip>/data`

Phone and ESP32 must be on the same Wi-Fi.

## Open in Android Studio

1. Open `android-app` as a project.
2. Let Gradle sync.
3. Run on Android device (or emulator).

## First-time setup in app

1. Upload and start `smart_home_safety.ino` on ESP32.
2. In app, set **ESP32 Base URL** to:
   - `http://<esp32-ip>/`
3. Tap **Connect**.

Example:
- `http://192.168.1.50/`

## What app shows

- Direct ESP32 connection state
- Last alert
- IMU acceleration and deviation
- Flame sensor values
- Touch / PIR / Radar states

## Added safety features

- Dark emergency dashboard UI
- SOS hold button (hold for 4 seconds)
- SOS panel with 50-second auto escalation timer
- Auto action via configured method (SMS or call)
- Optional Telegram emergency message integration
- Threshold-based emergency alerts
- Weekly activity log with day filter

## Configure SOS and alerts

In the app `Settings` tab, configure:

- Emergency contacts (comma separated phone numbers)
- Notification method (SMS or Call)
- Temperature/IMU/Flame thresholds
- Telegram toggle + bot token + chat id
