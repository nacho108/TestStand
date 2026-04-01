#include "scale_manager.h"

#include <math.h>

#include "app_config.h"
#include "app_state.h"
#include "esc_telemetry.h"
#include "motor_control.h"

void beginScaleManager() {
    scaleDetected = scale.begin();
    if (!scaleDetected) {
        return;
    }

    scale.setSampleRate(NAU7802_SPS_320);
    scale.calibrateAFE();
    loadScaleCalibration();
}

void pollScale() {
    while (scaleDetected && scale.available()) {
        int32_t raw = scale.getReading();
        lastScaleRaw = raw;
        lastScaleWeight = rawToWeightGrams(raw);
        lastScaleStdDev = 0.0f;
        lastScaleReadMs = millis();
        lastScaleSampleValid = true;
    }
}

bool parseScaleCalibrationCommand(const String& cmd, float& outValue) {
    const char* prefix = "scale calibration ";
    if (!cmd.startsWith(prefix)) {
        return false;
    }

    String valueText = cmd.substring(strlen(prefix));
    valueText.trim();
    outValue = valueText.toFloat();
    return true;
}

void saveScaleCalibration() {
    preferences.begin("am32cli", false);
    preferences.putLong("scale_zero", (int32_t)scale.getZeroOffset());
    preferences.putFloat("scale_cal", scale.getCalibrationFactor());
    preferences.end();
}

void loadScaleCalibration() {
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
    float calFactor = scale.getCalibrationFactor();
    if (fabs(calFactor) < 0.000001f) {
        return 0.0f;
    }
    return ((float)raw - (float)scale.getZeroOffset()) / calFactor;
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

        Serial.print("  last sample age: ");
        Serial.print(millis() - lastScaleReadMs);
        Serial.println(" ms");
    } else {
        Serial.println("  last sample: none yet");
    }
}

void printScaleReading() {
    if (!scaleDetected) {
        Serial.println("Scale not detected");
        return;
    }

    Serial.println("Reading scale for 0.5 s...");

    int32_t avgRaw = 0;
    float avgWeight = 0.0f;
    float stddev = 0.0f;
    uint32_t samples = 0;

    if (!acquireAveragedScaleSample(SCALE_AVG_WINDOW_MS, avgRaw, avgWeight, stddev, samples)) {
        Serial.println("Scale data not ready yet");
        return;
    }

    Serial.print("Scale raw=");
    Serial.print(avgRaw);
    Serial.print("  weight=");
    Serial.print(avgWeight, 3);
    Serial.print(" g  samples=");
    Serial.print(samples);
    Serial.print("  stddev=");
    Serial.print(stddev, 3);
    Serial.print(" g  precision(+/-3sigma)=");
    Serial.print(3.0f * stddev, 3);
    Serial.println(" g");
}

void tareScale() {
    if (!scaleDetected) {
        Serial.println("Scale not detected");
        return;
    }

    Serial.println("Taring scale (1 second average)...");

    int32_t avgRaw = 0;
    float avgWeight = 0.0f;
    float stddev = 0.0f;
    uint32_t samples = 0;

    if (!acquireAveragedScaleSample(SCALE_TARE_WINDOW_MS, avgRaw, avgWeight, stddev, samples)) {
        Serial.println("Failed to read scale for tare");
        return;
    }

    scale.setZeroOffset(avgRaw);
    saveScaleCalibration();
    lastScaleSampleValid = false;

    Serial.print("Scale tared using ");
    Serial.print(samples);
    Serial.println(" samples");

    Serial.print("New zero offset = ");
    Serial.println(avgRaw);
}

void calibrateScale(float knownWeightGrams) {
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
    Serial.println("Averaging for 0.5 s...");

    int32_t avgRaw = 0;
    float avgWeightBefore = 0.0f;
    float stddev = 0.0f;
    uint32_t samples = 0;

    if (!acquireAveragedScaleSample(SCALE_AVG_WINDOW_MS, avgRaw, avgWeightBefore, stddev, samples)) {
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

    int32_t verifyRaw = 0;
    float verifyWeight = 0.0f;
    float verifyStdDev = 0.0f;
    uint32_t verifySamples = 0;
    if (!acquireAveragedScaleSample(SCALE_AVG_WINDOW_MS, verifyRaw, verifyWeight, verifyStdDev, verifySamples)) {
        verifyRaw = avgRaw;
        verifyWeight = rawToWeightGrams(avgRaw);
        verifyStdDev = 0.0f;
        verifySamples = 0;
    }

    Serial.print("Scale calibrated with ");
    Serial.print(knownWeightGrams, 3);
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
