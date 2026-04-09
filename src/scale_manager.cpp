#include "scale_manager.h"

#include <math.h>

#include "app_config.h"
#include "app_state.h"
#include "esc_telemetry.h"
#include "motor_control.h"
#include "simulation.h"

namespace {

constexpr size_t SCALE_WINDOW_CAPACITY = 256;

struct ScaleWindowSample {
    unsigned long timestampMs = 0;
    int32_t raw = 0;
    float weight = 0.0f;
};

ScaleWindowSample scaleWindow[SCALE_WINDOW_CAPACITY];
size_t scaleWindowHead = 0;
size_t scaleWindowCount = 0;
int64_t scaleWindowRawSum = 0;
double scaleWindowWeightSum = 0.0;
double scaleWindowWeightSqSum = 0.0;
bool startupAutoTarePending = false;
bool startupAutoTareCompleted = false;

void clearScaleWindow() {
    scaleWindowHead = 0;
    scaleWindowCount = 0;
    scaleWindowRawSum = 0;
    scaleWindowWeightSum = 0.0;
    scaleWindowWeightSqSum = 0.0;
    lastScaleReadMs = 0;
    lastScaleRaw = 0;
    lastScaleWeight = 0.0f;
    lastScaleWindowSampleCount = 0;
    lastScaleSampleValid = false;
    lastScaleStdDev = 0.0f;
}

void removeOldestScaleWindowSample() {
    if (scaleWindowCount == 0) {
        return;
    }

    const ScaleWindowSample& sample = scaleWindow[scaleWindowHead];
    scaleWindowRawSum -= sample.raw;
    scaleWindowWeightSum -= sample.weight;
    scaleWindowWeightSqSum -= (double)sample.weight * (double)sample.weight;
    scaleWindowHead = (scaleWindowHead + 1) % SCALE_WINDOW_CAPACITY;
    --scaleWindowCount;
}

void trimScaleWindow(unsigned long nowMs) {
    while (scaleWindowCount > 0) {
        const ScaleWindowSample& oldest = scaleWindow[scaleWindowHead];
        if (nowMs - oldest.timestampMs <= SCALE_AVG_WINDOW_MS) {
            break;
        }
        removeOldestScaleWindowSample();
    }
}

void refreshScaleWindowOutputs(unsigned long nowMs) {
    trimScaleWindow(nowMs);

    if (scaleWindowCount == 0) {
        lastScaleWindowSampleCount = 0;
        lastScaleSampleValid = false;
        lastScaleStdDev = 0.0f;
        return;
    }

    lastScaleWindowSampleCount = (uint32_t)scaleWindowCount;
    lastScaleRaw = (int32_t)(scaleWindowRawSum / (int64_t)scaleWindowCount);
    lastScaleWeight = (float)(scaleWindowWeightSum / (double)scaleWindowCount);

    double meanSq = scaleWindowWeightSqSum / (double)scaleWindowCount;
    double sqMean = (double)lastScaleWeight * (double)lastScaleWeight;
    double variance = meanSq - sqMean;
    if (variance < 0.0) {
        variance = 0.0;
    }

    lastScaleStdDev = (float)sqrt(variance);
    lastScaleSampleValid = true;
}

void pushScaleSample(int32_t raw) {
    const unsigned long nowMs = millis();
    const float weight = rawToWeightGrams(raw);

    trimScaleWindow(nowMs);
    if (scaleWindowCount == SCALE_WINDOW_CAPACITY) {
        removeOldestScaleWindowSample();
    }

    const size_t insertIndex = (scaleWindowHead + scaleWindowCount) % SCALE_WINDOW_CAPACITY;
    scaleWindow[insertIndex].timestampMs = nowMs;
    scaleWindow[insertIndex].raw = raw;
    scaleWindow[insertIndex].weight = weight;
    ++scaleWindowCount;

    scaleWindowRawSum += raw;
    scaleWindowWeightSum += weight;
    scaleWindowWeightSqSum += (double)weight * (double)weight;

    lastScaleReadMs = nowMs;
    refreshScaleWindowOutputs(nowMs);
}

}

