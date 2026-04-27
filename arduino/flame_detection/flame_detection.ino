/*
 * ============================================================
 *  FireBot — Flame Detection Test
 *  flame_detection.ino
 * ============================================================
 *  Purpose:
 *    Test all three flame sensors, the MQ-2 smoke sensor,
 *    DHT11 temperature/humidity, and alert outputs (buzzer,
 *    red/blue LEDs) independently before integrating into
 *    the main sketch.
 *
 *  Use this sketch to:
 *    1. Confirm each flame sensor triggers correctly
 *         (use a lighter — carefully — or an IR remote as a test source)
 *    2. Verify LEFT / CENTER / RIGHT direction logic
 *    3. Check MQ-2 responds to smoke
 *    4. Confirm DHT11 reads sensible temperature and humidity
 *    5. Verify buzzer and alert LEDs work
 *
 *  Pins used:
 *    D2  = Flame sensor LEFT  (active LOW)
 *    D3  = Flame sensor CENTER (active LOW)
 *    D4  = Flame sensor RIGHT  (active LOW) / Buzzer
 *    D7  = MQ-2 digital out
 *    D8  = DHT11 data
 *    A3  = Red LED
 *    A4  = Blue LED
 *
 *  Open Serial Monitor at 9600 baud to watch sensor output.
 * ============================================================
 */

#include <DHT.h>

// ── Pin Definitions ─────────────────────────────────────────

#define FLAME_LEFT    2
#define FLAME_CENTER  3
#define FLAME_RIGHT   4   // also used as buzzer pin in this test

#define MQ2_PIN       7
#define DHT_PIN       8
#define DHT_TYPE      DHT11

#define LED_RED       A3
#define LED_BLUE      A4

// Buzzer is driven with tone() on FLAME_RIGHT pin (D4)
// Only do this in isolation tests — in the main sketch
// the buzzer is on the same pin and flame sensor is input.
// In final wiring, separate the buzzer to a dedicated pin.
#define BUZZER_PIN    4

// ── Objects ──────────────────────────────────────────────────

DHT dht(DHT_PIN, DHT_TYPE);

// ── setup() ─────────────────────────────────────────────────

void setup() {
  Serial.begin(9600);

  pinMode(FLAME_LEFT,   INPUT);
  pinMode(FLAME_CENTER, INPUT);
  pinMode(FLAME_RIGHT,  INPUT);
  pinMode(MQ2_PIN,      INPUT);

  pinMode(LED_RED,  OUTPUT);
  pinMode(LED_BLUE, OUTPUT);

  digitalWrite(LED_RED,  LOW);
  digitalWrite(LED_BLUE, LOW);

  dht.begin();
  delay(2000); // let DHT stabilise

  Serial.println("====================================");
  Serial.println("  FireBot — Flame Detection Test");
  Serial.println("====================================");
  Serial.println("Bring a flame or IR source near each");
  Serial.println("sensor to test LEFT / CENTER / RIGHT.");
  Serial.println("====================================");
}

// ── loop() ──────────────────────────────────────────────────

void loop() {

  // ── Read flame sensors (active LOW) ───────────────────
  bool fLeft   = (digitalRead(FLAME_LEFT)   == LOW);
  bool fCenter = (digitalRead(FLAME_CENTER) == LOW);
  bool fRight  = (digitalRead(FLAME_RIGHT)  == LOW);
  bool smoke   = (digitalRead(MQ2_PIN)      == HIGH);

  // ── Read DHT11 ────────────────────────────────────────
  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();

  // ── Print sensor status ───────────────────────────────
  Serial.print("FLAME  L:");
  Serial.print(fLeft   ? "YES" : " no");
  Serial.print("  C:");
  Serial.print(fCenter ? "YES" : " no");
  Serial.print("  R:");
  Serial.print(fRight  ? "YES" : " no");
  Serial.print("   SMOKE:");
  Serial.print(smoke   ? "YES" : " no");

  if (!isnan(temp) && !isnan(hum)) {
    Serial.print("   TEMP:");
    Serial.print(temp, 1);
    Serial.print("C  HUM:");
    Serial.print(hum, 0);
    Serial.print("%");
  } else {
    Serial.print("   DHT: read error");
  }

  // ── Direction interpretation ──────────────────────────
  if (fLeft || fCenter || fRight || smoke) {
    Serial.print("   >> ");
    if      (fCenter)              Serial.print("FIRE AHEAD");
    else if (fLeft  && !fRight)    Serial.print("FIRE LEFT");
    else if (fRight && !fLeft)     Serial.print("FIRE RIGHT");
    else if (fLeft  && fRight)     Serial.print("FIRE BOTH SIDES");
    if      (smoke)                Serial.print(" + SMOKE");

    // Alert outputs
    tone(BUZZER_PIN, 1000);
    digitalWrite(LED_RED,  HIGH);
    digitalWrite(LED_BLUE, LOW);
  } else {
    // All clear
    noTone(BUZZER_PIN);
    digitalWrite(LED_RED,  LOW);
    digitalWrite(LED_BLUE, LOW);
  }

  Serial.println();
  delay(300);
}
