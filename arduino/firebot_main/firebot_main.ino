/*
 * ============================================================
 *  FireBot — Main Arduino Sketch
 *  firebot_main.ino
 * ============================================================
 *  Hardware: Arduino UNO
 *
 *  This sketch is the primary brain of FireBot. It handles:
 *    - Continuous sensor reading (DHT11, HC-SR04, MQ-2, flame x3, LDR)
 *    - Obstacle detection and avoidance
 *    - Smoke / flame detection and high alert mode
 *    - Flame direction scanning and robot alignment
 *    - Water pump activation and servo nozzle sweep
 *    - Sending telemetry to ESP32-CAM over UART
 *
 *  Operational flow:
 *    Initialize → Read sensors → Obstacle? → Flame/Smoke? →
 *    High Alert → Align → Suppress → Verify → Resume patrol
 *
 *  Libraries required:
 *    - DHT sensor library (Adafruit)
 *    - Adafruit Unified Sensor (Adafruit)
 *    - Servo (built-in)
 *
 *  Pin map (matches hardware-setup.md):
 *    D2  = Flame sensor LEFT  (active LOW)
 *    D3  = Flame sensor CENTER (active LOW)  ← also Servo signal
 *    D4  = Flame sensor RIGHT  (active LOW)  ← also Buzzer
 *    D5  = HC-SR04 TRIG
 *    D6  = HC-SR04 ECHO
 *    D7  = MQ-2 digital out
 *    D8  = DHT11 data
 *    D9  = White headlight LED
 *    D10 = L298N IN1 (left motors)
 *    D11 = L298N IN2 (left motors)
 *    D12 = L298N IN3 (right motors)
 *    D13 = L298N IN4 (right motors)
 *    A0  = MQ-2 analog (optional, for raw value)
 *    A1  = LDR (voltage divider)
 *    A2  = Relay IN (pump)
 *    A3  = Red LED
 *    A4  = Blue LED
 *
 *  NOTE: D3 is shared between Servo signal and Flame Center pin
 *  in this default mapping. If conflicts arise, move Servo to
 *  another PWM pin (D9) and shift headlight LED elsewhere.
 * ============================================================
 */

#include <DHT.h>
#include <Servo.h>

// ── Pin Definitions ─────────────────────────────────────────

// Flame sensors (active LOW = flame detected)
#define FLAME_LEFT    2
#define FLAME_CENTER  3
#define FLAME_RIGHT   4

// Ultrasonic sensor HC-SR04
#define TRIG_PIN      5
#define ECHO_PIN      6

// MQ-2 smoke/gas sensor
#define MQ2_DO_PIN    7
#define MQ2_AO_PIN    A0   // optional analog reading

// DHT11 temperature & humidity
#define DHT_PIN       8
#define DHT_TYPE      DHT11

// Lighting
#define HEADLIGHT_PIN 9    // white LED for dark environments

// L298N motor driver
#define IN1           10   // left  motors direction A
#define IN2           11   // left  motors direction B
#define IN3           12   // right motors direction A
#define IN4           13   // right motors direction B

// LDR light sensor (analog, voltage divider)
#define LDR_PIN       A1

// Relay (controls water pump) — active LOW on most relay modules
#define RELAY_PIN     A2

// Alert LEDs
#define LED_RED       A3
#define LED_BLUE      A4

// Servo for nozzle sweep — reassign if D3 conflicts
#define SERVO_PIN     3

// ── Tunable Parameters ──────────────────────────────────────

