# FireBot — Testing & Operation Guide

This guide walks through the complete process of testing and operating FireBot, from first-time hardware checks to final deployment. Follow the phases in order — each phase confirms the next one will work.

---

## Prerequisites

Before anything else, make sure you have:

- Arduino IDE installed with the **ESP32 board package** (Espressif) added
- The following libraries installed via Library Manager:
  - `DHT sensor library` — Adafruit
  - `Adafruit Unified Sensor` — Adafruit
  - `UniversalTelegramBot` — Brian Lough
  - `ArduinoJson` — Benoit Blanchon (v6.x)
  - `ESPAsyncWebServer` + `AsyncTCP` — me-no-dev (install as ZIP from GitHub)
- A USB-TTL adapter for flashing the ESP32-CAM
- A Telegram bot created via @BotFather (see `docs/telegram-setup.md`)
- Your `credentials.h` file created in both ESP32 sketch folders (see below)

### credentials.h setup

Create this file in **both** of these locations before flashing anything:

- `esp32/firebot_esp32/credentials.h`
- `esp32/telegram_test/credentials.h`

```cpp
#ifndef CREDENTIALS_H
#define CREDENTIALS_H

const char*  WIFI_SSID  = "YourWiFiName";
const char*  WIFI_PASS  = "YourWiFiPassword";
const String BOT_TOKEN  = "123456789:ABCDefGhIJKlmNoPQRsTUVwxyZ";
const String CHAT_ID    = "987654321";

#endif
```

> These files are gitignored and never committed. If you change your WiFi or bot credentials, update both copies.

---

## Phase 1 — Flame Sensor & Alert Hardware Test

**Sketch:** `arduino/flame_detection/flame_detection.ino`
**Board:** Arduino UNO
**Serial Monitor:** 9600 baud

This test has no motors, no UART, and no ESP32 — it only checks sensors and alert outputs.

### Steps

1. Wire up the flame sensors (LEFT on D2, CENTER on D3, RIGHT on D4), MQ-2 (D7), DHT11 (D8), red LED (A3), blue LED (A4).
2. Open `flame_detection.ino` in Arduino IDE.
3. Select **Tools → Board → Arduino UNO** and the correct COM port.
4. Upload and open Serial Monitor at **9600 baud**.
5. You should immediately see sensor readings printing every 300ms like:

```
FLAME  L: no  C: no  R: no   SMOKE: no   TEMP:27.5C  HUM:61%
```

### What to verify

| Test | How | Expected result |
|---|---|---|
| Flame LEFT | Hold lighter near LEFT sensor | `L:YES` + `FIRE LEFT` printed, buzzer on, red LED on |
| Flame CENTER | Hold lighter near CENTER sensor | `C:YES` + `FIRE AHEAD` printed |
| Flame RIGHT | Hold lighter near RIGHT sensor | `R:YES` + `FIRE RIGHT` printed |
| Smoke (MQ-2) | Blow smoke near MQ-2 | `SMOKE:YES` printed |
| DHT11 | Check reading | Temperature and humidity show realistic values |
| All clear | Remove flame source | All sensors `no`, buzzer off, LEDs off |

> Use a lighter briefly and carefully, or use a TV IR remote as a safe IR source for initial testing.

---

## Phase 2 — Motor & Obstacle Avoidance Test

**Sketch:** `arduino/obstacle_avoidance/obstacle_avoidance.ino`
**Board:** Arduino UNO

This test confirms the L298N wiring and that motor directions (forward, backward, left, right) are correct.

### Steps

1. Place the robot on the floor with enough room to move.
2. Upload `obstacle_avoidance.ino`.
3. Power the robot.

### What to verify

| Test | How | Expected result |
|---|---|---|
| Forward patrol | Power on with clear path | Robot moves forward |
| Obstacle detection | Hold hand in front of HC-SR04 | Robot stops, reverses, turns |
| Turn alternation | Trigger obstacle twice | First turn goes one direction, second goes the other |
| Clear path resume | Remove hand | Robot resumes forward movement |

> If motors spin in the wrong direction, swap the two wires on that motor at the L298N terminal — do not change the code.

---

## Phase 3 — Telegram Bot Test

**Sketch:** `esp32/telegram_test/telegram_test.ino`
**Board:** AI Thinker ESP32-CAM
**Serial Monitor:** 115200 baud

This is the only time you use `telegram_test.ino`. Once credentials are confirmed, you never need it again.

### Flashing the ESP32-CAM

The ESP32-CAM has no onboard USB — you need a USB-TTL adapter:

1. Connect adapter: `TX → U0R`, `RX → U0T`, `GND → GND`, `3.3V → 3.3V`
2. Pull `IO0` to `GND` to enter flash mode
3. Press the reset button on the ESP32-CAM
4. Upload the sketch in Arduino IDE (select **AI Thinker ESP32-CAM** as board)
5. After upload completes, **disconnect IO0 from GND** and press reset again to boot normally

### Steps

1. Make sure `credentials.h` is in `esp32/telegram_test/` with your correct credentials.
2. Flash `telegram_test.ino`.
3. Open Serial Monitor at **115200 baud**.
4. Watch for:

```
[WIFI] Connecting to 'YourWiFiName'......... connected!
[WIFI] IP: 192.168.1.XX
[TELEGRAM] Sending startup test message...
[TELEGRAM] Startup message sent — check your Telegram chat.
```

5. Check your Telegram — you should receive:

```
✅ Telegram test successful!
Bot token and Chat ID are working correctly.
IP: 192.168.1.XX

Send me any message and I'll echo it back.
```

### What to verify

| Test | How | Expected result |
|---|---|---|
| Startup message | Power on | Message arrives in Telegram within 10 seconds |
| Echo | Send any text to your bot | Bot echoes it back |
| /ping | Send `/ping` | Bot replies `🏓 Pong! FireBot is alive.` |
| /ip | Send `/ip` | Bot replies with the ESP32's IP address |

> If the startup message never arrives: double-check `BOT_TOKEN` and `CHAT_ID` in `credentials.h`. Make sure you sent `/start` to your bot in Telegram at least once before testing.

---

## Phase 4 — Full System Integration

Once Phases 1–3 all pass, flash the two final production sketches.

### 4a — Flash Arduino UNO

**Sketch:** `arduino/firebot_main/firebot_main.ino`

1. Open `firebot_main.ino` in Arduino IDE.
2. Select **Arduino UNO** as the board.
3. Upload.
4. Open Serial Monitor at **9600 baud** — you should see:

```
FIREBOT_READY
T:27.5,H:61.0,D:120.5,F:0,S:0
T:27.5,H:61.0,D:119.8,F:0,S:0
```

Telemetry packets printing every 500ms confirms sensors and UART output are working.

### 4b — Flash ESP32-CAM

**Sketch:** `esp32/firebot_esp32/firebot_esp32.ino`

1. Make sure `credentials.h` is in `esp32/firebot_esp32/`.
2. Flash using the USB-TTL adapter (same process as Phase 3).
3. Boot normally (IO0 disconnected, reset pressed).
4. Open Serial Monitor at **9600 baud** — you should see:

```
[WIFI] Connected! IP: 192.168.1.XX
[TELEGRAM] Startup message sent.
```

5. Check Telegram — you should receive:

```
🤖 FireBot online
IP: 192.168.1.XX
```

---

## Phase 5 — End-to-End Fire Response Test

With both boards flashed and the robot fully assembled:

1. Power on the robot. Wait for the Telegram startup message.
2. Open the dashboard in your browser at `http://192.168.1.XX` (use the IP from the startup message).
3. Confirm the dashboard shows live sensor readings updating.
4. Hold a lighter briefly near the CENTER flame sensor.

### Expected sequence

| Step | What happens |
|---|---|
| Flame detected | Robot stops immediately |
| High alert | Buzzer sounds, red/blue LEDs blink |
| Alignment | Robot turns until CENTER sensor is active |
| Suppression | Pump activates, servo sweeps left and right |
| Telegram alert | `🔥 FIRE DETECTED` message + photo arrives |
| Dashboard | Shows `flame: true` and live temp/humidity |
| Fire out | Robot stops pump, servo returns to center |
| Extinguished message | `✅ Fire extinguished` arrives in Telegram |
| Patrol resumes | Robot moves forward again |

---

## Normal Operation

Once tested, normal startup is:

1. Power on Arduino UNO (or shared power supply for both boards).
2. Power on ESP32-CAM.
3. Wait for Telegram startup message — this confirms WiFi and bot are live.
4. Robot begins forward patrol automatically.

You do not need a laptop or Serial Monitor during normal operation. All status is reported through Telegram and the web dashboard.

---

## Quick Troubleshooting Reference

| Symptom | Likely cause | Fix |
|---|---|---|
| No telemetry in Serial Monitor | DHT11 wiring issue | Check DHT11 data pin and pull-up resistor |
| Motors don't move | ENA/ENB not connected | Bridge ENA/ENB jumpers on L298N, or connect to PWM pins |
| Motors spin wrong direction | Motor wires swapped | Swap the two motor wires on the affected side at L298N |
| Robot circles instead of patrolling | One motor reversed | Check IN1/IN2 vs IN3/IN4 wiring against pin map |
| ESP32 won't connect to WiFi | Wrong credentials | Re-check `credentials.h`, confirm 2.4GHz network (not 5GHz) |
| Telegram startup message never arrives | Wrong BOT_TOKEN or CHAT_ID | Re-run Phase 3 to isolate the issue |
| `SSL handshake failed` in Serial | TLS issue | Confirm `secureClient.setInsecure()` is present in sketch |
| Photo not sent, text alert sent instead | Camera init failed | Check camera ribbon cable is fully seated |
| Dashboard shows no data | ESP32 not receiving UART | Check TX/RX cross-connection between UNO and ESP32 |
| Flame sensor never triggers | Sensitivity too low | Adjust the potentiometer on the flame sensor module |
| Robot doesn't stop for obstacles | HC-SR04 wiring | Check TRIG (D5) and ECHO (D6) connections |

---

← Related docs: [Hardware Setup](hardware-setup.md) · [Software Setup](software-setup.md) · [Telegram Setup](telegram-setup.md) · [System Architecture](system-architecture.md)
