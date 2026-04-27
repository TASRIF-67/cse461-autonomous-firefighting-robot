# 💻 Software Setup Guide

Everything you need to install, configure, and upload code to both the **Arduino UNO** and **ESP32-CAM**.

---

## 1. Install Arduino IDE

Download and install from: https://www.arduino.cc/en/software
Recommended: **Arduino IDE 2.x**

---

## 2. Add ESP32 Board Support

The ESP32-CAM requires the Espressif board package.

1. Open Arduino IDE → **File → Preferences**
2. In *Additional Boards Manager URLs*, add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Go to **Tools → Board → Boards Manager**
4. Search for `esp32` by Espressif Systems → **Install**

---

## 3. Install Required Libraries

Go to **Tools → Manage Libraries** and install each of these:

### For Arduino UNO

| Library | Author | Purpose |
|---|---|---|
| `DHT sensor library` | Adafruit | DHT11/DHT22 temperature & humidity |
| `Adafruit Unified Sensor` | Adafruit | Required by DHT library |
| `Servo` | Arduino | SG90 nozzle servo control |

> The `NewPing` library (by Tim Eckel) is optional for cleaner HC-SR04 code. Search `NewPing` in Library Manager.

### For ESP32-CAM

| Library | Author | Purpose |
|---|---|---|
| `AsyncTCP` | me-no-dev | Required for async web server |
| `ESPAsyncWebServer` | me-no-dev | Web server for dashboard API |
| `UniversalTelegramBot` | Brian Lough | Telegram bot messages |
| `ArduinoJson` | Benoit Blanchon | JSON parsing for Telegram & dashboard |

> `AsyncTCP` and `ESPAsyncWebServer` are **not** in the standard Library Manager. Install from GitHub:
> - https://github.com/me-no-dev/AsyncTCP
> - https://github.com/me-no-dev/ESPAsyncWebServer
>
> Download as ZIP → Arduino IDE → **Sketch → Include Library → Add .ZIP Library**

---

## 4. Upload Code to Arduino UNO

1. Connect Arduino UNO to your computer via USB
2. **Disconnect the UART wires** between Arduino and ESP32-CAM (TX/RX pins) — if left connected, the upload will fail
3. Open `arduino/firebot_main/firebot_main.ino`
4. Select board: **Tools → Board → Arduino UNO**
5. Select port: **Tools → Port → COMx** (Windows) or `/dev/ttyUSBx` (Linux/Mac)
6. Click **Upload** (→ arrow button)
7. Open Serial Monitor at **9600 baud** to verify sensor readings

---

## 5. Upload Code to ESP32-CAM

The ESP32-CAM has no USB port. You need a **USB-to-TTL adapter** (FTDI or CH340).

### Wiring for flashing

| USB-TTL | ESP32-CAM | Notes |
|---|---|---|
| 5V | 5V | Power during flash |
| GND | GND | Common ground |
| TX | GPIO3 (U0RXD) | Adapter TX → ESP32 RX |
| RX | GPIO1 (U0TXD) | Adapter RX → ESP32 TX |
| GND | IO0 | **SHORT IO0 to GND to enter flash mode** |

> The `IO0 → GND` short is mandatory. This puts the ESP32-CAM into download/flash mode. Remove the short after flashing.

### Steps

1. Short IO0 to GND
2. Power the ESP32-CAM (connect USB-TTL to computer)
3. Open `esp32/firebot_esp32/firebot_esp32.ino`
4. Update your WiFi credentials and Telegram token in the sketch
5. Select board: **Tools → Board → AI Thinker ESP32-CAM**
6. Select port matching your USB-TTL adapter
7. Set **Upload Speed: 115200**
8. Click Upload
9. When you see `Connecting....___....` — **press the RESET button** on the ESP32-CAM once
10. Wait for "Hard resetting via RTS pin..." — upload complete
11. **Remove the IO0 → GND short**
12. Press RESET again to boot normally
13. Open Serial Monitor at **115200 baud** — the IP address will be printed

---

## 6. Configure WiFi & Telegram Credentials

In `esp32/firebot_esp32/firebot_esp32.ino`, find and update:

```cpp
const char* SSID       = "YourWiFiName";
const char* PASSWORD   = "YourWiFiPassword";
const String BOT_TOKEN = "YOUR_TELEGRAM_BOT_TOKEN";
const String CHAT_ID   = "YOUR_TELEGRAM_CHAT_ID";
```

See [`telegram-setup.md`](telegram-setup.md) for how to get your bot token and chat ID.

---

## 7. Open the Dashboard

1. Find the ESP32's IP address from Serial Monitor output, e.g. `192.168.1.42`
2. Open `dashboard/firebot-dashboard.html` in any browser on the **same WiFi network**
3. Enter the IP address in the "ESP32 IP" field
4. Click **Connect**

The dashboard will now show live sensor readings every 1.5 seconds.

---

## 8. Verify Everything Works

Go through this checklist in order:

- [ ] Arduino Serial Monitor shows temperature, humidity, distance values
- [ ] Flame sensors return LOW when a lighter is nearby
- [ ] Ultrasonic sensor shows correct distance in cm
- [ ] Motors spin correctly (test each direction separately first)
- [ ] Servo sweeps properly
- [ ] Relay clicks and pump activates
- [ ] Buzzer sounds
- [ ] LEDs blink
- [ ] ESP32-CAM connects to WiFi and prints IP
- [ ] Dashboard shows live data
- [ ] Telegram receives a test message

---

## Troubleshooting

| Problem | Likely Cause | Fix |
|---|---|---|
| Arduino upload fails | UART wires still connected | Disconnect TX/RX before upload |
| ESP32 upload stuck at `Connecting...` | IO0 not grounded, or missed RESET timing | Re-short IO0, retry, press RESET when prompted |
| Dashboard shows "Offline" | Wrong IP, different WiFi network, or CORS issue | Verify IP in Serial Monitor; ensure browser and ESP32 on same network |
| DHT sensor reads `nan` | Missing pull-up resistor, wrong pin | Add 10kΩ between DATA and 5V |
| Motors spin wrong direction | Motor wires swapped | Swap OUT1/OUT2 or reverse IN1/IN2 in code |
| Pump doesn't activate | Relay wiring or relay is NC not NO | Check COM/NO/NC — use NO terminal |

---

Next: [System Architecture →](system-architecture.md)
