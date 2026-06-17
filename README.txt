# Krutika Yogesh Malkhede — IoT Assignment Package
## Adaptive Multi-Cell Battery Intelligence Engine (Unified BMS)

This package contains the firmware source code for the complete, unified
Battery Management System covering all six challenge tasks:

1. Adaptive Multi-Cell Battery Intelligence Engine
2. Event-Driven Safety Protection Kernel
3. Intelligent Embedded HMI & Diagnostic Interface
4. Fault-Tolerant Embedded Runtime System
5. Intelligent Cloud Telemetry Architecture (Blynk)
6. Executive Battery Intelligence Dashboard (Blynk)

## Contents

- `sketch.ino` — Complete Arduino/ESP32 firmware source code (all 6 tasks integrated)
- `diagram.json` — Wokwi circuit schematic (ESP32, 4x potentiometer, 20x4 I2C LCD,
  4x LED, buzzer). Note: the relay/safety-cutoff logic in Task 2 is implemented
  purely via GPIO25 in firmware; no separate physical relay part is wired in the
  diagram for this simulation.
- `libraries.txt` — Required Wokwi libraries (LiquidCrystal I2C, Blynk)

## Live Project Link

https://wokwi.com/projects/467059005329336321

## How to Run

1. Go to https://wokwi.com and create a New ESP32 Project
2. Replace the default sketch.ino with the one in this package
3. Replace/import diagram.json with your circuit schematic
4. Add `Blynk` and `LiquidCrystal_I2C` via the Library Manager
5. Update the BLYNK_TEMPLATE_ID / BLYNK_TEMPLATE_NAME / BLYNK_AUTH_TOKEN
   at the top of sketch.ino with your own Blynk credentials
6. Click "Start Simulation"
