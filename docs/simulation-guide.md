# FireBot — Wokwi Simulation Guide

Wokwi ([wokwi.com](https://wokwi.com)) is a free browser-based simulator
that supports Arduino UNO, all the sensors FireBot uses, and a Serial
Monitor identical to the Arduino IDE. This guide walks through setting up
and running each Arduino sketch in simulation before touching real hardware.

> No account is required to run simulations. An account is only needed
> to save your project.

---

## What This Guide Covers

| Sketch | Simulated | Notes |
|---|---|---|
| `flame_detection.ino` | ✅ Full | Flame sensors replaced by push buttons |
| `obstacle_avoidance.ino` | ✅ Full | HC-SR04 distance slider built into Wokwi |
| `firebot_main.ino` | ✅ Mostly | Flame sensors + MQ-2 replaced by buttons |
| `telegram_test.ino` | ❌ Skip | Needs real WiFi and Telegram API |
| `firebot_esp32.ino` | ❌ Skip | Needs real camera and Telegram API |

---

## How Wokwi Works

Every Wokwi project has two files:

- **`sketch.ino`** — paste your Arduino code here directly, no changes needed
- **`diagram.json`** — defines which components exist and how they are wired

You edit both inside the browser. When you click **▶ Start Simulation**,
Wokwi compiles and runs the sketch just like the Arduino IDE would, and
opens a Serial Monitor at the bottom of the screen.

---

## Flame Sensor Substitution

The KY-026 flame sensor is not in Wokwi's library. It is replaced by a
**push button** in all simulations below.

This works because the KY-026 digital output is **active LOW** — it pulls
the pin LOW when flame is detected. A push button connected between the
pin and GND does exactly the same thing when pressed.

```
Real hardware:   KY-026 DO pin → LOW when flame present
Simulation:      Push button   → LOW when pressed (held down = flame present)
```

Press and hold a button during simulation to simulate a flame being present.
Release it to simulate flame gone.

---

## Simulation 1 — Flame Detection Test

**Sketch:** `arduino/flame_detection/flame_detection.ino`

This simulation tests flame sensors (LEFT, CENTER, RIGHT), MQ-2 smoke
sensor, DHT11 temperature/humidity, alert LEDs, and the buzzer.

### Step 1 — Create the project

1. Go to [wokwi.com](https://wokwi.com)
2. Click **New Project** → select **Arduino UNO**
3. Delete the default `sketch.ino` content and paste your full
   `flame_detection.ino` code

### Step 2 — Add components

Click the **+** button in the component panel and add the following.
Search by name for each one:

| Component | Quantity | Wokwi name to search |
|---|---|---|
| Push button | 4 | `pushbutton` |
| DHT22 sensor | 1 | `dht22` (used in place of DHT11) |
| LED (red) | 1 | `led` |
| LED (blue) | 1 | `led` |
| Resistor 220Ω | 2 | `resistor` |
| Piezo buzzer | 1 | `buzzer` |

> Wokwi does not have a DHT11 component. Use DHT22 instead — same
> wiring, same code behavior. The only difference is the sensor type
> constant, which does not affect what this test checks.

### Step 3 — Wire the components

Connect everything according to this table. In Wokwi, click a component
pin and drag to the destination pin to draw a wire.

**Push buttons (flame sensors and MQ-2):**

| Button | Arduino pin | Other leg | What it simulates |
|---|---|---|---|
| Button 1 | D2 | GND | Flame sensor LEFT |
| Button 2 | D3 | GND | Flame sensor CENTER |
| Button 3 | D4 | GND | Flame sensor RIGHT |
| Button 4 | D7 | GND | MQ-2 smoke sensor |

> Each button also needs the Arduino pin connected to 5V through a
> 10kΩ pull-up resistor — this matches how the real KY-026 module
> behaves. In Wokwi, right-click the button and enable
> **"Pull up resistor"** from the properties panel instead of adding
> a physical resistor component.

**DHT22 (temperature/humidity):**

| DHT22 pin | Arduino pin |
|---|---|
| VCC | 5V |
| GND | GND |
| DATA | D8 |

**Red LED:**

| LED pin | Destination |
|---|---|
| Anode (+) | D A3 via 220Ω resistor |
| Cathode (−) | GND |

**Blue LED:**

| LED pin | Destination |
|---|---|
| Anode (+) | D A4 via 220Ω resistor |
| Cathode (−) | GND |

**Buzzer:**

| Buzzer pin | Arduino pin |
|---|---|
| + | D4 |
| − | GND |

> D4 is shared between Flame RIGHT button and the buzzer in the real
> sketch. In simulation, connect the buzzer to D4 as well — Wokwi
> handles the shared pin without conflict since the button pulls LOW
> only when pressed and the buzzer tone only fires when flame is active.

### Step 4 — Update the DHT type

Because Wokwi uses DHT22 instead of DHT11, change one line in the
pasted sketch before running:

```cpp
// Change this:
#define DHT_TYPE  DHT11

// To this:
#define DHT_TYPE  DHT22
```

This is simulation-only. Change it back before flashing to real hardware.

### Step 5 — Run the simulation

1. Click **▶ Start Simulation**
2. Open the **Serial Monitor** tab at the bottom (9600 baud)
3. You should see readings printing every 300ms:

```
FLAME  L: no  C: no  R: no   SMOKE: no   TEMP:25.0C  HUM:50%
```

### Step 6 — Test each sensor

| Test | Action in Wokwi | Expected Serial output |
|---|---|---|
| Flame LEFT | Press and hold Button 1 | `L:YES` + `>> FIRE LEFT` |
| Flame CENTER | Press and hold Button 2 | `C:YES` + `>> FIRE AHEAD` |
| Flame RIGHT | Press and hold Button 3 | `R:YES` + `>> FIRE RIGHT` |
| Both sides | Hold Button 1 + Button 3 | `>> FIRE BOTH SIDES` |
| Smoke | Press and hold Button 4 | `SMOKE:YES` + `+ SMOKE` |
| All clear | Release all buttons | All readings back to `no` |
| LEDs | Press any flame button | Red LED on, blue LED off |
| Buzzer | Press any flame button | Buzzer icon activates in Wokwi |
| Temperature | Click DHT22 component | Drag the temp slider, watch Serial |

---

## Simulation 2 — Obstacle Avoidance Test

**Sketch:** `arduino/obstacle_avoidance/obstacle_avoidance.ino`

This simulation tests the HC-SR04 ultrasonic sensor and L298N motor
driver logic. Because Wokwi cannot physically drive wheels, you watch
the Serial Monitor to confirm the correct motor commands are being sent.

### Step 1 — Create the project

1. New Project → **Arduino UNO**
2. Paste your full `obstacle_avoidance.ino` code into `sketch.ino`

### Step 2 — Add components

| Component | Quantity |
|---|---|
| HC-SR04 ultrasonic sensor | 1 |

> You do not need to add an L298N or motors in Wokwi for this test.
> The motor pins (D10–D13) are digital outputs — you will verify them
> through Serial output instead. Add `Serial.println()` statements if
> your sketch does not already print motor state (see Step 4 below).

### Step 3 — Wire the HC-SR04

| HC-SR04 pin | Arduino pin |
|---|---|
| VCC | 5V |
| GND | GND |
| TRIG | D5 |
| ECHO | D6 |

### Step 4 — Add debug Serial prints (temporary)

To see motor state in the Serial Monitor without real motors, add these
lines inside each motor function in the sketch. Remove them before
flashing to real hardware.

```cpp
void moveForward(int speed) {
  Serial.println("[MOTOR] Forward");   // add this line
  analogWrite(IN1, speed); digitalWrite(IN2, LOW);
  analogWrite(IN3, speed); digitalWrite(IN4, LOW);
}

void moveBackward(int speed) {
  Serial.println("[MOTOR] Backward");  // add this line
  digitalWrite(IN1, LOW); analogWrite(IN2, speed);
  digitalWrite(IN3, LOW); analogWrite(IN4, speed);
}

void turnLeft(int speed) {
  Serial.println("[MOTOR] Turn Left"); // add this line
  analogWrite(IN3, speed); digitalWrite(IN4, LOW);
  digitalWrite(IN1, LOW);  digitalWrite(IN2, LOW);
}

void turnRight(int speed) {
  Serial.println("[MOTOR] Turn Right");// add this line
  analogWrite(IN1, speed); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, LOW);
}

void stopMotors() {
  Serial.println("[MOTOR] Stop");      // add this line
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}
```

### Step 5 — Run the simulation

1. Click **▶ Start Simulation**
2. Open Serial Monitor at **9600 baud**
3. You should see `[MOTOR] Forward` immediately on boot

### Step 6 — Test obstacle detection

Wokwi's HC-SR04 component has a **distance slider** you can drag in
real time during simulation.

| Test | Action | Expected Serial output |
|---|---|---|
| Normal patrol | Distance slider > 20cm | `[MOTOR] Forward` continuously |
| Obstacle near | Drag slider below 20cm | `[MOTOR] Stop` → `[MOTOR] Backward` → `[MOTOR] Turn Left` or `Turn Right` |
| Obstacle cleared | Drag slider back above 20cm | `[MOTOR] Forward` resumes |
| Alternating turns | Trigger obstacle twice | First avoidance turns one way, second turns the other |
| Out of range | Drag slider to max | `[MOTOR] Forward` (999cm = open space) |

---

## Simulation 3 — Full Main Sketch Test

**Sketch:** `arduino/firebot_main/firebot_main.ino`

This is the most complete simulation. It combines everything from
Simulations 1 and 2 and also tests the UART telemetry output that the
ESP32 reads.

### Step 1 — Create the project

1. New Project → **Arduino UNO**
2. Paste your full `firebot_main.ino` code

### Step 2 — Add all components

| Component | Quantity | Simulates |
|---|---|---|
| Push button | 3 | Flame sensors LEFT, CENTER, RIGHT |
| Push button | 1 | MQ-2 smoke sensor |
| HC-SR04 | 1 | Ultrasonic distance sensor |
| DHT22 | 1 | DHT11 temperature/humidity |
| Servo motor | 1 | Nozzle sweep servo |
| LED (red) | 1 | Alert LED red |
| LED (blue) | 1 | Alert LED blue |
| Resistor 220Ω | 2 | LED current limiting |
| Piezo buzzer | 1 | Alert buzzer |

> Motors and the water pump relay are omitted — verify them through
> Serial print statements as described in Simulation 2 Step 4.

### Step 3 — Wire everything

**Flame sensor buttons:**

| Button | Pin | Pull-up | Simulates |
|---|---|---|---|
| Button 1 | D2 | Enable in properties | FLAME_LEFT |
| Button 2 | D3 | Enable in properties | FLAME_CENTER |
| Button 3 | D4 | Enable in properties | FLAME_RIGHT |

**Smoke sensor button:**

| Button | Pin | Pull-up | Simulates |
|---|---|---|---|
| Button 4 | D7 | Enable in properties | MQ2_DO_PIN |

> MQ-2 digital output goes HIGH when smoke is detected (opposite of
> flame sensors). In Wokwi, right-click Button 4 → Properties →
> set **"Active state"** to **HIGH** so pressing it sends HIGH to D7.

**HC-SR04:**

| Pin | Arduino |
|---|---|
| VCC | 5V |
| GND | GND |
| TRIG | D5 |
| ECHO | D6 |

**DHT22:**

| Pin | Arduino |
|---|---|
| VCC | 5V |
| GND | GND |
| DATA | D8 |

**Servo:**

| Pin | Arduino |
|---|---|
| Signal | D3 |
| VCC | 5V |
| GND | GND |

> D3 is shared between FLAME_CENTER button and the servo signal in
> the default pin map. In Wokwi this causes a conflict. Move the
> servo signal wire to **D9** in the diagram and change one line
> in the sketch for this simulation only:

```cpp
// Change this:
#define SERVO_PIN  3

// To this for simulation:
#define SERVO_PIN  9
```

Remember to revert this before flashing to real hardware, or
permanently move the servo to D9 in your wiring (recommended).

**LEDs and buzzer:** same wiring as Simulation 1.

### Step 4 — Change DHT type

Same as Simulation 1 — change `DHT11` to `DHT22` in the sketch for
simulation only.

### Step 5 — Run the simulation

1. Click **▶ Start Simulation**
2. Open Serial Monitor at **9600 baud**
3. After the 2-second startup delay you should see telemetry packets:

```
FIREBOT_READY
T:25.0,H:50.0,D:150.0,F:0,S:0
T:25.0,H:50.0,D:150.0,F:0,S:0
```

### Step 6 — Test the full behavior

Work through each scenario in order:

**Telemetry format**

Confirm each field updates correctly as you change sensor states.
The ESP32 parses this exact format — any malformed packet means
the dashboard and Telegram alerts will not work.

| Field | Trigger | Expected packet change |
|---|---|---|
| Temperature | Drag DHT22 slider | `T:` value changes |
| Humidity | Drag DHT22 slider | `H:` value changes |
| Distance | Drag HC-SR04 slider | `D:` value changes |
| Flame | Press any flame button | `F:1` appears |
| Smoke | Press smoke button | `S:1` appears |

**Obstacle avoidance**

Drag HC-SR04 distance slider below 20cm. Serial Monitor should show
motor stop → reverse → turn, and the robot should resume forward
after the slider goes back above 20cm.

**Flame detection and alignment**

Press and hold individual flame buttons and confirm the correct
turn direction is chosen:

| Button held | Expected motor output |
|---|---|
| Button 1 only (LEFT) | `[MOTOR] Turn Left` |
| Button 2 only (CENTER) | `[MOTOR] Stop` — already aligned |
| Button 3 only (RIGHT) | `[MOTOR] Turn Right` |
| Button 1 + Button 3 | `[MOTOR] Turn Right` (default nudge) |

**Suppression phase**

While a flame button is held, watch the Servo component in Wokwi —
the arm should sweep back and forth between the MIN and MAX angles
defined in the sketch. Release the button to simulate fire
extinguished — the servo should return to center (90°).

**Alert LEDs and buzzer**

When any flame or smoke button is pressed, the red LED should turn on,
the blue LED should turn off, and the buzzer should activate. When all
buttons are released, LEDs and buzzer should clear.

---

## Things Wokwi Cannot Test

Be aware of these gaps before moving to real hardware:

| Item | Why simulation cannot cover it |
|---|---|
| Real flame sensor sensitivity | The potentiometer on KY-026 needs physical tuning |
| Motor direction | Actual wheel spin direction depends on physical wiring |
| Water pump relay | Relay switching and pump activation need real components |
| UART to ESP32 | Wokwi can simulate two boards but Telegram API won't respond |
| Camera capture | No camera simulation exists |
| WiFi and dashboard | Needs real network |

---

## Before Flashing to Real Hardware

After simulation passes, revert these simulation-only changes:

- [ ] Change `DHT22` back to `DHT11` in all sketches
- [ ] Change `SERVO_PIN` back to `3` (or keep `9` if you rewired)
- [ ] Remove the `Serial.println("[MOTOR] ...")` debug lines
- [ ] Confirm `credentials.h` exists in both ESP32 sketch folders

---

← Related docs: [Testing Guide](testing-guide.md) · [Hardware Setup](hardware-setup.md) · [Software Setup](software-setup.md)
