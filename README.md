#  Ignis-Trace

**An AI-powered "Black Box" for forests — catching wildfire threats at the ignition phase, before they become a satellite-visible disaster.**

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

### Arduino Uno Q — Edge AI Core
The project is built around the Uno Q's dual-core architecture:
- **Qualcomm MPU** — runs the lightweight Edge Impulse audio classification model, doing the heavy lifting of real-time inference
- **STM32 MCU** — handles low-latency sensor polling and real-time I/O alongside the AI core

### 🎙️ Acoustic Sensing
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
-   Base-station mapping interface: **planned**
-  Prototyping in Arduino App Lab, bridging Python-based TinyML with real-time C++ sensor polling
-  Custom enclosure: **in design for 3D printing**

---

## Roadmap

- [ ] Finalize and deploy audio classification model on Uno Q
- [ ] Tune pan-tilt scan pattern for MLX90614 thermal sweep
- [ ] Implement LoRa node-to-node and node-to-base packet protocol
- [ ] Build ESP32 + LoRa base receiver firmware
- [ ] Build laptop dashboard (live map + alert feed)
- [ ] Design and print biomimetic field enclosure
- [ ] Field test with multiple nodes

---

*Ignis-Trace — because the best time to catch a wildfire is before it's a wildfire.*
