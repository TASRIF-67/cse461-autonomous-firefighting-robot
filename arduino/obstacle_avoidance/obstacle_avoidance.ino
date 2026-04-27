/*
 * ============================================================
 *  FireBot — Obstacle Avoidance Test
 *  obstacle_avoidance.ino
 * ============================================================
 *  Purpose:
 *    Test and tune the HC-SR04 ultrasonic sensor and motor
 *    avoidance logic in isolation — without flame detection,
 *    pump, or ESP32 involved.
 *
 *  Upload this sketch first when building the robot to verify:
 *    1. HC-SR04 reads accurate distances
 *    2. Motors respond correctly to direction commands
 *    3. Avoidance threshold feels right for your environment
 *
 *  Pins used:
 *    D5  = HC-SR04 TRIG
 *    D6  = HC-SR04 ECHO
 *    D10 = L298N IN1 (left motors)
 *    D11 = L298N IN2 (left motors)
 *    D12 = L298N IN3 (right motors)
 *    D13 = L298N IN4 (right motors)
 *
 *  Open Serial Monitor at 9600 baud to watch distance readings.
 * ============================================================
 */

// ── Pin Definitions ─────────────────────────────────────────

#define TRIG_PIN  5
#define ECHO_PIN  6

#define IN1  10
#define IN2  11
#define IN3  12
#define IN4  13

// ── Tunable Parameters ──────────────────────────────────────

#define OBSTACLE_THRESHOLD_CM  20   // obstacle if closer than this
#define PATROL_SPEED           150
#define TURN_SPEED             130
#define REVERSE_DURATION_MS    300
#define TURN_DURATION_MS       400

// ── State ────────────────────────────────────────────────────

bool turnLeftNext = true;

// ── setup() ─────────────────────────────────────────────────

void setup() {
  Serial.begin(9600);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  stopMotors();
  delay(1000);

  Serial.println("Obstacle avoidance test started.");
  Serial.println("Threshold: " + String(OBSTACLE_THRESHOLD_CM) + " cm");
  moveForward(PATROL_SPEED);
}

// ── loop() ──────────────────────────────────────────────────

void loop() {
  float dist = readUltrasonic();

  // Print distance every loop for monitoring
  Serial.print("Distance: ");
  Serial.print(dist);
  Serial.println(" cm");

  if (dist > 0 && dist < OBSTACLE_THRESHOLD_CM) {
    Serial.println(">> Obstacle detected! Avoiding...");
    avoidObstacle();
  }

  delay(100); // small delay between readings
}

// ── Sensor ──────────────────────────────────────────────────

/*
 *  readUltrasonic()
 *  Fires a 10µs pulse on TRIG and measures the echo duration.
 *  Returns distance in cm. Returns 999 if out of range.
 */
float readUltrasonic() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return 999.0;
  return (duration * 0.0343) / 2.0;
}

// ── Obstacle Avoidance ───────────────────────────────────────

/*
 *  avoidObstacle()
 *  Stop → Reverse → Turn → Resume forward.
 *  Alternates turn direction to avoid circling in place.
 */
void avoidObstacle() {
  stopMotors();
  delay(100);

  moveBackward(PATROL_SPEED);
  delay(REVERSE_DURATION_MS);
  stopMotors();
  delay(100);

  if (turnLeftNext) {
    Serial.println("   Turning LEFT");
    turnLeft(TURN_SPEED);
  } else {
    Serial.println("   Turning RIGHT");
    turnRight(TURN_SPEED);
  }
  turnLeftNext = !turnLeftNext;

  delay(TURN_DURATION_MS);
  stopMotors();
  delay(100);

  Serial.println("   Resuming patrol.");
  moveForward(PATROL_SPEED);
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
  analogWrite(IN3, speed); digitalWrite(IN4, LOW);
  digitalWrite(IN1, LOW);  digitalWrite(IN2, LOW);
}

void turnRight(int speed) {
  analogWrite(IN1, speed); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, LOW);
}

void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}