void beginScaleManager() {
    if (simulationEnabled()) {
        clearScaleWindow();
        scaleDetected = true;
        updateSimulation();
        startupAutoTarePending = false;
        startupAutoTareCompleted = true;
        return;
    }

    scaleDetected = scale.begin();
    if (!scaleDetected) {
        clearScaleWindow();
        startupAutoTarePending = false;
        startupAutoTareCompleted = false;
        return;
    }
    scale.setLDO(NAU7802_LDO_3V0);
    scale.setGain(NAU7802_GAIN_128);
    scale.setSampleRate(NAU7802_SPS_320);
    scale.calibrateAFE();
    loadScaleCalibration();
    clearScaleWindow();
    startupAutoTarePending = true;
    startupAutoTareCompleted = false;
}

void pollScale() {
    if (simulationEnabled()) {
        updateSimulation();
        return;
    }

    while (scaleDetected && scale.available()) {
        pushScaleSample(scale.getReading());
    }

    if (scaleDetected) {
        refreshScaleWindowOutputs(millis());
    }

    if (!scaleDetected || !startupAutoTarePending || startupAutoTareCompleted) {
        return;
    }

    if (!lastScaleSampleValid) {
        return;
    }

    if (lastScaleWindowSampleCount < SCALE_STARTUP_STABLE_MIN_SAMPLES) {
        return;
    }

    if (lastScaleStdDev > SCALE_STARTUP_STABLE_STDDEV_G) {
        return;
    }

    Serial.println("Scale startup auto-tare: stable readings detected.");
    if (tareScale()) {
        startupAutoTarePending = false;
        startupAutoTareCompleted = true;
    }
}

bool parseScaleCalibrationCommand(const String& cmd, float& outValue) {
    const char* prefixes[] = {
        "scale calibration ",
        "scale cal "
    };

    for (const char* prefix : prefixes) {
        if (!cmd.startsWith(prefix)) {
            continue;
        }

        String valueText = cmd.substring(strlen(prefix));
        valueText.trim();
        outValue = valueText.toFloat();
        return true;
    }

    return false;
}

bool parseScaleFactorCommand(const String& cmd, float& outValue) {
    const char* prefix = "scale factor ";
    if (!cmd.startsWith(prefix)) {
        return false;
    }

    String valueText = cmd.substring(strlen(prefix));
    valueText.trim();
    outValue = valueText.toFloat();
    return true;
}

void saveScaleCalibration() {
    if (simulationEnabled()) {
        return;
    }

    preferences.begin("am32cli", false);
    preferences.putLong("scale_zero", (int32_t)scale.getZeroOffset());
    preferences.putFloat("scale_cal", scale.getCalibrationFactor());
    preferences.end();
}

void loadScaleCalibration() {
    if (simulationEnabled()) {
        return;
    }

    preferences.begin("am32cli", true);
    int32_t savedZero = preferences.getLong("scale_zero", DEFAULT_SCALE_ZERO_OFFSET);
    float savedCal = preferences.getFloat("scale_cal", DEFAULT_SCALE_CAL_FACTOR);
    preferences.end();

    if (scaleDetected) {
        scale.setZeroOffset(savedZero);
        scale.setCalibrationFactor(savedCal);
    }
}

float rawToWeightGrams(int32_t raw) {
    if (simulationEnabled()) {
        return (float)raw;
    }

    float calFactor = scale.getCalibrationFactor();
    if (fabs(calFactor) < 0.000001f) {
        return 0.0f;
    }
    return ((float)raw - (float)scale.getZeroOffset()) / calFactor;
}

uint32_t getScaleWindowSampleCount() {
    return lastScaleWindowSampleCount;
}

