/*
 * ============================================================
 *  FireBot — Motor Test
 *  motor_test.ino
 * ============================================================
 *  Purpose:
 *    Test all motor movements and the servo nozzle sweep
 *    independently before integrating into the main sketch.
 *
 *  Upload this to verify:
 *    1. All 4 motors spin in the correct direction
 *    2. Left/Right turns are correct (not mirrored)
 *    3. Speed control (PWM) works as expected
 *    4. Servo sweeps smoothly between SWEEP_MIN and SWEEP_MAX
 *    5. Relay clicks and you can hear/see the pump activate
 *
 *  The sketch runs through a fixed sequence:
 *    Forward → Backward → Turn Left → Turn Right →
 *    Stop → Servo sweep → Relay/pump test → Repeat
 *
 *  Pins used:
 *    D3  = SG90 Servo signal
 *    D10 = L298N IN1 (left  motors)
 *    D11 = L298N IN2 (left  motors)
 *    D12 = L298N IN3 (right motors)
 *    D13 = L298N IN4 (right motors)
 *    A2  = Relay IN  (water pump)
 *
 *  Open Serial Monitor at 9600 baud to follow the sequence.
 * ============================================================
 */

#include <Servo.h>

// ── Pin Definitions ─────────────────────────────────────────

#define IN1        10
#define IN2        11
#define IN3        12
#define IN4        13

#define SERVO_PIN  3
#define RELAY_PIN  A2

// ── Tunable Parameters ──────────────────────────────────────

#define TEST_SPEED      150   // 0-255 PWM for motors
#define SERVO_CENTER    90
#define SERVO_SWEEP_MIN 60
#define SERVO_SWEEP_MAX 120

// ── Objects ──────────────────────────────────────────────────

Servo nozzleServo;

// ── setup() ─────────────────────────────────────────────────

void setup() {
  Serial.begin(9600);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, HIGH); // relay OFF at start (active LOW)

  nozzleServo.attach(SERVO_PIN);
  nozzleServo.write(SERVO_CENTER);

  stopMotors();
  delay(2000);

  Serial.println("====================================");
  Serial.println("  FireBot — Motor & Servo Test");
  Serial.println("====================================");
}

// ── loop() ──────────────────────────────────────────────────

void loop() {

  // ── 1. Forward ────────────────────────────────────────
  Serial.println(">> FORWARD");
  moveForward(TEST_SPEED);
  delay(1500);
  stopMotors();
  delay(500);

  // ── 2. Backward ───────────────────────────────────────
  Serial.println(">> BACKWARD");
  moveBackward(TEST_SPEED);
  delay(1500);
  stopMotors();
  delay(500);

  // ── 3. Turn Left ──────────────────────────────────────
  Serial.println(">> TURN LEFT");
  turnLeft(TEST_SPEED);
  delay(800);
  stopMotors();
  delay(500);

  // ── 4. Turn Right ─────────────────────────────────────
  Serial.println(">> TURN RIGHT");
  turnRight(TEST_SPEED);
  delay(800);
  stopMotors();
  delay(500);

  // ── 5. Stop ───────────────────────────────────────────
  Serial.println(">> STOP");
  stopMotors();
  delay(1000);

  // ── 6. Servo sweep ────────────────────────────────────
  Serial.println(">> SERVO SWEEP  (60° → 120° → 60°)");

  // Sweep right
  for (int pos = SERVO_CENTER; pos <= SERVO_SWEEP_MAX; pos += 2) {
    nozzleServo.write(pos);
    delay(15);
  }
  // Sweep left
  for (int pos = SERVO_SWEEP_MAX; pos >= SERVO_SWEEP_MIN; pos -= 2) {
    nozzleServo.write(pos);
    delay(15);
  }
  // Return to center
  for (int pos = SERVO_SWEEP_MIN; pos <= SERVO_CENTER; pos += 2) {
    nozzleServo.write(pos);
    delay(15);
  }
  Serial.println("   Servo back to center.");
  delay(500);

  // ── 7. Relay / Pump test ──────────────────────────────
  // WARNING: This will activate your water pump.
  // Make sure tubing is pointed safely into a container.
  Serial.println(">> RELAY ON  (pump activate — 1 second)");
  digitalWrite(RELAY_PIN, LOW);  // relay ON
  delay(1000);
  digitalWrite(RELAY_PIN, HIGH); // relay OFF
  Serial.println(">> RELAY OFF (pump stop)");
  delay(500);

  // ── Pause before repeating ────────────────────────────
  Serial.println("---- Sequence complete. Repeating in 3s ----");
  Serial.println();
  delay(3000);
}

// ── Motor Control ────────────────────────────────────────────

void moveForward(int speed) {
  analogWrite(IN1, speed); digitalWrite(IN2, LOW);
  analogWrite(IN3, speed); digitalWrite(IN4, LOW);
}

void moveBackward(int speed) {
  digitalWrite(IN1, LOW); analogWrite(IN2, speed);
  digitalWrite(IN3, LOW); analogWrite(IN4, speed);
}

void turnLeft(int speed) {
  // Right motors forward only
  analogWrite(IN3, speed); digitalWrite(IN4, LOW);
  digitalWrite(IN1, LOW);  digitalWrite(IN2, LOW);
}

void turnRight(int speed) {
  // Left motors forward only
  analogWrite(IN1, speed); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, LOW);
}

void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}
