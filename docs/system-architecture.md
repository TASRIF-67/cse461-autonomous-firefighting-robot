# 🧠 System Architecture

FireBot uses a **dual-brain architecture**: an Arduino UNO handles all real-time hardware control, while an ESP32-CAM handles all wireless communication. They communicate over a UART serial link.

---

## Why Dual-Brain?

The Arduino UNO is excellent at fast, deterministic hardware I/O — reading sensors, driving motors, controlling servos. But it has no WiFi.

The ESP32-CAM has WiFi, a camera, and more processing power — but very limited GPIO pins (most are used by the camera).

Splitting responsibilities between them gives FireBot the best of both worlds.

---

## Block Diagram Overview

```
┌─────────────────────────────────────────────────────────────┐
│                      SENSOR INPUTS                          │
│                                                             │
│  Flame Sensor Left ─────┐                                   │
│  Flame Sensor Center ───┤                                   │
│  Flame Sensor Right ────┤                                   │
│  HC-SR04 Ultrasonic ────┼──► Arduino UNO ─────────────────►│──► UART Serial Link ──► ESP32-CAM
│  MQ-2 Smoke & Gas ──────┤         │                        │
│  DHT11 Temp/Hum ────────┤         │                        │
│  LDR Light Sensor ──────┘         │                        │
└───────────────────────────────────┼─────────────────────────┘
                                    │
                    ┌───────────────▼────────────────┐
                    │       ACTUATION & ALERTS        │
                    │                                 │
                    │  SG90 Servo Nozzle              │
                    │  L298N → 4× DC Gear Motors      │
                    │  5V Relay → Water Pump          │
                    │  Piezo Buzzer                   │
                    │  Red & Blue LEDs                │
                    └─────────────────────────────────┘

                    ESP32-CAM
                    ├── OV2640 Camera (FPV stream)
                    ├── Local IoT Web Dashboard (HTTP server)
                    └── Telegram Bot Alerts

┌───────────────────────────────────┐
│         POWER SUPPLY              │
│                                   │
│  18650 Battery Pack               │
│       └── Power Regulator         │
│             ├── 5V → Arduino      │
│             ├── 5V → ESP32-CAM    │
│             ├── 5V → Relay        │
│             └── 12V → L298N       │
└───────────────────────────────────┘
```

---

## Arduino UNO — Responsibilities

The Arduino is the **real-time controller**. It runs the main control loop and is responsible for:

- Reading all sensors every loop iteration
- Obstacle detection and avoidance (HC-SR04)
- Flame direction detection (3× flame sensors)
- Smoke/gas detection (MQ-2)
- Motor control (forward, reverse, turn via L298N)
- Servo nozzle sweep
- Relay/pump control
- Buzzer and LED alerts
- Sending sensor data to ESP32 over UART
- Receiving commands from ESP32 over UART (optional remote control)

---

## ESP32-CAM — Responsibilities

The ESP32-CAM is the **wireless communication layer**. It:

- Connects to the local WiFi network
- Runs an HTTP web server on port 80
- Serves the `/data` JSON endpoint (polled by the dashboard)
- Captures photos with the OV2640 camera on fire detection
- Sends Telegram messages with alert text and/or photo
- Streams live FPV video (optional MJPEG stream)
- Receives sensor data packets from Arduino over UART

---

## UART Communication Protocol

The two boards communicate at **9600 baud** over their hardware serial pins (Arduino TX → ESP32 RX and vice versa).

### Data Format (Arduino → ESP32)

The Arduino sends a comma-delimited string every 500ms (or on state change):

```
T:28.5,H:62.3,D:45.0,F:0,S:0\n
```

| Field | Meaning | Values |
|---|---|---|
| `T` | Temperature (°C) | Float |
| `H` | Humidity (%) | Float |
| `D` | Distance (cm) | Float |
| `F` | Flame detected | 0 = no, 1 = yes |
| `S` | Smoke detected | 0 = no, 1 = yes |
| `\n` | Newline terminates packet | — |

### ESP32 JSON Response (to Dashboard)

The ESP32 parses the UART packet and serves it as JSON at `GET /data`:

```json
{
  "temperature": 28.5,
  "humidity": 62.3,
  "distance": 45.0,
  "button": false
}
```

> `button` maps to the flame/trigger state (`F` field). The dashboard uses this field name for the alert logic.

### Optional: ESP32 → Arduino Commands

If you implement remote control, the ESP32 can send single-character commands back to Arduino:

| Command | Action |
|---|---|
| `F` | Move forward |
| `B` | Move backward |
| `L` | Turn left |
| `R` | Turn right |
| `S` | Stop |
| `P` | Activate pump |

---

## Dashboard API Endpoints

The ESP32 HTTP server exposes these endpoints:

| Endpoint | Method | Response |
|---|---|---|
| `/data` | GET | JSON with all sensor readings |
| `/stream` | GET | MJPEG camera stream (if implemented) |
| `/photo` | GET | Latest captured JPEG image |

The dashboard (`firebot-dashboard.html`) polls `/data` every 1.5 seconds via `fetch()`.

---

## Scalability Notes

If you want to extend this project:

- **Add more sensors** — always add them to the Arduino side (more GPIO, 5V tolerance)
- **Add remote control** — send commands from dashboard → ESP32 → Arduino via UART
- **Add MQTT** — replace the HTTP polling with MQTT pub/sub for lower latency
- **Add SD card logging** — the ESP32-CAM has an SD card slot; log sensor data to CSV
- **OTA updates** — ESP32 supports over-the-air firmware updates via ArduinoOTA

---

Next: [Operational Flowchart →](operational-flowchart.md)