#define OBSTACLE_THRESHOLD_CM   20    // stop and avoid if closer than this
#define LDR_DARK_THRESHOLD      400   // analog value below = dark (tune for your LDR)
#define PATROL_SPEED            150   // motor PWM 0-255 during patrol
#define TURN_SPEED              130   // motor PWM during turns
#define SERVO_CENTER            90    // nozzle home position (degrees)
#define SERVO_SWEEP_MIN         60    // nozzle left limit
#define SERVO_SWEEP_MAX         120   // nozzle right limit
#define SERVO_SWEEP_STEP        2     // degrees per sweep step
#define SERVO_SWEEP_DELAY_MS    15    // ms between steps (smaller = faster sweep)
#define ALIGN_TURN_DURATION_MS  150   // how long to turn per alignment step
#define REVERSE_DURATION_MS     300   // how long to reverse on obstacle
#define TURN_DURATION_MS        400   // how long to turn on obstacle
#define UART_INTERVAL_MS        500   // telemetry send interval to ESP32
#define LED_BLINK_INTERVAL_MS   200   // alert LED blink speed

// ── Global Objects ───────────────────────────────────────────

DHT   dht(DHT_PIN, DHT_TYPE);
Servo nozzleServo;

// ── Global State ─────────────────────────────────────────────

float   temperature     = 0.0;
float   humidity        = 0.0;
float   distanceCm      = 0.0;
bool    flameLeft       = false;
bool    flameCenter     = false;
bool    flameRight      = false;
bool    smokeDetected   = false;
bool    isDark          = false;
bool    alertActive     = false;

// Timing
unsigned long lastUartSend      = 0;
unsigned long lastLedBlink      = 0;
bool          ledBlinkState     = false;

// Turn direction alternates each obstacle to avoid looping
bool turnLeftNext = true;

// ── setup() ─────────────────────────────────────────────────

void setup() {
  // UART to ESP32-CAM at 9600 baud
  Serial.begin(9600);

  // Sensor pins
  pinMode(FLAME_LEFT,   INPUT);
  pinMode(FLAME_CENTER, INPUT);
  pinMode(FLAME_RIGHT,  INPUT);
  pinMode(TRIG_PIN,     OUTPUT);
  pinMode(ECHO_PIN,     INPUT);
  pinMode(MQ2_DO_PIN,   INPUT);
  pinMode(LDR_PIN,      INPUT);

  // Output pins
  pinMode(IN1,          OUTPUT);
  pinMode(IN2,          OUTPUT);
  pinMode(IN3,          OUTPUT);
  pinMode(IN4,          OUTPUT);
  pinMode(RELAY_PIN,    OUTPUT);
  pinMode(LED_RED,      OUTPUT);
  pinMode(LED_BLUE,     OUTPUT);
  pinMode(HEADLIGHT_PIN,OUTPUT);

  // Safe initial states
  stopMotors();
  digitalWrite(RELAY_PIN,     HIGH);  // relay off (active LOW module)
  digitalWrite(LED_RED,       LOW);
  digitalWrite(LED_BLUE,      LOW);
  digitalWrite(HEADLIGHT_PIN, LOW);

  // Servo — attach and move to center/home
  nozzleServo.attach(SERVO_PIN);
  nozzleServo.write(SERVO_CENTER);

  // DHT sensor startup
  dht.begin();

  // Short pause for sensors to stabilise, then begin patrol
  delay(2000);
  moveForward(PATROL_SPEED);

  Serial.println("FIREBOT_READY");
}

// ── loop() ──────────────────────────────────────────────────
/*
 *  Main control loop mirrors the operational flowchart:
 *
 *  1. Read all sensors
 *  2. Send telemetry to ESP32 (every UART_INTERVAL_MS)
 *  3. Obstacle? → avoid and restart loop
 *  4. Flame or smoke? → enter High Alert
 *     4a. Align to flame
 *     4b. Suppress until extinguished
 *     4c. Resume patrol
 *  5. Otherwise → check darkness, keep patrolling
 */
