# 🔄 Operational Flowchart — Logic Walkthrough

This document explains every decision and action in the FireBot control loop, based on the operational flowchart diagram. Reference `diagrams/operational-flowchart.png` alongside this.

---

## Overview

The robot's logic is a continuous loop with two parallel concerns:

1. **Patrol & Obstacle Avoidance** — keep moving, avoid walls
2. **Fire Detection & Suppression** — detect, locate, extinguish

The cycle is: **Patrol → Detect → Alert → Suppress → Verify → Resume**

---

## Step-by-Step Logic

### Phase 1 — Initialize

```
START
  └── Initialize patrol and FPV stream
        - Set up all pin modes
        - Start UART serial to ESP32
        - Initialize servo to center (90°)
        - Ensure pump relay is OFF
        - Start motors in forward patrol mode
        - ESP32 starts WiFi + web server
```

---

### Phase 2 — Read Sensors & Send Telemetry

```
READ sensors and send telemetry
  - Read DHT11 (temperature, humidity)
  - Read HC-SR04 (distance in cm)
  - Read MQ-2 (smoke/gas digital)
  - Read 3× flame sensors (Left, Center, Right)
  - Read LDR (light level, optional)
  - Send data string to ESP32 over UART
  - ESP32 updates /data endpoint → dashboard refreshes
```

This happens **every loop iteration** — it is always running.

---

### Phase 3 — Obstacle Check

```
Obstacle less than 20 cm?

  YES →  Stop motors
         Reverse slightly (move backward ~300ms)
         Turn (left or right, alternating or random)
         Return to patrol forward
         → Go back to READ SENSORS

  NO  →  Continue to smoke/flame check
```

The 20 cm threshold can be tuned in code. Reducing it makes the robot bolder; increasing it makes it more cautious.

---

### Phase 4 — Smoke or Flame Check

```
Smoke or flame detected?
  (MQ-2 digital HIGH, or any flame sensor reads LOW)

  NO  →  Check LDR for darkness
         LDR indicates darkness?
           YES → Turn on white headlight LED
           NO  → Continue with normal lights
         Move forward, continue patrol
         → Go back to READ SENSORS

  YES →  Enter HIGH ALERT MODE (Phase 5)
```

---

### Phase 5 — High Alert Mode

Triggered when smoke or flame is first detected. This is a state change — the robot **stops patrolling** and focuses entirely on the fire.

```
Enter High Alert Mode
  ├── Activate siren buzzer (continuous or pulsing tone)
  ├── Activate red/blue LEDs (alternating blink)
  ├── ESP32-CAM updates dashboard status (button: true → alert banner shows)
  └── ESP32-CAM captures photo and sends Telegram alert
        - Photo of the fire/scene
        - Alert text: "🔥 FIRE DETECTED — FireBot is responding"
        - Location timestamp
```

---

### Phase 6 — Flame Direction Scan

```
Scan flame direction: Left / Center / Right
  - Read all three flame sensors simultaneously
  - Determine which sensor has the strongest signal
    (digital: which one reads LOW; or analog: which reads lowest voltage)

  Results:
    Left only   → fire is to the left
    Center only → fire is directly ahead
    Right only  → fire is to the right
    Multiple    → use priority: Center > Left > Right
```

---

### Phase 7 — Alignment Check

```
Aligned with fire center?

  NO  →  Turn toward stronger flame direction
         (small turn increment, ~15°)
         → Re-scan flame direction
         → Check alignment again

  YES →  Stop rover movement
         Proceed to suppression
```

This is a **feedback loop** — the robot keeps turning and checking until the center sensor is the strongest reading.

---

### Phase 8 — Fire Suppression

```
Stop rover movement (motors off)
Activate relay → Start water pump
Sweep nozzle with servo: +30° to -30° (left to right sweep)
  - Servo starts at 60°, sweeps to 120° (±30° from center 90°)
  - Slow sweep speed to maximize water coverage
  - Repeat sweep continuously while flame detected
```

---

### Phase 9 — Verify Extinguished

```
Flame still detected?
  (All three flame sensors read HIGH = no flame)

  YES →  Continue sweeping nozzle
         → Loop back to check flame sensors

  NO  →  Fire extinguished!
         Turn off pump (relay OFF)
         Stop servo sweep (return to center 90°)
         Turn off buzzer
         Turn off red/blue LEDs
         Send "fire extinguished" Telegram update
         → Resume patrol (go back to Phase 2)
```

---

## State Summary Table

| State | Motors | Pump | Buzzer | LEDs | Servo |
|---|---|---|---|---|---|
| Patrolling | Forward | OFF | OFF | Normal | Center |
| Obstacle | Reverse/Turn | OFF | OFF | Normal | Center |
| Dark patrol | Forward | OFF | OFF | Headlight ON | Center |
| High Alert | STOPPED | OFF | ON | Red+Blue blink | Center |
| Aligning | Turning | OFF | ON | Red+Blue blink | Center |
| Suppressing | STOPPED | ON | ON | Red+Blue blink | Sweeping |
| Extinguished | Forward | OFF | OFF | Normal | Center |

---

## Code Structure (Arduino)

The main Arduino sketch mirrors this flowchart:

```cpp
void loop() {
  readSensors();          // Phase 2 — always runs
  sendTelemetry();        // Phase 2 — send to ESP32

  if (obstacleDetected()) {
    avoidObstacle();      // Phase 3
    return;
  }

  if (flameOrSmokeDetected()) {
    enterHighAlert();     // Phase 5
    scanFlameDirection(); // Phase 6
    alignToFlame();       // Phase 7 — loop until aligned
    suppressFire();       // Phase 8+9 — loop until extinguished
    resumePatrol();       // Phase 2
  } else {
    checkLDR();           // Phase 4 optional
    patrolForward();      // Continue patrol
  }
}
```

---

## Tuning Parameters

These values in the Arduino sketch can be adjusted for your environment:

| Parameter | Default | Effect |
|---|---|---|
| `OBSTACLE_THRESHOLD_CM` | 20 | Closer = bolder robot |
| `SERVO_SWEEP_MIN` | 60° | Left nozzle limit |
| `SERVO_SWEEP_MAX` | 120° | Right nozzle limit |
| `SERVO_SWEEP_SPEED` | 15ms/step | Slower = more coverage |
| `ALIGN_INCREMENT` | 15° | Turn step during alignment |
| `PATROL_SPEED` | 150/255 | Motor PWM during patrol |
| `UART_INTERVAL_MS` | 500 | How often to send telemetry |

---

Next: [Telegram Setup →](telegram-setup.md)
