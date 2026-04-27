// ============================================================
//  telegram_test.ino
//  FireBot — Standalone Telegram Bot Test
//
//  Purpose:
//    Verify that your Bot Token, Chat ID, and WiFi credentials
//    are correct BEFORE flashing the full firebot_esp32.ino.
//    This sketch sends a single test message on boot, then
//    echoes back any message you send to the bot.
//
//  This sketch has NO dependencies on:
//    - Camera / OV2640
//    - Arduino UART / sensor data
//    - ESPAsyncWebServer
//
//  Board:    AI Thinker ESP32-CAM  (or any ESP32 board)
//  Libraries required:
//    - UniversalTelegramBot  (Brian Lough — Library Manager)
//    - ArduinoJson           (Benoit Blanchon — Library Manager)
// ============================================================

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// ════════════════════════════════════════════════════════════
//  CONFIG — Fill these in, then upload
// ════════════════════════════════════════════════════════════

const char*  WIFI_SSID  = "YourWiFiName";
const char*  WIFI_PASS  = "YourWiFiPassword";
const String BOT_TOKEN  = "123456789:ABCDefGhIJKlmNoPQRsTUVwxyZ";
const String CHAT_ID    = "987654321";   // negative number for group chats

// ════════════════════════════════════════════════════════════
//  TUNING
// ════════════════════════════════════════════════════════════

// How often to poll Telegram for new incoming messages (ms).
// Keep above 1000 ms — Telegram rate-limits faster polling.
#define POLL_INTERVAL_MS  2000

// WiFi connection timeout (ms)
#define WIFI_TIMEOUT_MS  15000

// ════════════════════════════════════════════════════════════
//  GLOBALS
// ════════════════════════════════════════════════════════════

WiFiClientSecure     secureClient;
UniversalTelegramBot bot(BOT_TOKEN, secureClient);

unsigned long lastPollTime = 0;

// ════════════════════════════════════════════════════════════
//  setup()
// ════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== FireBot — Telegram Test ===");

  // ── Connect to WiFi ──────────────────────────────────────
  connectWiFi();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ERROR] WiFi connection failed. Check SSID and password.");
    Serial.println("        Halting. Press reset to try again.");
    while (true) delay(1000);
  }

  // ── Skip TLS certificate verification ───────────────────
  //    Fine for testing. For production, supply the Telegram
  //    root CA certificate to secureClient instead.
  secureClient.setInsecure();

  // ── Send startup test message ────────────────────────────
  Serial.println("[TELEGRAM] Sending startup test message...");

  String msg  = "✅ *Telegram test successful!*\n\n";
         msg += "Bot token and Chat ID are working correctly.\n";
         msg += "IP: `" + WiFi.localIP().toString() + "`\n\n";
         msg += "Send me any message and I'll echo it back.";

  bool ok = bot.sendMessage(CHAT_ID, msg, "Markdown");

  if (ok) {
    Serial.println("[TELEGRAM] Startup message sent — check your Telegram chat.");
  } else {
    Serial.println("[TELEGRAM] Send FAILED.");
    Serial.println("           Double-check BOT_TOKEN and CHAT_ID.");
    Serial.println("           Also make sure you sent /start to the bot first.");
  }

  Serial.println("\n[INFO] Polling for incoming messages every "
                 + String(POLL_INTERVAL_MS) + " ms...");
  Serial.println("[INFO] Send any message to your bot to test the echo.\n");
}

// ════════════════════════════════════════════════════════════
//  loop()
// ════════════════════════════════════════════════════════════

void loop() {

  // ── Poll Telegram for new messages on interval ───────────
  if (millis() - lastPollTime >= POLL_INTERVAL_MS) {
    lastPollTime = millis();

    int numMessages = bot.getUpdates(bot.last_message_received + 1);

    // Process all queued messages in one pass
    while (numMessages > 0) {
      Serial.printf("[TELEGRAM] %d new message(s) received.\n", numMessages);

      for (int i = 0; i < numMessages; i++) {
        handleIncomingMessage(i);
      }

      // Check if further messages are queued
      numMessages = bot.getUpdates(bot.last_message_received + 1);
    }
  }

  // ── WiFi watchdog — print warning if connection drops ────
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Connection lost. Attempting reconnect...");
    WiFi.reconnect();
    delay(3000);
  }
}

// ════════════════════════════════════════════════════════════
//  handleIncomingMessage()
//  Reads message i from the bot buffer and echoes it back.
// ════════════════════════════════════════════════════════════

void handleIncomingMessage(int i) {
  String senderName = bot.messages[i].from_name;
  String text       = bot.messages[i].text;
  String fromChatId = bot.messages[i].chat_id;

  Serial.println("[MSG] From: " + senderName + " | Text: " + text);

  // ── /start command ───────────────────────────────────────
  if (text == "/start") {
    String welcome  = "👋 Hello, " + senderName + "!\n\n";
           welcome += "This is the FireBot Telegram test sketch.\n";
           welcome += "Send any message and I'll echo it back.\n\n";
           welcome += "*Available commands:*\n";
           welcome += "/start — show this message\n";
           welcome += "/ip    — show current IP address\n";
           welcome += "/ping  — check bot is alive";
    bot.sendMessage(fromChatId, welcome, "Markdown");
    return;
  }

  // ── /ip command ─────────────────────────────────────────
  if (text == "/ip") {
    bot.sendMessage(fromChatId,
      "📡 IP address: `" + WiFi.localIP().toString() + "`", "Markdown");
    return;
  }

  // ── /ping command ────────────────────────────────────────
  if (text == "/ping") {
    bot.sendMessage(fromChatId, "🏓 Pong! FireBot is alive.", "");
    return;
  }

  // ── Default: echo back whatever was sent ─────────────────
  String echo  = "🔁 Echo from FireBot:\n_" + text + "_";
  bot.sendMessage(fromChatId, echo, "Markdown");
}

// ════════════════════════════════════════════════════════════
//  connectWiFi()
//  Blocks until connected or timeout is reached.
// ════════════════════════════════════════════════════════════

void connectWiFi() {
  Serial.printf("[WIFI] Connecting to '%s'", WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_TIMEOUT_MS) {
      Serial.println(" timed out.");
      return;
    }
    delay(500);
    Serial.print(".");
  }

  Serial.println(" connected!");
  Serial.println("[WIFI] IP: " + WiFi.localIP().toString());
}