void loop() {

  // ── Phase 2: Read sensors ──────────────────────────────
  readSensors();

  // ── Phase 2: Send telemetry to ESP32 ──────────────────
  if (millis() - lastUartSend >= UART_INTERVAL_MS) {
    sendTelemetry();
    lastUartSend = millis();
  }

  // ── Phase 3: Obstacle avoidance ───────────────────────
  if (obstacleDetected()) {
    avoidObstacle();
    return;  // restart loop — re-read sensors immediately
  }

  // ── Phase 4/5: Fire or smoke detection ────────────────
  if (flameDetected() || smokeDetected) {
    enterHighAlert();       // Phase 5 — stop, buzzers, LEDs
    alignToFlame();         // Phase 6+7 — turn until centered
    suppressFire();         // Phase 8+9 — pump + sweep until clear
    resumePatrol();         // back to normal
    return;
  }

  // ── Phase 4: Normal patrol ─────────────────────────────
  checkDarkness();           // optional headlight control
  moveForward(PATROL_SPEED); // keep moving
}

// ════════════════════════════════════════════════════════════
//  SENSOR FUNCTIONS
// ════════════════════════════════════════════════════════════

/*
 *  readSensors()
 *  Reads all sensors and updates global state variables.
 *  Called every loop iteration.
 */
void readSensors() {
  // DHT11 — temperature and humidity
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) temperature = t;
  if (!isnan(h)) humidity    = h;

  // HC-SR04 — distance in cm
  distanceCm = readUltrasonic();

  // Flame sensors — LOW = flame detected (active LOW)
  flameLeft   = (digitalRead(FLAME_LEFT)   == LOW);
  flameCenter = (digitalRead(FLAME_CENTER) == LOW);
  flameRight  = (digitalRead(FLAME_RIGHT)  == LOW);

  // MQ-2 — HIGH = smoke/gas above threshold
  smokeDetected = (digitalRead(MQ2_DO_PIN) == HIGH);

  // LDR — low analog value = dark environment
  isDark = (analogRead(LDR_PIN) < LDR_DARK_THRESHOLD);
}

/*
 *  readUltrasonic()
 *  Sends a pulse on TRIG and measures echo duration.
 *  Returns distance in centimetres.
 *  Returns 999 if no echo received (out of range).
 */
float readUltrasonic() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout
  if (duration == 0) return 999.0;                // no echo = open space
  return (duration * 0.0343) / 2.0;
}

/*
 *  obstacleDetected()
 *  Returns true if something is within the obstacle threshold.
 */
bool obstacleDetected() {
  return (distanceCm > 0 && distanceCm < OBSTACLE_THRESHOLD_CM);
}

/*
 *  flameDetected()
 *  Returns true if ANY flame sensor is triggered.
 */
bool flameDetected() {
  return (flameLeft || flameCenter || flameRight);
}

// ════════════════════════════════════════════════════════════
//  TELEMETRY
// ════════════════════════════════════════════════════════════

/*
 *  sendTelemetry()
 *  Sends a comma-delimited data packet to the ESP32-CAM via UART.
 *
 *  Format:  T:28.5,H:62.3,D:45.0,F:1,S:0\n
 *    T = temperature (°C)
 *    H = humidity (%)
 *    D = distance (cm)
 *    F = flame detected (0/1)
 *    S = smoke detected (0/1)
 */
void sendTelemetry() {
  Serial.print("T:");  Serial.print(temperature, 1);
  Serial.print(",H:"); Serial.print(humidity, 1);
  Serial.print(",D:"); Serial.print(distanceCm, 1);
  Serial.print(",F:"); Serial.print(flameDetected() ? 1 : 0);
  Serial.print(",S:"); Serial.println(smokeDetected ? 1 : 0);
}

// ════════════════════════════════════════════════════════════
//  OBSTACLE AVOIDANCE  (Phase 3)
// ════════════════════════════════════════════════════════════

/*
 *  avoidObstacle()
 *  Stops, reverses briefly, then turns to find a clear path.
 *  Alternates turn direction each call to avoid circling.
 */
void avoidObstacle() {
  stopMotors();
  delay(100);

  // Reverse
  moveBackward(PATROL_SPEED);
  delay(REVERSE_DURATION_MS);
  stopMotors();
  delay(100);

  // Turn (alternate direction each time)
  if (turnLeftNext) {
    turnLeft(TURN_SPEED);
  } else {
    turnRight(TURN_SPEED);
  }
  turnLeftNext = !turnLeftNext;
  delay(TURN_DURATION_MS);

  stopMotors();
  delay(100);
  moveForward(PATROL_SPEED);
}

