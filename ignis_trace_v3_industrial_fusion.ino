/*
 * ============================================================
 *  Ignis-Trace — v3 (INDUSTRIAL-GRADE REFERENCE ARCHITECTURE)
 *  NOT BUILT — DESIGN REFERENCE ONLY, FOR FUTURE SCALING
 * ============================================================
 *  v2 showed simple decision-level (weighted-rule) fusion.
 *  This file sketches what an actual industry-deployable
 *  version of that fusion layer would need to look like if
 *  Ignis-Trace nodes were meant to run unattended in a forest
 *  for months, not on a bench for a contest demo.
 *
 *  The hobby-grade version answers "did we see a threat?".
 *  An industrial version has to also answer "do we trust the
 *  sensor that told us that?" — which is the actual gap
 *  between v2 and this.
 *
 *  WHAT CHANGES FROM v2, AND WHY:
 *
 *  1. PROBABILISTIC FUSION, NOT FIXED THRESHOLDS
 *     v2 used hard-coded confidence cutoffs (0.85, 0.60, etc).
 *     That's fine on a bench, but drifts badly in the field as
 *     sensors age, temperature swings, or dust/moisture changes
 *     baseline readings. A real deployment needs each sensor to
 *     report a *probability* of threat, updated with a simple
 *     Bayesian combination (P(threat | audio, thermal) instead
 *     of "if A > 0.85 OR (A > 0.5 AND B > 0.6)"). This adapts
 *     naturally as confidence in either sensor rises or falls,
 *     rather than needing hand-tuned constants re-tuned per site.
 *
 *  2. SENSOR HEALTH / SELF-DIAGNOSTICS
 *     A field node has no engineer nearby to notice a dead mic
 *     or a servo that's stopped sweeping. Industrial fusion has
 *     to detect a failed/degraded sensor and stop trusting it
 *     (rather than silently fusing garbage data), and report its
 *     own health status over LoRa so the base station knows a
 *     node needs maintenance.
 *
 *  3. AUTOMATIC BASELINE RECALIBRATION
 *     Ambient temperature and noise floor drift with weather and
 *     season. A fixed THERMAL_BASELINE_C (like in v2) goes stale.
 *     This version periodically recalibrates against a rolling
 *     window of "quiet" readings, and persists calibration to
 *     non-volatile storage so it survives power loss/reboot.
 *
 *  4. FAULT-TOLERANT COMMUNICATION
 *     v1/v2 assumed the LoRa link just works. A real node needs
 *     retry-with-backoff, and to queue/store alerts locally if
 *     the base receiver is briefly unreachable, so events aren't
 *     silently dropped.
 *
 *  5. WATCHDOG / FAIL-SAFE OPERATION
 *     If the fusion loop hangs (sensor I2C bus lockup, servo
 *     stall, etc), an unattended node should self-reset rather
 *     than going dark for months. A hardware watchdog timer is
 *     included.
 *
 *  This is intentionally written as an architecture/reference
 *  sketch — a starting skeleton with the key structures and
 *  comments explaining the reasoning, not a finished, tested
 *  product. Treat function bodies marked TODO as exactly that.
 * ============================================================
 */

#include <Wire.h>
#include <EEPROM.h>
#include <avr/wdt.h>

// ------------------------------------------------------------
// Sensor confidence/health model
// ------------------------------------------------------------
struct SensorReading {
  float value;          // raw normalized reading (0-1 space)
  float confidence;      // 0-1, how much we currently trust this sensor
  unsigned long timestamp;
  bool healthy;
};

SensorReading audioReading  = {0, 1.0, 0, true};
SensorReading thermalReading = {0, 1.0, 0, true};

// ------------------------------------------------------------
// Bayesian-style fusion priors (industry approach: treat each
// sensor as independent evidence, combine via simple naive-Bayes
// odds update rather than fixed AND/OR thresholds)
// ------------------------------------------------------------
const float PRIOR_THREAT_PROBABILITY = 0.02;  // baseline rate of real threats
// Likelihood ratios: how much more likely is this reading given
// a real threat vs. given normal conditions. Calibrate from field
// data / labeled test runs over time; these are starting guesses.
const float AUDIO_LIKELIHOOD_RATIO_HIGH   = 12.0;
const float AUDIO_LIKELIHOOD_RATIO_MED    = 3.0;
const float THERMAL_LIKELIHOOD_RATIO_HIGH = 5.0;

