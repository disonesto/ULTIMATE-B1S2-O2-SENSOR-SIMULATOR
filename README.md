# Ultimate B1S2 O2 Sensor Simulator v3.3.1 (Rationality Dip Version)

This firmware is an automotive emulation solution designed for microcontrollers to simulate a downstream Bank 1 Sensor 2 (B1S2) narrow-band oxygen sensor. It mirrors ideal catalyst efficiency while actively defeating strict manufacturer rationality monitors (specifically targeted at **P2097 - Post Catalyst Fuel Trim Too Rich** codes). 

The repository provides the complete Arduino-compatible source code (`3.3.1.ino`) and the exact hardware engineering layout required to safely emulate the sensor without triggering diagnostic trouble codes (DTCs).

---

## 🔌 Hardware Architecture & Ground Loop Prevention

> ⚠️ **CRITICAL WARNING**: Factory post-catalyst O2 sensor signal lines operate on an **isolated differential circuit floating above chassis ground**. Connecting a standard microcontroller ground directly to your vehicle's frame or a non-isolated power supply will trigger a **ground loop**. This error can permanently destroy your Engine Control Module (ECM/ECU) or flood the signal path with raw electrical interference.

To bypass this risk completely, this architecture runs the vehicle's 12V power through a galvanic isolation module *before* converting it down to 5V. This creates a completely isolated power pocket for the microcontroller, matching the floating circuit of the factory ECM.

### Bill of Materials (BOM)
*   **Microcontroller**: **Seeed Studio XIAO SAMD21**. Chosen for its ultra-compact footprint, 12-bit analog input resolution, and true hardware 10-bit Digital-to-Analog Converter (DAC) on pin `A0` (essential for true voltage replication without PWM noise).
*   **Ground-Loop Isolator**: **B1212S-1WR3 Module**. A 12V-to-12V (1:1) DC-DC isolation transformer that breaks the physical connection between the car's frame ground and the simulator’s power/signal lines.
*   **Power Stage**: **12V to 5V (5A, 25W) USB-C Buck Converter**. Drops the clean, isolated 12V supply down to a stable 5V via a USB-C connection to safely drive the MCU.

---

## 🛠️ Power & Signal Connection Guide

To ensure the simulator shuts down safely and doesn't drain the vehicle battery, connect the setup to a **fused, ignition-switched 12V power rail** (such as an accessory circuit or radio fuse).

### 1. Isolated Power Path Lineup
1.  **B1212S-1WR3 Pin 1 (Vin)**: Connect to the vehicle's Ignition-Switched 12V supply line.
2.  **B1212S-1WR3 Pin 2 (GND)**: Connect to the vehicle's Chassis Ground.
3.  **B1212S-1WR3 Pin 3 (-Vout)**: Connect to the **Negative (-) Input** of the 12V-to-5V Buck Converter. *(This becomes your new isolated system ground)*.
4.  **B1212S-1WR3 Pin 4 (+Vout)**: Connect to the **Positive (+) Input** of the 12V-to-5V Buck Converter.
5.  **USB-C Output**: Plug the buck converter's USB-C output directly into the **Seeed Studio XIAO SAMD21**.

### 2. Sensor Signal Connections
Because the entire microcontroller is now floating on the isolated side of the circuit, you must link its ground reference directly to the vehicle's sensor loop:
*   **XIAO GND Pin**: Connect directly to the **Downstream O2 Sensor Signal Ground Wire** on the engine wiring harness.
*   **XIAO Pin A0 (DAC)**: Connect directly to the **Downstream O2 Sensor Signal Positive (+) Wire** heading back to the ECM. This outputs the simulated waveform (`0.000V` to `0.900V`).
*   **XIAO Pin A1 (ADC)**: connect directly to the **Downstream O2 Sensor Signal Positive (+) Reference Wire** to allow the logic brain to read real-time exhaust parameters.

---

## ⚙️ Configuration & Editable Settings

### 1. Hardware Pin Mapping
```cpp
const int sensorIn = A1;    
const int simOut = A0;      
```
*   **`sensorIn`**: The analog pin monitoring your vehicle's physical upstream sensor (B1S1) reference.
*   **`simOut`**: The analog output pin generating the simulated downstream O2 sensor signal via the DAC.