// ════════════════════════════════════════════════════════════
//  HIGH ALERT MODE  (Phase 5)
// ════════════════════════════════════════════════════════════

/*
 *  enterHighAlert()
 *  Called the moment fire or smoke is detected.
 *  Stops the robot, starts visual/audio alerts.
 *  The ESP32 will receive the updated telemetry (F:1 or S:1)
 *  on the next UART send and trigger Telegram + dashboard alert.
 */
void enterHighAlert() {
  alertActive = true;
  stopMotors();

  // Force immediate telemetry so ESP32 triggers Telegram alert fast
  sendTelemetry();
  lastUartSend = millis();

  // Start buzzer and LEDs — will be maintained in the loops below
  tone(FLAME_RIGHT, 1000);  // 1kHz tone on buzzer (reusing D4 as tone pin)
  digitalWrite(LED_RED,  HIGH);
  digitalWrite(LED_BLUE, LOW);
}

// ════════════════════════════════════════════════════════════
//  FLAME ALIGNMENT  (Phase 6 + 7)
// ════════════════════════════════════════════════════════════

/*
 *  alignToFlame()
 *  Turns the robot until the CENTER flame sensor is the
 *  dominant (or only) active sensor.
 *
 *  Logic:
 *    - If only RIGHT is active → turn right
 *    - If only LEFT  is active → turn left
 *    - If CENTER is active (alone or strongest) → stop turning
 *    - If no flame detected → stop (may have been smoke only)
 *
 *  Blinks alert LEDs during alignment.
 */
void alignToFlame() {
  // Re-read before starting alignment
  readSensors();

  // If no flame at all (smoke-only trigger), skip alignment
  if (!flameDetected()) return;

  while (true) {
    readSensors();
    blinkAlertLeds(); // keep LEDs blinking during alignment

    // Aligned — center sensor active
    if (flameCenter) {
      stopMotors();
      break;
    }

    // Fire is to the right → turn right
    if (flameRight && !flameLeft) {
      turnRight(TURN_SPEED);
      delay(ALIGN_TURN_DURATION_MS);
      stopMotors();
      delay(50);
    }

    // Fire is to the left → turn left
    else if (flameLeft && !flameRight) {
      turnLeft(TURN_SPEED);
      delay(ALIGN_TURN_DURATION_MS);
      stopMotors();
      delay(50);
    }

    // Both sides equally active — nudge right by default
    else if (flameLeft && flameRight) {
      turnRight(TURN_SPEED);
      delay(ALIGN_TURN_DURATION_MS / 2);
      stopMotors();
      delay(50);
    }

    // No flame — fire may have moved or smoke-only; exit
    else {
      stopMotors();
      break;
    }

    // Send telemetry update during alignment loop
    if (millis() - lastUartSend >= UART_INTERVAL_MS) {
      sendTelemetry();
      lastUartSend = millis();
    }
  }
}

// ════════════════════════════════════════════════════════════
//  FIRE SUPPRESSION  (Phase 8 + 9)
// ════════════════════════════════════════════════════════════

/*
 *  suppressFire()
 *  Activates the water pump via relay and sweeps the nozzle
 *  servo back and forth between SERVO_SWEEP_MIN and SERVO_SWEEP_MAX.
 *
 *  Continues until ALL three flame sensors read clear (HIGH).
 *  Keeps sending telemetry and blinking LEDs throughout.
 */
