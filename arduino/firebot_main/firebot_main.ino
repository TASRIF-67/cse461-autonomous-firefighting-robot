#include <Servo.h>
#include <DHT.h>

// Motor pins
#define M1A 13
#define M1B 12
#define M2A 11
#define M2B 10
#define MotorSpeed 3

// Sensor pins
#define flameL A0
#define flameC A1
#define flameR A2
#define smokeSensor A3
#define DHT_PIN 7
#define DHT_TYPE DHT11

// Actuator pins
#define servoPin 9
#define relayPin 8
#define headlightPin 4

// Ultrasonic
#define trigPin 6
#define echoPin 5

// Servo angles
#define SERVO_CENTER 90
#define SERVO_MIN 55
#define SERVO_MAX 125

// Flame thresholds
const int FLAME_STRONG_LR = 250;
const int FLAME_STRONG_C = 350;

// Speed levels
const int SPEED_FAST = 220;
const int SPEED_NORMAL = 160;
const int SPEED_SLOW = 100;
const int SPEED_CRAWL = 70;
const int SPEED_TURN = 240;

// Timing
const unsigned long PUMP_TIMEOUT = 10000;
const unsigned long SEND_INTERVAL = 500;
const unsigned long DHT_INTERVAL = 2000;

const int FIRE_CONFIRM_COUNT = 3;

Servo Fire_Hose;
DHT dht(DHT_PIN, DHT_TYPE);

unsigned long pumpStartTime = 0;
unsigned long lastSendTime = 0;
unsigned long lastDHTTime = 0;
bool pumpActive = false;
bool headlightsOn = false;
int fireConfirmCount = 0;
String currentStatus = "Standby";

float cachedTemp = 0.0;
float cachedHum = 0.0;

// Motor functions
void Forward()
{
  digitalWrite(M1A, LOW);
  digitalWrite(M1B, HIGH);
  digitalWrite(M2A, HIGH);
  digitalWrite(M2B, LOW);
}

void Backward()
{
  digitalWrite(M1A, HIGH);
  digitalWrite(M1B, LOW);
  digitalWrite(M2A, LOW);
  digitalWrite(M2B, HIGH);
}

void Right()
{
  digitalWrite(M1A, LOW);
  digitalWrite(M1B, HIGH);
  digitalWrite(M2A, LOW);
  digitalWrite(M2B, HIGH);
}

void Left()
{
  digitalWrite(M1A, HIGH);
  digitalWrite(M1B, LOW);
  digitalWrite(M2A, HIGH);
  digitalWrite(M2B, LOW);
}

void Stop()
{
  digitalWrite(M1A, LOW);
  digitalWrite(M1B, LOW);
  digitalWrite(M2A, LOW);
  digitalWrite(M2B, LOW);
}

void SetSpeed(int spd)
{
  analogWrite(MotorSpeed, constrain(spd, 0, 255));
}

// Speed helpers
int distanceToSpeed(long cm)
{
  if (cm < 15)
  {
    return 0;
  }
  else if (cm < 20)
  {
    return SPEED_CRAWL;
  }
  else if (cm < 35)
  {
    return SPEED_SLOW;
  }
  else
  {
    return SPEED_NORMAL;
  }
}

// Headlights
void setHeadlights(bool state)
{
  headlightsOn = state;
  if (state)
  {
    digitalWrite(headlightPin, HIGH);
  }
  else
  {
    digitalWrite(headlightPin, LOW);
  }
}

// Pump
void Flooding(bool status)
{
  if (status && !pumpActive)
  {
    pumpStartTime = millis();
    pumpActive = true;
    digitalWrite(relayPin, LOW);
  }
  else if (!status)
  {
    pumpActive = false;
    digitalWrite(relayPin, HIGH);
  }
}

void checkPumpTimeout()
{
  if (pumpActive && millis() - pumpStartTime > PUMP_TIMEOUT)
  {
    Flooding(false);
  }
}