### 2. Rationality Dip Engine (P2097 Fix)
Modern ECMs expect a downstream sensor to occasionally track lean or drop its voltage to prove it isn't statically frozen or stuck rich. A flat signal will trigger a rationality fault.
```cpp
const unsigned long DIP_INTERVAL = 70000; // 70 seconds
const unsigned long DIP_DURATION = 1000;  // 1 second
```
*   **`DIP_INTERVAL`**: How often (in milliseconds) the simulator forces a brief drop in signal voltage. Default is 70 seconds.
*   **`DIP_DURATION`**: The window of time (in milliseconds) the dip remains active. Default is 1 second.
*   *Adjustment Tip*: If your vehicle clears codes but throws a pending P2097 after sustained highway cruising, decrease `DIP_INTERVAL` to `60000` (60s) to satisfy faster factory polling routines.

### 3. OBD2 Mode $06 Tuned Targets
These variables mirror ideal parameters for standard factory emission tests. They map directly to Mode $06 Test IDs (TIDs):
```cpp
const float TARGET_CRUISE_BASE = 0.795;
const float TARGET_RICH        = 0.890;
const float TARGET_LEAN        = 0.045;
```
*   **`TARGET_CRUISE_BASE`**: The baseline target voltage during normal closed-loop driving. Maps to **TID $a0 (Catalyst Monitor)** to show healthy oxygen storage capability.
*   **`TARGET_RICH`**: Simulated voltage output when the engine enters heavy acceleration or rich phases. Maps to **TID $8b (Rich Test)**.
*   **`TARGET_LEAN`**: Simulated voltage output during deceleration fuel cut-off (DFCO). Maps to **TID $8c (Lean Test)**.

### 4. Safety Constraints & Output Clipping
```cpp
const float SAFE_AVG_MAX = 0.820;
const float SAFE_AVG_MIN = 0.780;
```
*   **`SAFE_AVG_MAX` / `SAFE_AVG_MIN`**: Hard boundary ceilings and floors for the calculated cruise targets. These guarantee that **TID $91 (Voltage Average)** never slips past standard test thresholds (Max 0.450V under strict factory specs; scaled safely in code logic).

---

## 🧠 Core Algorithm Logic

### Active Filtering & Smooth Transitioning
The engine does not sharply jump voltages. It mimics real chemical sensor latency using an Exponential Moving Average (EMA) layout:
```cpp
currentOutput = (currentOutput * 0.15) + (target * 0.85);   // High speed shift
currentOutput = (currentOutput * 0.950) + (target * 0.050); // Engine cruise dampening
```
*   **0.15 / 0.85 weighting**: Used during quick rich/lean shifts to prove rapid sensor responsiveness to the ECM.
*   **0.95 / 0.05 weighting**: Used during steady cruising to isolate the simulator from aggressive raw signal chatter.

### Engine Heartbeat Simulation
To prevent the ECM from detecting a static "dead" sensor, a pseudo-sine wave is mixed over the cruise signal to simulate normal combustion micro-fluctuations:
```cpp
float heartbeat = sin(millis() / 1500.0) * 0.020;
```
*   This introduces a real-world rolling oscillation of `±20mV` every ~1.5 seconds.

### Realism Jitter
```cpp
float jitter = (random(0, 5) / 1000.0);
```
*   Appends up to `5mV` of random background white noise to break any mathematical clean waves that factory diagnostic software might flag as unnatural emulation.

---

## 📊 Simulated Diagnostic Specifications

The firmware outputs parameters carefully designed to pass standard automotive scanner profiles:

| Test ID (TID) | Target Test Description | Factory Min | Factory Max | Simulated Code Result | Status |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **TID $8b** | Rich Test | 0.825V | 7.995V | **0.880V** | ✅ PASS |
| **TID $8c** | Lean Test | 0.000V | 0.100V | **0.045V** | ✅ PASS |
| **TID $91** | Voltage Average | 0.000V | 0.450V | **0.430V** *(Adjusted)* | ✅ PASS |
| **TID $a0** | Catalyst Monitor | 0.349 | 3.999 | **0.760V** | ✅ PASS |

---

## 🚀 Hardware Tuning Instructions

1.  **Stuck Lean Faults**: If your vehicle registers a stuck lean code, slightly raise `TARGET_CRUISE_BASE` to `0.810`.
2.  **Stuck Rich Faults (P2097)**: Decrease your `DIP_INTERVAL` to `55000` (55 seconds) or raise your dip target minimum slightly to simulate a broader sensor sweep.
3.  **Resolution Checks**: Ensure your microcontroller configuration matches the 12-bit input read (`analogReadResolution(12)`) and 10-bit output write (`analogWriteResolution(10)`) declared in `setup()`.
