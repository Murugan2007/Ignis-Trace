# Ignis-Trace

**An AI-powered "Black Box" for forests — catching wildfire threats at the ignition phase, before they become a satellite-visible disaster.**

*Submitted to [Invent the Future with Arduino UNO Q](https://www.hackster.io) — Hackster.io*

`Arduino UNO Q` `Edge AI` `Edge Impulse` `LoRa` `Sensor Fusion` `Wildfire Detection`

---

## Table of Contents
- [The Problem](#the-problem)
- [How It Works](#how-it-works)
- [Hardware & Architecture](#hardware--architecture)
- [Why the Uno Q?](#why-the-uno-q)
- [Current Status](#current-status)
- [Firmware Versions](#firmware-versions)
- [Repository Structure](#repository-structure)
- [Roadmap](#roadmap)
- [License](#license)

---

## The Problem

Wildfires are typically detected once smoke plumes are already visible from orbit — by then, the fire is established and containment is a much harder fight. Most detection systems are top-down (satellite, aerial) and reactive.

**Ignis-Trace flips this.** It's a bottom-up, edge-computing sentry node deployed *in* the forest that listens and watches for the precursors of fire — illegal logging machinery, electrical arcing, human intrusion — and logs the evidence before things spiral out of control.

---

## How It Works

| Stage | Function |
|---|---|
| **Sense** | An I2S digital microphone continuously listens to the local acoustic environment |
| **Classify** | An on-device TinyML model distinguishes natural forest sounds from human-made acoustic threats (chainsaws, engines, electrical arcing) |
| **Verify & Log** | On a confirmed threat, the node triggers a high-speed forensic log capturing the environmental timeline around the event |
| **Relay** | The event is packaged into a short LoRa packet and relayed node-to-node back to a base receiver |
| **Visualize** | A base-station laptop/PC maps the alert to the node's physical location on a live dashboard |

---

## Hardware & Architecture

###  Arduino Uno Q — Edge AI Core
The project is built around the Uno Q's dual-core architecture:
- **Qualcomm MPU** — runs the lightweight Edge Impulse audio classification model, doing the heavy lifting of real-time inference
- **STM32 MCU** — handles low-latency sensor polling and real-time I/O alongside the AI core

###  Acoustic Sensing
- **Microphone:** INMP441 (I2S digital MEMS mic)
- **Model pipeline:** Edge Impulse — trained to separate ambient forest audio from threat signatures like machinery and electrical arcing

> **Note:** An earlier revision of this design included a VL53L5CX Time-of-Flight depth sensor for spatial/structural anomaly tracking. This has been **dropped** — acoustic classification alone covers the detection use case without the added complexity.

###  Thermal Sensing — Budget-Conscious Design
The original plan called for a thermal camera array (AMG8833 or higher resolution) to image heat signatures directly. Due to budget constraints, this build substitutes:
- **MLX90614** single-point IR temperature sensor
- Mounted on a **2-axis pan-tilt servo bracket**, sweeping the sensor across the field of view to build up a scanned thermal picture point-by-point instead of capturing it all at once

This trades scan speed and resolution for cost. **If budget allows, swap in an actual thermal camera (AMG8833 or better)** — the pan-tilt + MLX90614 rig is a workaround, not the ideal, and the rest of the system doesn't care which one feeds it data.

###  Node Communication — LoRa Mesh
- Each field node carries a LoRa radio and can talk to **other nodes** as well as directly to the **base receiver**
- Payloads are intentionally minimal — short, low-bandwidth alert packets (event type, confidence, timestamp), not raw sensor streams
- **Base receiver:** an ESP32 + LoRa module connected to a laptop/PC
- **Base station software:** a dashboard interface that plots incoming alerts on a map based on each node's known placement

###  Enclosure
A custom biomimetic 3D-printed enclosure, designed to blend into the forest environment and protect the electronics from the elements.

---

## Why the Uno Q?

The Uno Q's split personality — a real MPU for AI inference paired with an MCU for deterministic sensor I/O — is exactly the combination this project needs: run a real TinyML model *and* keep tight, low-latency control over sensors and radio, on a single board.

---

## Current Status

-  Microphone: **INMP441**, Edge Impulse audio classification model in progress
-  ToF sensor: **removed from scope**
-  Thermal sensing: **MLX90614 on 2-axis pan-tilt servo** (budget substitute for a proper thermal camera)
-  LoRa mesh communication (node ↔ node ↔ ESP32 base receiver): **in design**
-  Base-station mapping interface: **planned**
-  Prototyping in Arduino App Lab, bridging Python-based TinyML with real-time C++ sensor polling
-  Custom enclosure: **in design for 3D printing**

---

## Firmware Versions

The `firmware/` directory tracks the project's design evolution — not just the version submitted to the contest, but the original plan and where it's headed next. Each is a standalone reference; only the current build was field-tested within the contest timeline.

| Version | Sensors | Fusion Method | Status |
|---|---|---|---|
| **v1 — Original Design** | AMG8833 thermal camera + VL53L5CX ToF | — (single-sensor thresholding) | Reference only, superseded by budget constraints |
| **Current — Contest Build** | INMP441 mic + MLX90614 on 2-axis pan-tilt | Independent detection per sensor |  In active development for submission |
| **v2 — Fusion Upgrade** | INMP441 + MLX90614 pan-tilt | Decision-level (weighted-rule) fusion |  Designed, not built, post-contest roadmap |
| **v3 — Industrial Logic Reference** | Same hobby sensors, health-aware | Probabilistic (naive-Bayes) fusion + self-diagnostics, fault-tolerant comms, watchdog |  Architecture skeleton ,scaling reference, unbuilt |
| **v4 — Industrial Hardware Reference** | FLIR thermal core + industrial mic array + LoRaWAN + Jetson-class edge module | v3 fusion philosophy, rebuilt against real sensor data |  BOM + architecture document — deployment path, not built |

**Why keep the unbuilt versions in the repo?** They document the actual engineering path: what the ideal design looked like (v1), what budget and time forced (current build), and what a production-grade version would need — first in logic (v3), then in the actual physical hardware it would run on (v4). Judges and future contributors get the full picture, not just the snapshot that made the deadline.

- **v1** restores the original AMG8833 + VL53L5CX sensor stack for anyone who wants to reproduce the design with a real thermal camera.
- **v2** introduces decision-level sensor fusion: high-confidence audio alone alerts; medium-confidence audio *corroborated* by a thermal anomaly also alerts; a thermal spike alone only logs (a single-point IR sensor is too noisy to trust on its own).
- **v3** goes further , replacing fixed thresholds with a Bayesian posterior-probability model, adding per-sensor health/confidence tracking, automatic baseline recalibration, retry-with-backoff LoRa delivery, and a watchdog for unattended field reliability.
- **v4** goes past code entirely : a BOM and architecture document swapping every hobby component for its real industrial-grade equivalent (FLIR thermal core, industrial mic array, LoRaWAN gateway, ruggedized edge compute, solar/LiFePO4 power, IP66/67 enclosure), showing what an actual field-deployable product beyond the contest prototype would require.

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
    └── v4_industrial_hardware_BOM.md          # real industrial component BOM + architecture
```

> The current contest build (INMP441 + MLX90614 pan-tilt) lives at the project root / main firmware sketch as development progresses.

---

## Roadmap

- [☑] Finalize and deploy audio classification model on Uno Q
- [☑] Tune pan-tilt scan pattern for MLX90614 thermal sweep
- [☑] Implement LoRa node-to-node and node-to-base packet protocol
- [☑] Build ESP32 + LoRa base receiver firmware
- [☑] Build laptop dashboard (live map + alert feed)
- [ ] Design and print biomimetic field enclosure
- [ ] Field test with multiple nodes
- [ ] Post-contest: build and validate v2 decision-level fusion
- [ ] Long-term: implement v3 industrial-grade fusion architecture
- [ ] Long-term: procure and prototype v4 industrial hardware (FLIR thermal core, LoRaWAN, ruggedized enclosure)

---

## License

Released under the **BSD 3-Clause License** — see [`LICENSE`](./LICENSE) for details.

---

*Ignis-Trace — because the best time to catch a wildfire is before it's a wildfire.*