bool acquireAveragedScaleSample(
    unsigned long durationMs,
    int32_t& avgRaw,
    float& avgWeight,
    float& stddevWeight,
    uint32_t& sampleCount
) {
    avgRaw = 0;
    avgWeight = 0.0f;
    stddevWeight = 0.0f;
    sampleCount = 0;

    if (!scaleDetected) {
        return false;
    }

    if (simulationEnabled()) {
        unsigned long startMs = millis();
        while (millis() - startMs < durationMs) {
            while (Serial.available()) {
                char c = (char)Serial.read();
                if (c == 'X' || c == 'x') {
                    emergencyStopRamp();
                    return false;
                }
            }

            pollEscTelemetry();
            updateRamp();

            avgWeight += lastScaleWeight;
            avgRaw += lastScaleRaw;
            stddevWeight = SENSOR_SIMULATION_SCALE_STDDEV_G;
            sampleCount++;
            delay(10);
        }

        if (sampleCount == 0) {
            return false;
        }

        avgRaw /= (int32_t)sampleCount;
        avgWeight /= (float)sampleCount;
        lastScaleRaw = avgRaw;
        lastScaleWeight = avgWeight;
        lastScaleStdDev = stddevWeight;
        lastScaleReadMs = millis();
        lastScaleSampleValid = true;
        return true;
    }

    unsigned long startMs = millis();
    int64_t rawSum = 0;
    double weightSum = 0.0;
    double weightSqSum = 0.0;

    while (millis() - startMs < durationMs) {
        while (Serial.available()) {
            char c = (char)Serial.read();
            if (c == 'X' || c == 'x') {
                emergencyStopRamp();
                return false;
            }
        }

        pollEscTelemetry();
        updateRamp();

        while (scale.available()) {
            int32_t raw = scale.getReading();
            float weight = rawToWeightGrams(raw);
            pushScaleSample(raw);

            rawSum += raw;
            weightSum += weight;
            weightSqSum += (double)weight * (double)weight;
            sampleCount++;
        }

        delayMicroseconds(200);
    }

    if (sampleCount == 0) {
        return false;
    }

    avgRaw = (int32_t)(rawSum / (int64_t)sampleCount);
    avgWeight = (float)(weightSum / (double)sampleCount);

    double meanSq = weightSqSum / (double)sampleCount;
    double sqMean = (double)avgWeight * (double)avgWeight;
    double variance = meanSq - sqMean;
    if (variance < 0.0) {
        variance = 0.0;
    }

    stddevWeight = (float)sqrt(variance);

    lastScaleRaw = avgRaw;
    lastScaleWeight = avgWeight;
    lastScaleStdDev = stddevWeight;
    lastScaleReadMs = millis();
    lastScaleSampleValid = true;

    return true;
}

void printScaleStatus() {
    Serial.println("Scale:");

    Serial.print("  detected: ");
    Serial.println(scaleDetected ? "yes" : "no");

    if (simulationEnabled()) {
        updateSimulation();
        Serial.println("  source: simulation");
        Serial.print("  last weight: ");
        Serial.print(lastScaleWeight, 3);
        Serial.println(" g");
        Serial.print("  last stddev: ");
        Serial.print(lastScaleStdDev, 3);
        Serial.println(" g");
        Serial.print("  last sample age: ");
        Serial.print(millis() - lastScaleReadMs);
        Serial.println(" ms");
        return;
    }

    if (!scaleDetected) {
        Serial.println("  error: NAU7802 not detected");
        return;
    }

    Serial.print("  sample ready: ");
    Serial.println(scale.available() ? "yes" : "no");

    Serial.print("  zero offset: ");
    Serial.println(scale.getZeroOffset());

    Serial.print("  calibration factor: ");
    Serial.println(scale.getCalibrationFactor(), 6);

    if (lastScaleSampleValid) {
        Serial.print("  last raw reading: ");
        Serial.println(lastScaleRaw);

        Serial.print("  last weight: ");
        Serial.print(lastScaleWeight, 3);
        Serial.println(" g");

        Serial.print("  last stddev: ");
        Serial.print(lastScaleStdDev, 3);
        Serial.println(" g");

        Serial.print("  moving-average samples: ");
        Serial.println(lastScaleWindowSampleCount);

        Serial.print("  last sample age: ");
        Serial.print(millis() - lastScaleReadMs);
        Serial.println(" ms");
    } else {
        Serial.println("  last sample: none yet");
    }
}

void printScaleReading() {
    if (simulationEnabled()) {
        updateSimulation();
        Serial.print("Scale simulated weight=");
        Serial.print(lastScaleWeight, 3);
        Serial.print(" g  stddev=");
        Serial.print(lastScaleStdDev, 3);
        Serial.println(" g");
        return;
    }

    if (!scaleDetected) {
        Serial.println("Scale not detected");
        return;
    }

    pollScale();
    if (!lastScaleSampleValid) {
        Serial.println("Scale data not ready yet");
        return;
    }

    Serial.print("Scale raw=");
    Serial.print(lastScaleRaw);
    Serial.print("  weight=");
    Serial.print(lastScaleWeight, 3);
    Serial.print(" g  samples=");
    Serial.print(lastScaleWindowSampleCount);
    Serial.print("  stddev=");
    Serial.print(lastScaleStdDev, 3);
    Serial.print(" g  precision(+/-3sigma)=");
    Serial.print(3.0f * lastScaleStdDev, 3);
    Serial.print(" g  window=");
    Serial.print(SCALE_AVG_WINDOW_MS);
    Serial.println(" ms");
}