const float ALERT_POSTERIOR_THRESHOLD = 0.75; // fire alert above this posterior probability

// ------------------------------------------------------------
// Calibration storage (persisted to EEPROM so it survives reboot)
// ------------------------------------------------------------
struct CalibrationData {
  float thermalBaselineC;
  float audioNoiseFloor;
  uint32_t lastCalibratedEpoch;
  uint16_t checksum;
};
const int EEPROM_CAL_ADDR = 0;
CalibrationData calData;

const unsigned long RECAL_INTERVAL_MS = 6UL * 60UL * 60UL * 1000UL; // every 6 hours
unsigned long lastRecalTime = 0;

// ------------------------------------------------------------
// Comms reliability
// ------------------------------------------------------------
const int MAX_LORA_RETRIES = 3;
const unsigned long LORA_RETRY_BASE_MS = 500; // exponential backoff base

struct QueuedAlert {
  float posteriorProbability;
  unsigned long timestamp;
  bool sent;
};
const int ALERT_QUEUE_SIZE = 8;
QueuedAlert alertQueue[ALERT_QUEUE_SIZE];
int alertQueueHead = 0;

// ============================================================
void setup() {
  Serial.begin(115200);
  Wire.begin();

  loadCalibration();
  initWatchdog();

  Serial.println(F("[Ignis-Trace v3] Industrial fusion reference booting."));
  Serial.println(F("[NOTE] This is a design skeleton — not field-validated."));

  // TODO: init sensors (INMP441 pipeline handoff from MPU, MLX90614 + servos)
  // TODO: init LoRa radio
}

// ============================================================
void loop() {
  wdt_reset();  // pet the watchdog every cycle

  updateSensorHealth();

  // TODO: replace with real reads
  // audioReading = readAudioFromMPU();
  // thermalReading = scanThermalWithHealthCheck();

  float posterior = computeFusionPosterior(audioReading, thermalReading);

  if (posterior >= ALERT_POSTERIOR_THRESHOLD) {
    enqueueAlert(posterior);
  }

  flushAlertQueue();

  maybeRecalibrate();
}

// ============================================================
// Core fusion: naive-Bayes-style odds update instead of fixed
// thresholds. Each sensor contributes a likelihood ratio based
// on which confidence "bucket" its reading falls into; ratios
// combine multiplicatively (independence assumption — reasonable
// here since audio and thermal are physically unrelated channels).
float computeFusionPosterior(const SensorReading &audio, const SensorReading &thermal) {
  float priorOdds = PRIOR_THREAT_PROBABILITY / (1.0 - PRIOR_THREAT_PROBABILITY);
  float odds = priorOdds;

  if (audio.healthy) {
    if (audio.value >= 0.85) {
      odds *= AUDIO_LIKELIHOOD_RATIO_HIGH;
    } else if (audio.value >= 0.50) {
      odds *= AUDIO_LIKELIHOOD_RATIO_MED;
    }
    // Weight contribution by our current trust in this sensor
    odds = 1.0 + (odds - 1.0) * audio.confidence;
  }

  if (thermal.healthy) {
    if (thermal.value >= 0.60) {
      odds *= THERMAL_LIKELIHOOD_RATIO_HIGH;
    }
    odds = 1.0 + (odds - 1.0) * thermal.confidence;
  }

  float posterior = odds / (1.0 + odds);
  return posterior;
}

// ============================================================
// Sensor health monitoring: detect stale, stuck, or out-of-range
// readings and downgrade confidence rather than trusting blindly.
// A degraded sensor should reduce its own influence on fusion,
// not get silently excluded (which could mask a real fault) or
// silently trusted (which could cause false alerts/misses).
void updateSensorHealth() {
  unsigned long now = millis();

  // Staleness check
  if (now - audioReading.timestamp > 10000) {
    audioReading.confidence = max(0.0f, audioReading.confidence - 0.1f);
  }
  if (now - thermalReading.timestamp > 10000) {
    thermalReading.confidence = max(0.0f, thermalReading.confidence - 0.1f);
  }

  audioReading.healthy   = audioReading.confidence > 0.2;
  thermalReading.healthy = thermalReading.confidence > 0.2;

  // TODO: also check for stuck-value faults (same reading N times
  // in a row when environment should be changing), and physical
  // faults (e.g. servo not reaching commanded position).

  if (!audioReading.healthy || !thermalReading.healthy) {
    reportHealthDegradation();
  }
}

