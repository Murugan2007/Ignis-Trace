# Ignis-Trace — v4: Industrial Deployment Reference (Hardware BOM + Architecture)

> **Status: Reference document only — not built, not costed for procurement, not part of the contest submission.**
> This describes what Ignis-Trace would look like as an actual field-deployable industrial product, with real industrial-grade components swapped in for the hobby parts used in the contest build. It exists in the repo as a scaling/roadmap reference, not a build target.

---

## 1. Why This Is a Different Document, Not Just "Better Code"

v2 and v3 upgraded the **fusion logic** (weighted rules → probabilistic fusion + fault tolerance) while keeping the same INMP441 microphone and MLX90614 pan-tilt rig. That's a legitimate software roadmap.

Going *industrial* is a different axis entirely — the hobby sensors themselves become the bottleneck, regardless of how smart the fusion logic is:

| Limitation of the hobby build | Why it blocks real deployment |
|---|---|
| MLX90614 is a single IR point, mechanically scanned | Slow (seconds per full sweep), fragile (moving servo parts in a forest for months), low resolution |
| INMP441 is a consumer breakout mic | No weatherproofing, limited directional/range performance, no beamforming |
| Uno Q's Qualcomm MPU is a consumer dev-board module | Not rated for outdoor temperature/humidity/vibration, no conformal coating, no guaranteed long-term part availability — the STM32 MCU side is fine as-is |
| Point-to-point LoRa | No standardized network management, harder to scale past a handful of nodes, no built-in device provisioning/security |
| No enclosure ingress rating | Consumer 3D-printed housing won't survive a wet season |

This document swaps each of those for the industrial-grade equivalent.

---

## 2. Bill of Materials (Industrial-Grade)

| Subsystem | Hobby Build (Contest) | Industrial Equivalent | Why |
|---|---|---|---|
| **Thermal sensing** | MLX90614 + pan-tilt servo | **FLIR Lepton 3.5** or **FLIR Boson** thermal core (radiometric, 160×120 or 320×256) | Full-frame thermal image instead of a single scanned point — no moving parts, real-time coverage, actual radiometric temperature data usable for fire-precursor detection |
| **Acoustic sensing** | INMP441 breakout | **Industrial MEMS mic array** (e.g., weatherproof beamforming array, IP-rated) or a ruggedized shotgun/directional acoustic sensor | Directional detection (bearing to threat, not just presence), weatherproof housing rated for continuous outdoor use |
| **Compute** | Arduino UNO Q (Qualcomm MPU + STM32 MCU dev board) | **Uno Q architecture retained** — STM32 real-time MCU stays as-is; only the **Qualcomm MPU module is swapped for an industrial-temp-rated / long-lifecycle equivalent SOM** (e.g. an industrial-grade Qualcomm QCS-series module on a custom carrier, in place of the consumer dev-board MPU) | Keeps the proven dual-core split (AI inference + real-time I/O) that the whole project is built around, while addressing the actual weak point of a hobby dev board: the MPU side isn't rated for outdoor temperature swings or guaranteed long-term availability. No need to redesign the MCU/sensor-interfacing side at all |
| **Node-to-node / node-to-gateway radio** | Raw point-to-point LoRa | **LoRaWAN** with a certified module (e.g., RAK/Semtech LoRaWAN stack) talking to a proper **LoRaWAN gateway** (e.g., RAK7268 or equivalent, or carrier network like The Things Network / a private gateway) | Standardized network layer: device provisioning, encryption, adaptive data rate, and the ability to scale to dozens/hundreds of nodes without a custom protocol |
| **Power** | Battery (implicit, unspecified in contest build) | **Solar panel + MPPT charge controller + LiFePO4 battery pack**, sized for worst-case winter daylight | LiFePO4 for cycle life and cold-weather tolerance; MPPT for charging efficiency; sized for weeks of no-sun autonomy |
| **Enclosure** | 3D-printed biomimetic housing | **IP66/IP67-rated enclosure**, UV-stable polymer or coated aluminum, with cable glands and a conformally coated internal PCB | Multi-year outdoor survival: rain, humidity, insects, temperature swings, UV degradation |
| **Mounting** | N/A (not specified) | Tree-strap or pole-mount bracket, tamper-resistant fasteners | Field-serviceable, resistant to wildlife interference and casual tampering |

---

## 3. Architecture Overview

```
                    ┌─────────────────────────────┐
                    │        Field Node (Uno Q architecture) │
                    │                               │
   FLIR Lepton ──►  │  STM32 MCU (unchanged)          │
   Mic Array   ──►  │  - Real-time sensor polling     │
                    │  - Servo/actuator control        │
                    │                               │
                    │  Industrial MPU/SOM (swapped-in) │
                    │  - Thermal frame inference      │
                    │  - Audio classification          │
                    │  - Sensor fusion (v3 logic,       │
                    │    scaled to real sensor data)    │
                    │                               │
                    │  LoRaWAN Radio  ─────────────┼──► LoRaWAN Gateway ──► Backend / Dashboard
                    │                               │      (cellular / satellite backhaul
                    │  Solar + MPPT + LiFePO4         │       if off-grid)
                    │  IP66/67 Enclosure               │
                    └─────────────────────────────┘
```

- **Sensor fusion logic** carries over conceptually from v3 (probabilistic/Bayesian combination, sensor health tracking, fault tolerance) — the *architecture* of v3 is still the right approach here, just fed by higher-quality sensor data and running on an industrial-rated MPU instead of the consumer one.
- The **Uno Q's dual-core split stays intact** — this isn't a full compute redesign, just an MPU-module swap to close the one real gap (temperature rating / lifecycle) in an otherwise sound architecture.

---

## 4. What Doesn't Change

- The core *concept* — bottom-up, ignition-phase detection instead of satellite smoke detection — stays identical.
- The fusion *philosophy* from v3 (probabilistic combination, don't blindly trust a single degraded sensor, corroborate before alerting) still applies; it's the right logic regardless of which physical sensors feed it.
- The mission — protecting wildlife habitats and giving investigators a forensic timeline — is unchanged.

---

## 5. Honest Caveats

- This is a **components and architecture reference**, not a validated design. Real component selection (specific Jetson SKU, specific LoRaWAN gateway model, enclosure vendor) would need actual procurement research, thermal/power budgeting, and regulatory certification (FCC/CE for the radio, IP rating verification for the enclosure) before being buildable.
- Cost is an order of magnitude or more above the hobby build — this describes what an industrial pilot or production version would require, not something achievable on a contest budget or timeline.
- No firmware is provided for this version — the v3 fusion logic is the closest conceptual starting point, but it would need to be rewritten against the new sensors' actual data formats (thermal frame vs. single IR point, array mic vs. single I2S mic).

---

*This document exists to show the deployment path beyond the contest prototype — from hobby-grade proof of concept to a hardware profile suitable for real, unattended, multi-month forest deployment.*