// Ultrasonic
long DistanceCM()
{
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long dur = pulseIn(echoPin, HIGH, 30000);
  long dist = dur * 0.034 / 2;

  if (dist == 0 || dist > 400)
  {
    return 5;
  }
  return dist;
}

// Servo sweep
void Hose_sweep(int from, int to)
{
  int range = abs(to - from);
  int mid = (from + to) / 2;
  int step = 1;
  if (from > to)
  {
    step = -1;
  }

  for (int a = from; a != to + step; a += step)
  {
    Fire_Hose.write(a);
    int distFromMid = abs(a - mid);
    int ratio = map(distFromMid, 0, range / 2, 0, 100);
    int ms = map(ratio, 0, 100, 8, 20);
    delay(ms);
  }
}

// Fire detection with debounce
bool FireDetected()
{
  bool hit = false;

  int leftValue = analogRead(flameL);
  if (leftValue <= FLAME_STRONG_LR)
  {
    hit = true;
  }

  int centerValue = analogRead(flameC);
  if (centerValue <= FLAME_STRONG_C)
  {
    hit = true;
  }

  int rightValue = analogRead(flameR);
  if (rightValue <= FLAME_STRONG_LR)
  {
    hit = true;
  }

  if (hit)
  {
    fireConfirmCount++;
  }
  else
  {
    fireConfirmCount = max(0, fireConfirmCount - 1);
  }

  return fireConfirmCount >= FIRE_CONFIRM_COUNT;
}

// Smart move - returns 0 = done, 1 = fire found, 2 = obstacle
int SmartMove(unsigned long moveTime, int spd = SPEED_NORMAL, bool isTurn = false)
{
  fireConfirmCount = 0;
  unsigned long t = millis();
  SetSpeed(spd);

  while (millis() - t < moveTime)
  {
    delay(20);

    int lf = analogRead(flameL);
    int cf = analogRead(flameC);
    int rf = analogRead(flameR);

    if (lf <= FLAME_STRONG_LR || cf <= FLAME_STRONG_C || rf <= FLAME_STRONG_LR)
    {
      Stop();
      fireConfirmCount = 0;
      return 1;
    }

    long d = DistanceCM();
    if (d < 10)
    {
      Stop();
      return 2;
    }

    if (!isTurn)
    {
      SetSpeed(distanceToSpeed(d));
    }
  }
  return 0;
}

// Fire fighting
void FightLeft()
{
  Stop();
  setHeadlights(true);
  currentStatus = "Fighting Fire";

  if (DistanceCM() < 20)
  {
    Backward();
    delay(400);
    Stop();
  }

  Flooding(true);
  Hose_sweep(SERVO_CENTER, SERVO_MIN);
  Hose_sweep(SERVO_MIN, SERVO_CENTER);
  Flooding(false);
  Fire_Hose.write(SERVO_CENTER);
}

void FightCenter()
{
  Stop();
  setHeadlights(true);
  currentStatus = "Fighting Fire";

  Flooding(true);
  Hose_sweep(SERVO_CENTER, SERVO_MAX);
  Hose_sweep(SERVO_MAX, SERVO_MIN);
  Hose_sweep(SERVO_MIN, SERVO_CENTER);
  Flooding(false);
  Fire_Hose.write(SERVO_CENTER);
}

void FightRight()
{
  Stop();
  setHeadlights(true);
  currentStatus = "Fighting Fire";

  if (DistanceCM() < 20)
  {
    Backward();
    delay(400);
    Stop();
  }

  Flooding(true);
  Hose_sweep(SERVO_CENTER, SERVO_MAX);
  Hose_sweep(SERVO_MAX, SERVO_CENTER);
  Flooding(false);
  Fire_Hose.write(SERVO_CENTER);
}

// DHT read
void updateDHT()
{
  if (millis() - lastDHTTime < DHT_INTERVAL)
  {
    return;
  }

  lastDHTTime = millis();
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (!isnan(t) && !isnan(h))
  {
    cachedTemp = t;
    cachedHum = h;
  }
}

