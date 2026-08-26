# 🌲 Ignis-Trace

**An AI-powered "Black Box" for forests — catching wildfire threats at the ignition phase, before they become a satellite-visible disaster.**

*Submitted to [Invent the Future with Arduino UNO Q](https://www.hackster.io) — Hackster.io*

`Arduino UNO Q` `Edge AI` `Edge Impulse` `LoRa` `Sensor Fusion` `Wildfire Detection`

---

## Table of Contents
- [The Problem](#the-problem)
- [Solution Architecture](#solution-architecture)
- [How It Works](#how-it-works)
- [Why the Uno Q?](#why-the-uno-q)
- [User Journey](#user-journey)
- [Data Plan](#data-plan)
- [AI Approach](#ai-approach)
- [Responsible AI Plan](#responsible-ai-plan)
- [Current Status](#current-status)
- [Firmware Versions](#firmware-versions)
- [Repository Structure](#repository-structure)
- [Roadmap](#roadmap)
- [License](#license)

---

## The Problem

Wildfires are typically detected once smoke plumes are already visible from orbit — by then, the fire is established and containment is a much harder fight. Most detection systems are top-down (satellite, aerial) and reactive.

**Ignis-Trace flips this.** It's a bottom-up, edge-computing sentry node deployed *in* the forest that senses the precursors of fire — atmospheric gas shifts, heat signatures, and acoustic threats like illegal machinery — and escalates through a verification cascade before firing an alert, giving rangers a head start instead of a wildfire.

---

## Solution Architecture

### Sensors & Edge Node (Front-End)
- **Bosch BME680** — VOC/gas sensor, the low-power always-on trigger
- **MLX90614** — stepped thermal IR pyrometer on a micro-servo, woken on a gas-level trigger to sweep for heat
- **INMP441** — I2S MEMS microphone, used for acoustic verification of a confirmed heat event
- All three connect to an **Arduino UNO Q** dual-processor board

### Processing Division
- **STM32 MCU** — handles low-power ambient polling (BME680), micro-servo PWM for the thermal sweep, and 8×13 LED matrix display state
- **Qualcomm QRB2210 Linux MPU** — runs Python App Lab containers executing Edge Impulse audio classification, communicating with the STM32 side via inter-processor RPC (`Arduino_RouterBridge`)

### Telemetry & Gateway (Back-End)
- Alerts transmit as compact **6-byte packed binary structs** over **433 MHz SX1276 LoRa** to an **ESP32 gateway**

### Dashboard
- The ESP32 parses raw binary packets into JSON streams over USB Serial to a local **Python/Streamlit dashboard**, mapping live node status and thermal angles

---

## How It Works

Ignis-Trace runs a **3-tier verification cascade** — Gas → Thermal → Audio — so a single noisy sensor reading never fires a false alarm on its own; each stage only escalates to the next after its own signal crosses threshold.

| Stage | Function |
|---|---|
| **1. Sense (Gas)** | BME680 polls ambient VOC/gas levels continuously in an ultra-low-power sleep cycle |
| **2. Escalate (Thermal)** | A gas-level drop/shift wakes the micro-servo, which steps the MLX90614 through a scan to check for a heat source |
| **3. Verify (Audio)** | If heat is confirmed, the Linux MPU captures a 1-second audio window and runs it through the Edge Impulse classifier to confirm a human-made threat signature (chainsaw, engine, fire crackle) |
| **4. Alert** | On confirmation, a 6-byte payload fires over LoRa to the base station |
| **5. Visualize** | The Streamlit dashboard flips to alert state, pinpoints the node, and shows max hotspot temperature and servo heading |

---

## Why the Uno Q?

The Uno Q's split personality — a real MPU for AI inference paired with an MCU for deterministic sensor I/O — is exactly the combination this project needs: the STM32 side keeps the gas/thermal polling loop cheap and always-on, while the Qualcomm QRB2210 MPU only spins up its heavier Edge Impulse audio inference when the cascade actually escalates to that stage. One board covers both the ultra-low-power sentry role and the real AI verification role.

---

## User Journey

1. **Deployment** — Forestry rangers mount the IP65-rated single node on a tree trunk, covering roughly 1 hectare of coverage.
2. **Monitoring** — The system idles in an ultra-low-power sleep cycle; the 8×13 LED matrix displays a baseline green checkmark.
3. **Trigger & Escalation** — An atmospheric gas drop wakes the servo to sweep thermal angles. If heat is detected, the Linux MPU captures a 1-second audio window for acoustic verification.
4. **Alert** — A 6-byte payload fires via LoRa to the ranger tower base station. The Streamlit dashboard turns red, pinpoints the node on the map, displays the maximum hotspot temperature, and provides the exact servo heading for rapid ranger dispatch.

---

## Data Plan

- **Training data:** sourced from the **ESC-50** benchmark dataset (`crackling_fire`, `chainsaw`, `wind`, `chirping_birds`), combined with custom field recordings
- **Preprocessing:** standardized to 16 kHz, 16-bit mono PCM WAV, sliced into 1-second sliding windows
- **Validation:** 80/20 train-test split within Edge Impulse Studio, to prevent data leakage and evaluate real-world generalization

---

## AI Approach

- **Model:** 1D ConvNet / MobileNetV2 audio classifier, built in Edge Impulse using Mel-Frequency Energy (MFE) DSP feature extraction
- **Reasoning:** MFE converts 1D time-series acoustic data into 2D spectrograms, letting the model distinguish high-frequency broadband chainsaw signatures and rhythmic fire crackles from background forest noise
- **Metrics & deployment:** optimized via the Edge Impulse **EON Compiler** to hit sub-150ms inference latency and a >90% target F1-score on the Qualcomm MPU

---

## Responsible AI Plan

- **False alarms:** the strict 3-tier cascade (Gas → Thermal → Audio) exists specifically to suppress false positives — a wind gust or a non-combustion VOC shift alone can't fire an alert without corroboration from the next stage
- **Privacy:** audio is processed entirely on-device (edge AI) in a 1-second temporary RAM buffer and discarded immediately after classification. No raw ambient audio is ever recorded or transmitted over the air

---

## Current Status

- 🧪 Gas sensing: **BME680**, ambient VOC polling on the STM32 side
- 🌡️ Thermal sensing: **MLX90614** on a stepped micro-servo pyrometer sweep (triggered by gas-stage escalation)
- 🔊 Acoustic verification: **INMP441** + Edge Impulse audio classification on the QRB2210 MPU, in progress
- 🖥️ **8×13 LED matrix** status display (baseline green checkmark / red alert): in design
- 📡 **433 MHz SX1276 LoRa** → ESP32 gateway → Streamlit dashboard pipeline: in design
- 🖨️ **IP65-rated enclosure**, tree-mountable: in design for 3D printing
- 🛠️ Prototyping in Arduino App Lab, bridging Python-based TinyML (Qualcomm MPU) with real-time C++ sensor/servo control (STM32) via `Arduino_RouterBridge`

---

## Firmware Versions

The `firmware/` directory tracks the project's design evolution — not just the version submitted to the contest, but the original plan and where it's headed next. Each is a standalone reference; only the current build was field-tested within the contest timeline.

| Version | Sensors | Fusion Method | Status |
|---|---|---|---|
| **v1 — Original Design** | AMG8833 thermal camera + VL53L5CX ToF | — (single-sensor thresholding) | Reference only — superseded by budget constraints |
| **Current — Contest Build** | BME680 gas + MLX90614 stepped pyrometer + INMP441 mic, on Uno Q | 3-tier cascade (Gas → Thermal → Audio) | ✅ In active development for submission |
| **v2 — Fusion Upgrade** | Same current-build sensors | Decision-level (weighted-rule) fusion, generalized beyond the fixed 3-tier cascade | 🗺️ Designed, not built — post-contest roadmap |
| **v3 — Industrial Logic Reference** | Same hobby sensors, health-aware, Uno Q retained | Probabilistic (naive-Bayes) fusion + self-diagnostics, fault-tolerant comms, watchdog | 🗺️ Architecture skeleton — scaling reference, unbuilt |
| **v4 — Industrial Hardware Reference** | FLIR thermal core + industrial mic array + LoRaWAN, Uno Q architecture retained (MPU module swapped only) | v3 fusion philosophy, rebuilt against real sensor data | 📄 BOM + architecture document — deployment path, not built |

**Why keep the unbuilt versions in the repo?** They document the actual engineering path: what the ideal design looked like (v1), what the actual contest submission does (current build's 3-tier cascade), and what a production-grade version would need — first in logic (v2 → v3), then in the actual physical hardware it would run on (v4). Judges and future contributors get the full picture, not just the snapshot that made the deadline.

- **v1** restores the original AMG8833 + VL53L5CX sensor stack for anyone who wants to reproduce the design with a real thermal camera.
- **The current build already implements decision-level fusion** in the form of the 3-tier Gas → Thermal → Audio cascade — each stage only escalates after the previous one crosses threshold.
- **v2** generalizes that cascade into flexible weighted-rule fusion (e.g. a very high-confidence audio read alone can alert, without waiting on the full gas→thermal chain).
- **v3** goes further — replacing fixed thresholds with a Bayesian posterior-probability model, adding per-sensor health/confidence tracking, automatic baseline recalibration, retry-with-backoff LoRa delivery, and a watchdog for unattended field reliability. The Uno Q stays as the compute base throughout.
- **v4** goes past code entirely — a BOM and architecture document swapping the hobby sensors (and only the Qualcomm MPU module, not the whole board) for real industrial-grade equivalents, showing what an actual field-deployable product beyond the contest prototype would require.

---

## Repository Structure

```
ignis-trace/
├── README.md
├── LICENSE
├── firmware/
│   ├── v1_original/
│   │   └── ignis_trace_v1_original.ino        # AMG8833 + VL53L5CX
│   ├── v2_fusion_upgrade/
│   │   └── ignis_trace_v2_fusion_upgrade.ino  # decision-level fusion
│   └── v3_industrial_fusion/
│       └── ignis_trace_v3_industrial_fusion.ino  # probabilistic fusion + health/fault tolerance
└── docs/
    └── ignis_trace_v4_industrial_hardware_BOM.md  # real industrial component BOM + architecture
```

> The current contest build (BME680 + MLX90614 + INMP441 on Uno Q) lives at the project root / main firmware sketch as development progresses.

---

## Roadmap

- [ ] Finalize and deploy audio classification model on the QRB2210 MPU
- [ ] Implement gas-triggered thermal sweep escalation on the STM32 side
- [ ] Build 8×13 LED matrix status display logic
- [ ] Implement 433 MHz SX1276 LoRa 6-byte packet protocol
- [ ] Build ESP32 gateway (binary → JSON parsing over USB Serial)
- [ ] Build Streamlit dashboard (live map + thermal angle + alert feed)
- [ ] Design and print IP65-rated tree-mount enclosure
- [ ] Field test with multiple nodes
- [ ] Post-contest: build and validate v2 generalized decision-level fusion
- [ ] Long-term: implement v3 industrial-grade fusion architecture
- [ ] Long-term: procure and prototype v4 industrial hardware (FLIR thermal core, LoRaWAN, ruggedized enclosure)

---

## License

Released under the **BSD 3-Clause License** — see [`LICENSE`](./LICENSE) for details.

---

*Ignis-Trace — because the best time to catch a wildfire is before it's a wildfire.*
