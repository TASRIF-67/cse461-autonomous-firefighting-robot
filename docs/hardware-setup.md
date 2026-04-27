# 🔌 Hardware Setup & Wiring Guide

This document covers every pin connection for both the **Arduino UNO** and **ESP32-CAM**, the power wiring, and component notes.

---

## 🗂️ Pin Map — Arduino UNO

### Flame Sensors (Digital, Active LOW)

| Sensor | Arduino Pin |
|---|---|
| Flame Sensor Left (DO) | D2 |
| Flame Sensor Center (DO) | D3 |
| Flame Sensor Right (DO) | D4 |

> All three flame sensors share `5V` and `GND`. The DO (digital output) pin goes LOW when flame is detected.

---

### HC-SR04 Ultrasonic Sensor

| HC-SR04 Pin | Arduino Pin |
|---|---|
| VCC | 5V |
| GND | GND |
| TRIG | D5 |
| ECHO | D6 |

> **Note:** The HC-SR04 runs on 5V. ECHO outputs 5V — safe for Arduino UNO directly, but use a voltage divider if connecting to ESP32 (3.3V tolerant).

---

### MQ-2 Smoke & Gas Sensor

| MQ-2 Pin | Arduino Pin |
|---|---|
| VCC | 5V |
| GND | GND |
| DO (Digital Out) | D7 |
| AO (Analog Out) | A0 (optional) |

> Allow a 2-minute warm-up after powering the MQ-2 before readings are reliable.

---

### DHT11 Temperature & Humidity (Optional)

| DHT11 Pin | Arduino Pin |
|---|---|
| VCC | 5V |
| GND | GND |
| DATA | D8 |

> Add a 10kΩ pull-up resistor between DATA and VCC.

---

### LDR + White Headlight LED (Optional)

| Component | Arduino Pin |
|---|---|
| LDR (voltage divider mid-point) | A1 |
| White LED (through 220Ω resistor) | D9 |

---

### L298N Motor Driver

| L298N Pin | Arduino Pin | Notes |
|---|---|---|
| IN1 | D10 | Left motors direction |
| IN2 | D11 | Left motors direction |
| IN3 | D12 | Right motors direction |
| IN4 | D13 | Right motors direction |
| ENA | (tie HIGH or PWM pin) | Left motors enable |
| ENB | (tie HIGH or PWM pin) | Right motors enable |
| 12V | Battery (+) | Motor power |
| GND | Battery (-) + Arduino GND | Common ground |
| 5V OUT | — | Can power Arduino if regulator is used |

> Connect Motor A (left pair) to OUT1/OUT2 and Motor B (right pair) to OUT3/OUT4.

---

### SG90 Servo (Nozzle)

| Servo Wire | Arduino Pin |
|---|---|
| Red (VCC) | 5V |
| Brown (GND) | GND |
| Orange (Signal) | D3 (PWM) |

> Use `Servo.h` library. Center position (90°) points forward. Sweep ±30° during firefighting.

---

### 5V Relay Module (Water Pump)

| Relay Pin | Arduino Pin / Power |
|---|---|
| VCC | 5V |
| GND | GND |
| IN (signal) | A2 |
| COM | Pump + wire |
| NO (normally open) | Battery (+) for pump |

> The pump's negative wire goes directly to battery GND.

---

### Piezo Buzzer

| Buzzer Pin | Arduino Pin |
|---|---|
| + | D4 |
| − | GND |

---

### Red & Blue LEDs

| LED | Arduino Pin | Resistor |
|---|---|---|
| Red LED | A3 | 220Ω |
| Blue LED | A4 | 220Ω |

---

### UART to ESP32-CAM

| Arduino Pin | ESP32-CAM Pin | Notes |
|---|---|---|
| TX (D1) | GPIO3 (U0RXD) | Arduino TX → ESP32 RX |
| RX (D0) | GPIO1 (U0TXD) | Arduino RX → ESP32 TX |
| GND | GND | Common ground — REQUIRED |

> **Important:** Arduino UNO is 5V, ESP32 is 3.3V. Use a **voltage divider** on the Arduino TX → ESP32 RX line (e.g., 1kΩ + 2kΩ). The ESP32 TX → Arduino RX is fine directly.
>
> Disconnect the UART wires during Arduino flashing or it will fail.

---

## 🗂️ Pin Map — ESP32-CAM

| Function | ESP32-CAM GPIO |
|---|---|
| UART RX (from Arduino TX) | GPIO3 |
| UART TX (to Arduino RX) | GPIO1 |
| Camera (built-in) | Various (OV2640, managed by library) |
| Flash LED (built-in) | GPIO4 |

> The ESP32-CAM has very limited GPIO. Do not try to add external sensors to it — keep all sensing on the Arduino side.

---

## ⚡ Power Wiring

```
18650 Battery Pack
       │
       ▼
Power Supply Regulator
  ├── 5V out → Arduino 5V pin
  ├── 5V out → ESP32-CAM 5V pin
  ├── 5V out → Relay module VCC
  ├── 12V out (if applicable) → L298N motor driver 12V
  └── GND → Common ground rail (all components share this)
```

> **Never power the ESP32-CAM from the Arduino's 5V pin.** The camera module draws up to 300mA during WiFi transmission — this will brown out the Arduino. Use a dedicated regulator output.

---

## 🔍 Component Checklist

Before powering on, verify:

- [ ] Common GND between Arduino, ESP32-CAM, L298N, relay, and battery
- [ ] Voltage divider on Arduino TX → ESP32 RX line
- [ ] UART wires disconnected during Arduino flashing
- [ ] MQ-2 allowed to warm up before testing
- [ ] DHT11 pull-up resistor in place (if used)
- [ ] All motor wires secured (loose wires cause erratic behavior)
- [ ] Pump tubing pointed into a container during bench testing
- [ ] Power supply regulator rated for total current draw

---

## 📐 Suggested Physical Layout

```
Front of robot:
  [ Flame L ]  [ Flame C ]  [ Flame R ]
       [ HC-SR04 Ultrasonic ]

  Left Motors          Right Motors

  [ Water Pump + Nozzle (servo-mounted, center) ]

Top of chassis:
  [ Arduino UNO ]  [ ESP32-CAM (elevated for clear camera view) ]
  [ L298N ]  [ Relay ]  [ Battery + Regulator ]
  [ Buzzer ]  [ Red LED ]  [ Blue LED ]
```

---

Next: [Software Setup →](software-setup.md)
