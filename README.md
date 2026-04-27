# 🤖 FireBot — Autonomous Firefighting Robot

> **BRAC University Robotics Project**
> A dual-brain robot that patrols, detects flames, and autonomously extinguishes fires using a water pump — with live IoT dashboard and Telegram alerts.

---

## 📸 System Overview

FireBot uses an **Arduino UNO** as the primary controller (sensors + motors + actuators) and an **ESP32-CAM** as the secondary brain (WiFi, live dashboard, Telegram notifications, camera stream).

```
Sensors → Arduino UNO → Actuators (motors, pump, LEDs, buzzer)
                ↕  UART Serial
            ESP32-CAM → Web Dashboard + Telegram Bot + OV2640 Camera
```

---

## 📁 Repository Structure

```
firebot/
│
├── README.md                        ← You are here
│
├── arduino/
│   ├── firebot_main/
│   │   └── firebot_main.ino         ← Main Arduino sketch (all sensors + logic)
│   ├── obstacle_avoidance/
│   │   └── obstacle_avoidance.ino   ← Standalone obstacle avoidance test
│   ├── flame_detection/
│   │   └── flame_detection.ino      ← Standalone flame sensor test
│   └── motor_test/
│       └── motor_test.ino           ← Motor driver test sketch
│
├── esp32/
│   ├── firebot_esp32/
│   │   └── firebot_esp32.ino        ← ESP32-CAM: WiFi server + Telegram + UART
│   └── telegram_test/
│       └── telegram_test.ino        ← Standalone Telegram bot test
│
├── dashboard/
│   └── firebot-dashboard.html       ← Local IoT web dashboard (open in browser)
│
├── docs/
│   ├── hardware-setup.md            ← Wiring guide, pin mapping, components list
│   ├── software-setup.md            ← Arduino IDE, libraries, flashing instructions
│   ├── system-architecture.md       ← Block diagram explanation + UART protocol
│   ├── operational-flowchart.md     ← Step-by-step logic walkthrough
│   └── telegram-setup.md            ← Creating a Telegram bot + getting chat ID
│
├── diagrams/
│   ├── system-block-diagram.png     ← System block diagram image
│   └── operational-flowchart.png    ← Operational flowchart image
│
└── .gitignore
```

---

## ⚙️ Hardware Components

| Component | Role |
|---|---|
| Arduino UNO | Primary controller — reads sensors, drives motors, controls pump |
| ESP32-CAM | Secondary brain — WiFi server, Telegram alerts, FPV stream |
| Flame Sensor × 3 (Left, Center, Right) | Fire direction detection |
| HC-SR04 Ultrasonic | Obstacle distance measurement |
| MQ-2 Smoke & Gas Sensor | Smoke/gas early warning |
| DHT11 (optional) | Temperature & humidity |
| LDR + White LED (optional) | Auto headlight in dark environments |
| L298N Motor Driver | Controls 4 DC gear motors |
| 4× DC Gear Motors | Robot movement |
| 5V Relay Module | Switches water pump on/off |
| Mini Submersible Water Pump | Fire extinguishing |
| SG90 Servo | Sweeps water nozzle |
| Piezo Buzzer | Alert siren |
| Red + Blue LEDs | Emergency light effect |
| OV2640 Camera (on ESP32-CAM) | Live FPV video stream |
| 18650 Battery Pack | Power supply |
| Power Supply Regulator | Stable 5V/12V for all components |

---

## 🚀 Quick Start

1. **Wire the hardware** — see [`docs/hardware-setup.md`](docs/hardware-setup.md)
2. **Install libraries** — see [`docs/software-setup.md`](docs/software-setup.md)
3. **Flash Arduino** — upload `arduino/firebot_main/firebot_main.ino`
4. **Flash ESP32-CAM** — upload `esp32/firebot_esp32/firebot_esp32.ino` with your WiFi credentials
5. **Open dashboard** — open `dashboard/firebot-dashboard.html` in a browser, enter the ESP32 IP
6. **Set up Telegram** — see [`docs/telegram-setup.md`](docs/telegram-setup.md)

---

## 📡 How It Works

FireBot continuously patrols in a loop. When it encounters an **obstacle**, it reverses and turns. When it detects **smoke or flame**, it enters **High Alert Mode**: stops movement, activates buzzer and LEDs, captures a photo, sends a Telegram alert, scans for the flame direction with three sensors, aligns itself, then activates the water pump and sweeps the nozzle until the flame is gone.

Full logic: [`docs/operational-flowchart.md`](docs/operational-flowchart.md)

---

## 📄 Documentation Index

| File | What it covers |
|---|---|
| [`docs/hardware-setup.md`](docs/hardware-setup.md) | Pin wiring, circuit connections, power setup |
| [`docs/software-setup.md`](docs/software-setup.md) | IDE setup, libraries, upload instructions |
| [`docs/system-architecture.md`](docs/system-architecture.md) | Dual-brain architecture, UART protocol |
| [`docs/operational-flowchart.md`](docs/operational-flowchart.md) | Complete robot logic walkthrough |
| [`docs/telegram-setup.md`](docs/telegram-setup.md) | Telegram bot creation and configuration |

---

## 🎓 Academic Info

**Institution:** BRAC University
**Project Type:** Undergraduate Robotics / Embedded Systems
**Tools:** Arduino IDE, C/C++, HTML/CSS/JavaScript, Chart.js

---

## 📜 License

This project is open-source for educational purposes.
