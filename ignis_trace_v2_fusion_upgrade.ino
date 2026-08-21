/*
 * ============================================================
 *  Ignis-Trace — v2 (FUTURE UPGRADE — NOT BUILT FOR CONTEST DEADLINE)
 * ============================================================
 *  This sketch is a forward-looking design, kept in-repo as a
 *  roadmap item. It was NOT built or tested for the contest
 *  submission due to time constraints — it documents how
 *  decision-level sensor fusion would be added on top of the
 *  current build (INMP441 audio + MLX90614/pan-tilt thermal).
 *
 *  WHY DECISION-LEVEL FUSION (not Kalman/raw fusion):
 *    Audio and thermal here are async, low-rate, and live in
 *    completely different feature spaces (an FFT-based audio
 *    class score vs. a single-point IR temperature reading).
 *    There's no shared state to estimate and no continuous
 *    correlated signal to filter — so a Kalman filter or raw/
 *    early fusion approach is the wrong tool. Instead, each
 *    sensor branch independently produces a confidence score,
 *    and a lightweight weighted rule combines them. This is
 *    cheap to run on the STM32 MCU, easy to tune in the field,
 *    and easy to explain/justify in a write-up.
 *
 *  Runs on the STM32 real-time MCU side of the Arduino UNO Q.
 *  Audio confidence is expected to arrive over Serial (or an
 *  RPC/shared-memory channel) from the Edge Impulse model
 *  running on the Qualcomm MPU/Linux side — that link is
 *  stubbed here as readAudioConfidenceFromMPU().
 *
 *  Libraries required (for the thermal branch):
 *    - Adafruit_MLX90614
 *    - Servo (built-in)
 * ============================================================
 */

#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <Servo.h>

// ---------- Sensor / actuator objects ----------
Adafruit_MLX90614 mlx;
Servo panServo;
Servo tiltServo;

// ---------- Pan-tilt scan configuration ----------
const int PAN_MIN = 30;
const int PAN_MAX = 150;
const int TILT_MIN = 60;
const int TILT_MAX = 120;
const int SCAN_STEP_DEG = 15;
const int SETTLE_TIME_MS = 120;   // wait for servo + sensor to settle before reading

// ---------- Thermal scoring ----------
const float THERMAL_BASELINE_C = 25.0;   // rough ambient baseline, recalibrate in field
const float THERMAL_SCORE_SPAN_C = 40.0; // temp span mapped to a 0-1 score

// ---------- Fusion tuning ----------
const float AUDIO_HIGH_CONF_THRESHOLD   = 0.85;  // audio alone is enough to alert
const float AUDIO_MED_CONF_THRESHOLD    = 0.50;  // needs thermal corroboration
const float THERMAL_ANOMALY_SCORE_MIN   = 0.60;  // thermal score counted as "anomalous"
const float THERMAL_ONLY_LOG_SCORE_MIN  = 0.75;  // thermal alone -> log only, not full alert

const unsigned long FUSION_WINDOW_MS = 3000;     // audio + thermal must co-occur within this window

// ---------- State ----------
float lastAudioConfidence = 0.0;
unsigned long lastAudioTimestamp = 0;

float lastThermalScore = 0.0;
unsigned long lastThermalTimestamp = 0;

// ============================================================
void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  Wire.begin();

  if (!mlx.begin()) {
    Serial.println(F("[ERROR] MLX90614 not found. Check wiring/address."));
    while (1) { delay(10); }
  }
  Serial.println(F("[OK] MLX90614 initialized."));

  panServo.attach(9);    // adjust pins to match actual wiring
  tiltServo.attach(10);
  panServo.write((PAN_MIN + PAN_MAX) / 2);
  tiltServo.write((TILT_MIN + TILT_MAX) / 2);

  Serial.println(F("[Ignis-Trace v2] Fusion module ready (design reference, unbuilt)."));
}

// ============================================================
void loop() {
  // --- Thermal branch: pan-tilt scan, update thermal score ---
  float thermalScore = scanThermal();
  lastThermalScore = thermalScore;
  lastThermalTimestamp = millis();

  // --- Audio branch: pull latest confidence from MPU side ---
  float audioConfidence = readAudioConfidenceFromMPU();
  if (audioConfidence >= 0.0) {   // negative = no new reading this cycle
    lastAudioConfidence = audioConfidence;
    lastAudioTimestamp = millis();
  }

  // --- Fusion decision ---
  evaluateFusion();
}

