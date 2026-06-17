# Adaptive Multi-Cell Battery Intelligence Engine

A unified ESP32-based Battery Management System (BMS) built and simulated in **Wokwi**, integrating six progressive challenge tasks into a single firmware codebase and circuit.

**Live Simulation:** https://wokwi.com/projects/467059005329336321

**Author:** Krutika Yogesh Malkhede
---

## Overview

This project simulates a 4-cell lithium battery pack using potentiometers as analog voltage sources, processed by an ESP32 microcontroller. It performs real-time health monitoring, automatic safety protection, a rotating LCD diagnostic interface, fault-tolerant runtime management, and cloud telemetry with a live executive dashboard via Blynk IoT.

All six tasks below run together in **one circuit and one continuous firmware loop** — there are no separate sketches or simulations per task.

---

## Hardware Used

| Component | Quantity | Purpose |
|---|---|---|
| ESP32 Dev Module | 1 | Main microcontroller, WiFi, Blynk communication |
| Potentiometer | 4 | Simulates 4 lithium cell voltages (mapped to 2.5V–4.2V) |
| 20x4 I2C LCD | 1 | Rotating multi-screen diagnostic display |
| LED (Green / Yellow / Red / Blue) | 4 | Health status indicators |
| Buzzer | 1 | Non-blocking audible fault alert |
| Relay (logic via GPIO25) | — | Simulated load/charge cutoff for safety protection |

---

## Tasks Implemented

### 1. Adaptive Multi-Cell Battery Intelligence Engine
Reads all 4 cells, computes pack voltage, average voltage, and imbalance %, identifies weakest/strongest cell, and classifies pack health as HEALTHY / MINOR IMBALANCE / CRITICAL IMBALANCE / PACK FAILURE.

### 2. Event-Driven Safety Protection Kernel
Fully `millis()`-based, zero `delay()`. Detects weak cell, overvoltage, sensor anomaly, and rapid voltage fluctuation. Cuts off the relay with a 2-second anti-chatter cooldown and a non-blocking buzzer alert, with automatic recovery once conditions clear.

### 3. Intelligent Embedded HMI
5 rotating LCD screens (Cell Voltages / Pack Analytics / Health Status / Protection / System & Cloud) every 3 seconds, flicker-free. A dedicated fault-priority screen instantly overrides the rotation during dangerous conditions.

### 4. Fault-Tolerant Runtime System
Detects sensor disconnection, frozen ADC readings, and relay state mismatches. Runs a 4-mode state machine (NORMAL / DEGRADED / FAILSAFE / SHUTDOWN) with per-cell fault isolation and a timestamped circular fault log.

### 5. Intelligent Cloud Telemetry (Blynk)
Event-driven telemetry — data is sent only on health/mode/relay state changes, not on a fixed timer. Includes WiFi reconnect handling and an offline event queue that syncs automatically once the cloud connection is restored.

### 6. Executive Battery Intelligence Dashboard
A Blynk web dashboard with severity-colour-coded gauges, live status widgets, a timestamped fault history display, an auto-generated operator recommendation, and a historical voltage trend chart.

---

## Libraries Required

- `LiquidCrystal I2C`
- `Blynk`

---

## How to Run

1. Open the live project link above, **or** create a new ESP32 project at [wokwi.com](https://wokwi.com) and import `sketch.ino` + `diagram.json`
2. Install the libraries listed in `libraries.txt` via the Wokwi Library Manager
3. Replace the `BLYNK_TEMPLATE_ID`, `BLYNK_TEMPLATE_NAME`, and `BLYNK_AUTH_TOKEN` at the top of `sketch.ino` with your own Blynk credentials
4. Click **Start Simulation**