// Send data to ESP32
void SendToESP(int lf, int cf, int rf, int smoke)
{
  if (millis() - lastSendTime < SEND_INTERVAL)
  {
    return;
  }

  lastSendTime = millis();

  bool fireIR = false;
  if (lf <= FLAME_STRONG_LR || rf <= FLAME_STRONG_LR)
  {
    fireIR = true;
  }

  bool flameCtr = false;
  if (cf <= FLAME_STRONG_C)
  {
    flameCtr = true;
  }

  bool motorsOn = false;
  if (currentStatus != "Standby")
  {
    motorsOn = true;
  }

  String line = String(cachedTemp, 1) + "," +
                String(cachedHum, 1) + "," +
                String(fireIR ? 1 : 0) + "," +
                String(flameCtr ? 1 : 0) + "," +
                String(smoke) + "," +
                String(motorsOn ? 1 : 0) + "," +
                String(pumpActive ? 1 : 0) + "," +
                currentStatus;

  Serial.println(line);
}

// Setup
void setup()
{
  pinMode(M1A, OUTPUT);
  pinMode(M1B, OUTPUT);
  pinMode(M2A, OUTPUT);
  pinMode(M2B, OUTPUT);
  pinMode(MotorSpeed, OUTPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(headlightPin, OUTPUT);
  pinMode(flameL, INPUT);
  pinMode(flameC, INPUT);
  pinMode(flameR, INPUT);
  pinMode(smokeSensor, INPUT);

  Serial.begin(9600);
  dht.begin();

  SetSpeed(SPEED_NORMAL);
  Flooding(false);
  Stop();
  setHeadlights(false);

  Fire_Hose.attach(servoPin);
  Fire_Hose.write(SERVO_CENTER);

  delay(2000);
  updateDHT();
}

// Main loop
void loop()
{
  int lf = analogRead(flameL);
  int cf = analogRead(flameC);
  int rf = analogRead(flameR);
  int smoke = analogRead(smokeSensor);
  long dist = DistanceCM();

  checkPumpTimeout();
  updateDHT();

  // Check for fire and fight
  if (lf <= FLAME_STRONG_LR)
  {
    FightLeft();
  }
  else if (cf <= FLAME_STRONG_C)
  {
    FightCenter();
  }
  else if (rf <= FLAME_STRONG_LR)
  {
    FightRight();
  }
  else
  {
    // No fire - patrol
    currentStatus = "Patrolling";
    Flooding(false);
    setHeadlights(true);

    if (dist > 25)
    {
      Forward();
      SetSpeed(distanceToSpeed(dist));
    }
    else
    {
      Stop();
      delay(200);

      // Try turning right first
      Right();
      int result = SmartMove(450, SPEED_TURN, true);

      if (result == 0)
      {
        dist = DistanceCM();

        if (dist > 25)
        {
          // Right side is clear, go forward then correct
          Forward();
          SmartMove(600, SPEED_NORMAL, false);
          Left();
          SmartMove(400, SPEED_TURN, true);
          Forward();
        }
        else
        {
          // Right blocked, try left
          Left();
          SmartMove(700, SPEED_TURN, true);
          dist = DistanceCM();

          if (dist > 25)
          {
            // Left side is clear, go forward then correct
            Forward();
            SmartMove(600, SPEED_NORMAL, false);
            Right();
            SmartMove(400, SPEED_TURN, true);
            Forward();
          }
          else
          {
            // Both sides blocked, keep turning left until clear
            int attempts = 0;
            while (DistanceCM() < 25 && attempts < 10)
            {
              Left();
              SmartMove(300, SPEED_TURN, true);
              Stop();
              delay(100);
              attempts++;
            }
          }
        }
      }

      Stop();
    }
  }

  SendToESP(lf, cf, rf, smoke);
  delay(10);
}