// ============================================================
// Sweeps the pan-tilt rig across its range, taking an MLX90614
// point reading at each step, and returns a normalized 0-1
// "thermal anomaly score" based on the hottest point found.
float scanThermal() {
  float maxTemp = THERMAL_BASELINE_C;

  for (int pan = PAN_MIN; pan <= PAN_MAX; pan += SCAN_STEP_DEG) {
    panServo.write(pan);
    for (int tilt = TILT_MIN; tilt <= TILT_MAX; tilt += SCAN_STEP_DEG) {
      tiltServo.write(tilt);
      delay(SETTLE_TIME_MS);

      float objTemp = mlx.readObjectTempC();
      if (objTemp > maxTemp) {
        maxTemp = objTemp;
      }
    }
  }

  // Return servos to center between scans
  panServo.write((PAN_MIN + PAN_MAX) / 2);
  tiltServo.write((TILT_MIN + TILT_MAX) / 2);

  float score = (maxTemp - THERMAL_BASELINE_C) / THERMAL_SCORE_SPAN_C;
  score = constrain(score, 0.0, 1.0);

  Serial.print(F("[THERMAL] Max temp: "));
  Serial.print(maxTemp);
  Serial.print(F("C -> score: "));
  Serial.println(score);

  return score;
}

// ============================================================
// Stub for reading the latest audio classification confidence
// from the Edge Impulse model running on the MPU/Linux side.
// Real implementation would read a line over Serial/UART, or
// use a shared-memory / RPC channel between the two cores.
// Returns -1.0 if no new reading is available this cycle.
float readAudioConfidenceFromMPU() {
  if (Serial.available() > 0) {
    String line = Serial.readStringUntil('\n');
    float conf = line.toFloat();
    if (conf > 0.0) {
      Serial.print(F("[AUDIO] Confidence received: "));
      Serial.println(conf);
      return conf;
    }
  }
  return -1.0;
}

// ============================================================
// Decision-level fusion rule:
//   - High-confidence audio alone -> alert
//   - Medium-confidence audio + recent thermal anomaly -> alert
//     (corroboration catches borderline audio cases)
//   - Thermal spike alone -> low-priority log only (a single
//     IR point sensor is noisy; don't fire a full alert on it)
void evaluateFusion() {
  unsigned long now = millis();
  bool audioRecent = (now - lastAudioTimestamp) <= FUSION_WINDOW_MS;
  bool thermalRecent = (now - lastThermalTimestamp) <= FUSION_WINDOW_MS;

  if (audioRecent && lastAudioConfidence >= AUDIO_HIGH_CONF_THRESHOLD) {
    fireAlert(F("HIGH-CONFIDENCE AUDIO"), lastAudioConfidence, lastThermalScore);
    return;
  }

  if (audioRecent && thermalRecent &&
      lastAudioConfidence >= AUDIO_MED_CONF_THRESHOLD &&
      lastThermalScore >= THERMAL_ANOMALY_SCORE_MIN) {
    fireAlert(F("AUDIO + THERMAL CORROBORATION"), lastAudioConfidence, lastThermalScore);
    return;
  }

  if (thermalRecent && lastThermalScore >= THERMAL_ONLY_LOG_SCORE_MIN &&
      !(audioRecent && lastAudioConfidence >= AUDIO_MED_CONF_THRESHOLD)) {
    logOnly(F("THERMAL-ONLY (low priority)"), lastThermalScore);
  }
}

// ============================================================
void fireAlert(const __FlashStringHelper *reason, float audioConf, float thermalScore) {
  Serial.println(F("=== FUSION ALERT ==="));
  Serial.print(F("Reason: "));
  Serial.println(reason);
  Serial.print(F("Audio confidence: "));
  Serial.println(audioConf);
  Serial.print(F("Thermal score: "));
  Serial.println(thermalScore);
  Serial.println(F("====================="));

  // TODO: hand off to forensic logging + LoRa alert packet construction
}

// ============================================================
void logOnly(const __FlashStringHelper *reason, float thermalScore) {
  Serial.print(F("[LOG] "));
  Serial.print(reason);
  Serial.print(F(" - thermal score: "));
  Serial.println(thermalScore);
}