void suppressFire() {
  stopMotors();

  // Activate pump (relay active LOW)
  digitalWrite(RELAY_PIN, LOW);

  int   servoPos  = SERVO_CENTER;
  int   sweepDir  = 1;  // 1 = sweeping toward max, -1 = toward min

  // Keep suppressing until flame is gone
  while (flameDetected()) {
    readSensors();
    blinkAlertLeds();

    // Step servo
    servoPos += sweepDir * SERVO_SWEEP_STEP;
    if (servoPos >= SERVO_SWEEP_MAX) {
      servoPos = SERVO_SWEEP_MAX;
      sweepDir = -1;
    } else if (servoPos <= SERVO_SWEEP_MIN) {
      servoPos = SERVO_SWEEP_MIN;
      sweepDir = 1;
    }
    nozzleServo.write(servoPos);
    delay(SERVO_SWEEP_DELAY_MS);

    // Telemetry during suppression
    if (millis() - lastUartSend >= UART_INTERVAL_MS) {
      sendTelemetry();
      lastUartSend = millis();
    }
  }

  // Flame gone — shut everything off
  digitalWrite(RELAY_PIN, HIGH); // pump OFF
  nozzleServo.write(SERVO_CENTER);
}

// ════════════════════════════════════════════════════════════
//  RESUME PATROL  (Post-suppression)
// ════════════════════════════════════════════════════════════

/*
 *  resumePatrol()
 *  Clears the alert state, turns off all alert outputs,
 *  sends a "fire extinguished" telemetry packet, and
 *  restarts forward patrol.
 */
void resumePatrol() {
  alertActive = false;

  noTone(FLAME_RIGHT);          // buzzer off
  digitalWrite(LED_RED,  LOW);
  digitalWrite(LED_BLUE, LOW);

  // Send clear status — ESP32 will update dashboard + Telegram
  // Manually override flags so the packet shows F:0
  // (sensors should already read clear, but send immediately)
  sendTelemetry();
  lastUartSend = millis();

  delay(500);
  moveForward(PATROL_SPEED);
}

// ════════════════════════════════════════════════════════════
//  OPTIONAL: DARKNESS CHECK  (Phase 4)
// ════════════════════════════════════════════════════════════

/*
 *  checkDarkness()
 *  Turns the white headlight LED on if the LDR detects low
 *  ambient light, off otherwise.
 */
void checkDarkness() {
  digitalWrite(HEADLIGHT_PIN, isDark ? HIGH : LOW);
}

// ════════════════════════════════════════════════════════════
//  ALERT LED BLINK HELPER
// ════════════════════════════════════════════════════════════

/*
 *  blinkAlertLeds()
 *  Non-blocking alternating red/blue blink.
 *  Uses millis() timing — safe to call inside loops.
 */
void blinkAlertLeds() {
  if (millis() - lastLedBlink >= LED_BLINK_INTERVAL_MS) {
    ledBlinkState = !ledBlinkState;
    digitalWrite(LED_RED,  ledBlinkState ? HIGH : LOW);
    digitalWrite(LED_BLUE, ledBlinkState ? LOW  : HIGH);
    lastLedBlink = millis();
  }
}

// ════════════════════════════════════════════════════════════
//  MOTOR CONTROL
// ════════════════════════════════════════════════════════════
/*
 *  All motor functions take a speed parameter (0–255 PWM).
 *  The L298N IN1/IN2 control left motor pair direction.
 *  The L298N IN3/IN4 control right motor pair direction.
 *
 *  HIGH/LOW on IN pins sets direction; ENA/ENB control speed.
 *  If ENA/ENB are hard-wired HIGH, motors run at full speed
 *  regardless of the analogWrite below — connect ENA/ENB to
 *  PWM pins for variable speed control.
 */

void moveForward(int speed) {
  analogWrite(IN1, speed); digitalWrite(IN2, LOW);
  analogWrite(IN3, speed); digitalWrite(IN4, LOW);
}

void moveBackward(int speed) {
  digitalWrite(IN1, LOW); analogWrite(IN2, speed);
  digitalWrite(IN3, LOW); analogWrite(IN4, speed);
}

void turnLeft(int speed) {
  // Right motors forward, left motors stopped
  analogWrite(IN3, speed); digitalWrite(IN4, LOW);
  digitalWrite(IN1, LOW);  digitalWrite(IN2, LOW);
}

void turnRight(int speed) {
  // Left motors forward, right motors stopped
  analogWrite(IN1, speed); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, LOW);
}

void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}