// ============================================================
void reportHealthDegradation() {
  Serial.println(F("[HEALTH] Sensor confidence degraded — see status packet."));
  // TODO: include in periodic LoRa status/heartbeat packet so
  // the base station dashboard can flag this node for maintenance.
}

// ============================================================
// Periodically recalibrates baselines against recent "quiet"
// conditions (no active alert) and persists to EEPROM so
// calibration survives power loss. Real implementation would
// average a rolling buffer of recent readings during confirmed-
// quiet periods rather than just re-sampling once.
void maybeRecalibrate() {
  unsigned long now = millis();
  if (now - lastRecalTime < RECAL_INTERVAL_MS) {
    return;
  }
  lastRecalTime = now;

  // TODO: only recalibrate if no recent alert (don't learn a
  // fire as the new "normal")
  // calData.thermalBaselineC = rollingQuietAverageThermal();
  // calData.audioNoiseFloor  = rollingQuietAverageAudio();

  saveCalibration();
  Serial.println(F("[CAL] Recalibration cycle complete, saved to EEPROM."));
}

void loadCalibration() {
  EEPROM.get(EEPROM_CAL_ADDR, calData);
  uint16_t expected = computeChecksum(calData);
  if (calData.checksum != expected) {
    Serial.println(F("[CAL] No valid calibration found, using defaults."));
    calData.thermalBaselineC = 25.0;
    calData.audioNoiseFloor = 0.0;
    calData.lastCalibratedEpoch = 0;
    saveCalibration();
  }
}

void saveCalibration() {
  calData.checksum = computeChecksum(calData);
  EEPROM.put(EEPROM_CAL_ADDR, calData);
}

uint16_t computeChecksum(const CalibrationData &d) {
  // Simple checksum placeholder — swap for CRC16 in production
  uint16_t sum = 0;
  sum += (uint16_t)(d.thermalBaselineC * 100);
  sum += (uint16_t)(d.audioNoiseFloor * 100);
  sum += (uint16_t)(d.lastCalibratedEpoch & 0xFFFF);
  return sum;
}

// ============================================================
// Alert queue: buffer alerts locally so a temporarily
// unreachable base station doesn't mean a lost event.
void enqueueAlert(float posterior) {
  alertQueue[alertQueueHead] = { posterior, millis(), false };
  alertQueueHead = (alertQueueHead + 1) % ALERT_QUEUE_SIZE;

  Serial.print(F("[FUSION ALERT] Posterior threat probability: "));
  Serial.println(posterior, 3);
}

void flushAlertQueue() {
  for (int i = 0; i < ALERT_QUEUE_SIZE; i++) {
    if (!alertQueue[i].sent && alertQueue[i].timestamp > 0) {
      if (sendAlertWithRetry(alertQueue[i])) {
        alertQueue[i].sent = true;
      }
    }
  }
}

// ============================================================
// LoRa send with exponential backoff retry. Returns true on
// confirmed delivery (ACK), false if all retries exhausted —
// caller keeps the alert queued to retry again next cycle.
bool sendAlertWithRetry(const QueuedAlert &alert) {
  for (int attempt = 0; attempt < MAX_LORA_RETRIES; attempt++) {
    // TODO: replace with actual LoRa send + ACK wait
    bool ackReceived = false; // = loRaSendAndWaitForAck(alert);

    if (ackReceived) {
      return true;
    }

    unsigned long backoff = LORA_RETRY_BASE_MS * (1UL << attempt); // 500, 1000, 2000...
    delay(backoff);
  }
  return false;
}

// ============================================================
// Hardware watchdog: resets the MCU if the main loop hangs
// (e.g. I2C bus lockup from a stalled sensor), so an unattended
// field node doesn't go permanently dark on a transient fault.
void initWatchdog() {
  wdt_enable(WDTO_8S);
}
