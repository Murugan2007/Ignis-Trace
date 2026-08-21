/*
 * ============================================================
 *  Ignis-Trace — v1 (ORIGINAL DESIGN)
 * ============================================================
 *  This sketch reflects the ORIGINAL sensor plan for Ignis-Trace,
 *  before budget constraints forced a swap to a cheaper
 *  MLX90614 + 2-axis pan-tilt rig for thermal sensing.
 *
 *  Original thermal/spatial sensor stack:
 *    - AMG8833   : 8x8 thermal IR array camera   (full-frame heat imaging)
 *    - VL53L5CX  : 8x8 Time-of-Flight depth array (spatial/structural anomaly tracking)
 *
 *  Runs on the STM32 real-time MCU side of the Arduino UNO Q
 *  ("Real-Time Sentry" role). Acoustic classification (INMP441 +
 *  Edge Impulse) runs separately on the Qualcomm MPU/Linux side
 *  and is not part of this file.
 *
 *  Kept in-repo as a reference / fallback build for anyone who
 *  wants to reproduce the full original design with a real
 *  thermal camera instead of the budget MLX90614 substitute.
 *
 *  Libraries required:
 *    - Adafruit_AMG88xx      (Adafruit AMG8833 library)
 *    - SparkFun_VL53L5CX_Library
 *    - Wire (I2C, built-in)
 * ============================================================
 */

#include <Wire.h>
#include <Adafruit_AMG88xx.h>
#include <SparkFun_VL53L5CX_Library.h>

// ---------- Sensor objects ----------
Adafruit_AMG88xx thermalCam;
SparkFun_VL53L5CX tofSensor;
VL53L5CX_ResultsData tofData;

// ---------- Thermal frame buffer ----------
float thermalPixels[AMG88xx_PIXEL_ARRAY_SIZE];   // 8x8 = 64 values, in °C

// ---------- Thresholds (tune during field calibration) ----------
const float THERMAL_ANOMALY_THRESHOLD_C = 60.0;   // flag any pixel above this temp
const int   TOF_ANOMALY_DELTA_MM        = 150;    // flag sudden depth change vs baseline
const unsigned long POLL_INTERVAL_MS    = 250;    // sensor polling rate

// ---------- Baseline depth map for anomaly comparison ----------
int16_t baselineDepth[64];
bool baselineCaptured = false;

unsigned long lastPollTime = 0;

// ============================================================
void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  Wire.begin();

  Serial.println(F("[Ignis-Trace v1] Initializing sensors..."));

  // --- AMG8833 thermal camera ---
  if (!thermalCam.begin()) {
    Serial.println(F("[ERROR] AMG8833 not found. Check wiring/address."));
    while (1) { delay(10); }
  }
  Serial.println(F("[OK] AMG8833 thermal camera initialized."));

  // --- VL53L5CX ToF sensor ---
  if (!tofSensor.begin()) {
    Serial.println(F("[ERROR] VL53L5CX not found. Check wiring/address."));
    while (1) { delay(10); }
  }
  tofSensor.setResolution(8 * 8);      // 64-zone mode to match thermal grid
  tofSensor.setRangingFrequency(15);   // Hz
  tofSensor.startRanging();
  Serial.println(F("[OK] VL53L5CX ToF sensor initialized."));

  Serial.println(F("[Ignis-Trace v1] Capturing baseline depth map..."));
  captureBaseline();
  Serial.println(F("[Ignis-Trace v1] Ready. Monitoring for threats."));
}

// ============================================================
void loop() {
  unsigned long now = millis();
  if (now - lastPollTime < POLL_INTERVAL_MS) {
    return;
  }
  lastPollTime = now;

  bool thermalAlert = pollThermal();
  bool tofAlert      = pollToF();

  if (thermalAlert || tofAlert) {
    triggerForensicLog(thermalAlert, tofAlert);
  }
}

// ============================================================
// Reads the AMG8833 frame and checks for hotspots above threshold.
// Returns true if a thermal anomaly is detected.
bool pollThermal() {
  thermalCam.readPixels(thermalPixels);

  float maxTemp = thermalPixels[0];
  int maxIndex = 0;

  for (int i = 0; i < AMG88xx_PIXEL_ARRAY_SIZE; i++) {
    if (thermalPixels[i] > maxTemp) {
      maxTemp = thermalPixels[i];
      maxIndex = i;
    }
  }

  if (maxTemp >= THERMAL_ANOMALY_THRESHOLD_C) {
    Serial.print(F("[THERMAL ALERT] Hotspot "));
    Serial.print(maxTemp);
    Serial.print(F("C at pixel index "));
    Serial.println(maxIndex);
    return true;
  }
  return false;
}

// ============================================================
// Reads the VL53L5CX 8x8 depth grid and compares against the
// stored baseline to catch structural/physical anomalies
// (e.g. machinery or a person entering the sensor's field of view).
// Returns true if a spatial anomaly is detected.
bool pollToF() {
  if (!tofSensor.isDataReady()) {
    return false;
  }
  if (!tofSensor.getRangingData(&tofData)) {
    return false;
  }

  bool anomaly = false;
  for (int i = 0; i < 64; i++) {
    int16_t distMm = tofData.distance_mm[i];
    int16_t delta = abs(distMm - baselineDepth[i]);

    if (delta >= TOF_ANOMALY_DELTA_MM) {
      Serial.print(F("[TOF ALERT] Zone "));
      Serial.print(i);
      Serial.print(F(" depth changed by "));
      Serial.print(delta);
      Serial.println(F("mm"));
      anomaly = true;
    }
  }
  return anomaly;
}

// ============================================================
// Captures a resting-state depth map to compare future readings against.
void captureBaseline() {
  unsigned long start = millis();
  while (!tofSensor.isDataReady()) {
    if (millis() - start > 5000) {
      Serial.println(F("[WARN] Baseline capture timed out, using zeros."));
      memset(baselineDepth, 0, sizeof(baselineDepth));
      return;
    }
    delay(10);
  }

  tofSensor.getRangingData(&tofData);
  for (int i = 0; i < 64; i++) {
    baselineDepth[i] = tofData.distance_mm[i];
  }
  baselineCaptured = true;
}

// ============================================================
// Placeholder for the forensic logging trigger.
// In the full system, this hands off to the MPU/Linux side
// (Edge Impulse audio verdict + this sensor data) to write a
// timestamped log and queue a LoRa alert packet.
void triggerForensicLog(bool thermal, bool tof) {
  Serial.println(F("=== FORENSIC LOG TRIGGERED ==="));
  Serial.print(F("Timestamp (ms): "));
  Serial.println(millis());
  Serial.print(F("Thermal anomaly: "));
  Serial.println(thermal ? F("YES") : F("no"));
  Serial.print(F("ToF anomaly: "));
  Serial.println(tof ? F("YES") : F("no"));
  Serial.println(F("==============================="));

  // TODO: hand off event + sensor snapshot to MPU side for
  // logging and LoRa alert packet construction.
}