bool tareScale() {
    if (simulationEnabled()) {
        Serial.println("Scale tare is not available in simulation mode");
        return false;
    }

    if (!scaleDetected) {
        Serial.println("Scale not detected");
        return false;
    }

    Serial.println("Taring scale (1 second average)...");

    int32_t avgRaw = 0;
    float avgWeight = 0.0f;
    float stddev = 0.0f;
    uint32_t samples = 0;

    if (!acquireAveragedScaleSample(SCALE_TARE_WINDOW_MS, avgRaw, avgWeight, stddev, samples)) {
        Serial.println("Failed to read scale for tare");
        return false;
    }

    scale.setZeroOffset(avgRaw);
    saveScaleCalibration();
    clearScaleWindow();
    lastScaleSampleValid = false;

    Serial.print("Scale tared using ");
    Serial.print(samples);
    Serial.print(" samples");
    Serial.print("  stddev=");
    Serial.print(stddev, 3);
    Serial.println(" g");

    Serial.print("New zero offset = ");
    Serial.println(avgRaw);
    return true;
}

void calibrateScale(float knownWeightGrams) {
    if (simulationEnabled()) {
        Serial.println("Scale calibration is not available in simulation mode");
        return;
    }

    if (!scaleDetected) {
        Serial.println("Scale not detected");
        return;
    }

    if (knownWeightGrams <= 0.0f) {
        Serial.println("Calibration weight must be > 0 grams");
        return;
    }

    Serial.print("Calibrating scale with ");
    Serial.print(knownWeightGrams, 3);
    Serial.println(" g...");
    Serial.println("Averaging for 1.0 s...");

    int32_t avgRaw = 0;
    float avgWeightBefore = 0.0f;
    float stddev = 0.0f;
    uint32_t samples = 0;

    if (!acquireAveragedScaleSample(SCALE_CAL_WINDOW_MS, avgRaw, avgWeightBefore, stddev, samples)) {
        Serial.println("Scale data not ready");
        return;
    }

    int32_t zeroOffset = (int32_t)scale.getZeroOffset();
    int32_t delta = avgRaw - zeroOffset;

    if (delta == 0) {
        Serial.println("Calibration failed: averaged raw reading equals zero offset");
        return;
    }

    float newCalFactor = (float)delta / knownWeightGrams;
    scale.setCalibrationFactor(newCalFactor);
    saveScaleCalibration();
    clearScaleWindow();

    int32_t verifyRaw = 0;
    float verifyWeight = 0.0f;
    float verifyStdDev = 0.0f;
    uint32_t verifySamples = 0;
    if (!acquireAveragedScaleSample(SCALE_CAL_WINDOW_MS, verifyRaw, verifyWeight, verifyStdDev, verifySamples)) {
        verifyRaw = avgRaw;
        verifyWeight = rawToWeightGrams(avgRaw);
        verifyStdDev = 0.0f;
        verifySamples = 0;
    }

    Serial.print("Scale calibrated with ");
    Serial.print(knownWeightGrams, 3);
    Serial.print(" g");
    Serial.print("  stddev=");
    Serial.print(stddev, 3);
    Serial.println(" g");

    Serial.print("  averaged raw reading: ");
    Serial.println(avgRaw);

    Serial.print("  zero offset: ");
    Serial.println(zeroOffset);

    Serial.print("  delta: ");
    Serial.println(delta);

    Serial.print("  new calibration factor: ");
    Serial.println(newCalFactor, 6);

    Serial.print("  verified weight: ");
    Serial.print(verifyWeight, 3);
    Serial.println(" g");
}

bool setScaleCalibrationFactor(float newFactor) {
    if (simulationEnabled()) {
        Serial.println("Scale calibration factor is not available in simulation mode");
        return false;
    }

    if (!scaleDetected) {
        Serial.println("Scale not detected");
        return false;
    }

    if (fabs(newFactor) < 0.000001f) {
        Serial.println("Scale calibration factor must be non-zero");
        return false;
    }

    scale.setCalibrationFactor(newFactor);
    saveScaleCalibration();
    clearScaleWindow();

    Serial.print("Scale calibration factor set to ");
    Serial.println(newFactor, 6);
    return true;
}
