# 📱 Telegram Bot Setup

FireBot uses a Telegram bot to send real-time alerts when a fire is detected — including a photo from the ESP32-CAM. This guide walks through creating the bot and getting the credentials you need.

---

## Step 1 — Create a Telegram Bot

1. Open Telegram and search for **@BotFather**
2. Start a chat and send: `/newbot`
3. BotFather will ask for a **name** (display name, e.g. `FireBot Alert`)
4. Then a **username** (must end in `bot`, e.g. `firebot_alert_bot`)
5. BotFather will reply with your **Bot Token** — save it, it looks like:
   ```
   123456789:ABCDefGhIJKlmNoPQRsTUVwxyZ
   ```

> Keep your bot token secret. Anyone with this token can control your bot.

---

## Step 2 — Get Your Chat ID

The bot needs to know **where to send messages** — your personal chat ID or a group chat ID.

### Option A — Personal Chat ID

1. Search for your bot in Telegram (by its username) and send it any message, e.g. `/start`
2. Open this URL in your browser (replace `YOUR_TOKEN`):
   ```
   https://api.telegram.org/botYOUR_TOKEN/getUpdates
   ```
3. You'll see a JSON response. Find the `"chat"` object and copy the `"id"` value:
   ```json
   "chat": {
     "id": 987654321,
     "type": "private"
   }
   ```
4. Your Chat ID is `987654321`

### Option B — Group Chat ID

1. Create a Telegram group
2. Add your bot to the group
3. Send a message in the group
4. Open the same `getUpdates` URL above
5. The `"id"` under `"chat"` will be a **negative number** for groups, e.g. `-1001234567890`

---

## Step 3 — Update the ESP32 Sketch

Open `esp32/firebot_esp32/firebot_esp32.ino` and update these two lines:

```cpp
const String BOT_TOKEN = "123456789:ABCDefGhIJKlmNoPQRsTUVwxyZ";
const String CHAT_ID   = "987654321";
```

---

## Step 4 — Test the Bot

After uploading the ESP32 sketch, power on the robot and check that:

1. The ESP32 connects to WiFi (confirmed by IP printed in Serial Monitor)
2. A startup message arrives in Telegram:
   ```
   🤖 FireBot online. IP: 192.168.1.42
   ```
3. When a flame sensor is triggered, an alert message arrives:
   ```
   🔥 FIRE DETECTED — FireBot is responding!
   Location: [timestamp]
   Temp: 29.3°C | Humidity: 58%
   ```
4. A photo captured by the OV2640 arrives alongside the message

---

## How Telegram Alerts Work in Code

The ESP32 uses the `UniversalTelegramBot` library. The key calls are:

```cpp
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// Send a text alert
bot.sendMessage(CHAT_ID, "🔥 FIRE DETECTED!", "");

// Send a photo (after capturing with camera)
bot.sendPhoto(CHAT_ID, photoBuffer, photoSize, "fire_alert.jpg", "image/jpeg");
```

> The `WiFiClientSecure` is required because Telegram's API uses HTTPS. Make sure to set `client.setInsecure()` for testing, or use the proper certificate for production.

---

## Notification Types

You can customize which events trigger a Telegram message. Suggested events:

| Event | Message |
|---|---|
| Robot powers on | `🤖 FireBot online. IP: [ip]` |
| Flame detected | `🔥 FIRE DETECTED — responding!` + photo |
| Fire extinguished | `✅ Fire extinguished. Resuming patrol.` |
| WiFi reconnected | `📶 WiFi reconnected.` |
| Low battery (if sensor added) | `🔋 Low battery warning.` |

---

## Troubleshooting

| Problem | Fix |
|---|---|
| `getUpdates` returns empty `result` array | Bot hasn't received a message yet — send `/start` to it first |
| No messages received on ESP32 | Check token and chat ID are correct in sketch; verify WiFi connection |
| `SSL handshake failed` | Add `client.setInsecure()` before bot calls |
| Photos not sending | Ensure camera initialization succeeds before calling `sendPhoto` |

---

← Back to: [Software Setup](software-setup.md)
