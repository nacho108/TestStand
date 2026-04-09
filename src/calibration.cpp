#include "calibration.h"

#include <math.h>

#include "app_config.h"
#include "app_state.h"
#include "esc_telemetry.h"

namespace {

constexpr unsigned long kCalibrationCaptureTimeoutMs = 1500;
constexpr unsigned long kCalibrationCaptureWindowMs = 250;
constexpr size_t kCalibrationCaptureMinSamples = 3;

bool waitForTelemetryAverage(float (*readRawValue)(), float& outRawValue, size_t& outSampleCount) {
    const unsigned long startMs = millis();
    unsigned long firstSampleMs = 0;
    unsigned long lastCapturedUpdateMs = lastTlm.lastUpdateMs;
    float sum = 0.0f;
    size_t sampleCount = 0;

    while (millis() - startMs < kCalibrationCaptureTimeoutMs) {
        pollEscTelemetry();

        if (!lastTlm.valid) {
            delay(5);
            continue;
        }

        if (lastTlm.lastUpdateMs == lastCapturedUpdateMs) {
            delay(5);
            continue;
        }

        lastCapturedUpdateMs = lastTlm.lastUpdateMs;
        if (sampleCount == 0) {
            firstSampleMs = millis();
        }

        sum += readRawValue();
        ++sampleCount;

        if (sampleCount >= kCalibrationCaptureMinSamples
            && millis() - firstSampleMs >= kCalibrationCaptureWindowMs) {
            outRawValue = sum / (float)sampleCount;
            outSampleCount = sampleCount;
            return true;
        }
    }

    if (sampleCount > 0) {
        outRawValue = sum / (float)sampleCount;
        outSampleCount = sampleCount;
        return true;
    }

    return false;
}

void applySinglePointCalibration(LinearCalibration& cal, float rawValue, float realValue) {
    cal.offset = realValue - rawValue * cal.scale;
}

void printPendingCalibrationState(const char* label, const LinearCalibration& cal) {
    Serial.print(label);
    Serial.print(" low=");
    if (cal.lowCaptured) {
        Serial.print(cal.lowRaw, 4);
        Serial.print("->");
        Serial.print(cal.lowReal, 4);
    } else {
        Serial.print("not set");
    }

    Serial.print(" high=");
    if (cal.highCaptured) {
        Serial.print(cal.highRaw, 4);
        Serial.print("->");
        Serial.print(cal.highReal, 4);
    } else {
        Serial.print("not set");
    }
    Serial.println();
}

}

float getRawVoltageVolts() {
    return lastTlm.voltageRaw / 100.0f;
}

float getRawCurrentAmps() {
    return lastTlm.currentRaw / 100.0f;
}

float applyCalibration(float rawValue, const LinearCalibration& cal) {
    return rawValue * cal.scale + cal.offset;
}

float getCalibratedVoltageVolts() {
    return applyCalibration(getRawVoltageVolts(), voltageCal);
}

float getCalibratedCurrentAmps() {
    return applyCalibration(getRawCurrentAmps(), currentCal);
}

float getCalibratedPowerWatts() {
    return getCalibratedVoltageVolts() * getCalibratedCurrentAmps();
}

bool recomputeCalibration(LinearCalibration& cal) {
    float dx = cal.highRaw - cal.lowRaw;
    if (fabs(dx) < 0.000001f) {
        return false;
    }

    cal.scale = (cal.highReal - cal.lowReal) / dx;
    cal.offset = cal.lowReal - cal.lowRaw * cal.scale;
    return true;
}

void setCalibrationDefaults() {
    voltageCal.scale = DEFAULT_VOLTAGE_SCALE;
    voltageCal.offset = DEFAULT_VOLTAGE_OFFSET;
    voltageCal.lowCaptured = DEFAULT_VOLTAGE_LOW_CAPTURED;
    voltageCal.highCaptured = DEFAULT_VOLTAGE_HIGH_CAPTURED;
    voltageCal.lowRaw = DEFAULT_VOLTAGE_LOW_RAW;
    voltageCal.lowReal = DEFAULT_VOLTAGE_LOW_REAL;
    voltageCal.highRaw = DEFAULT_VOLTAGE_HIGH_RAW;
    voltageCal.highReal = DEFAULT_VOLTAGE_HIGH_REAL;

    currentCal.scale = DEFAULT_CURRENT_SCALE;
    currentCal.offset = DEFAULT_CURRENT_OFFSET;
    currentCal.lowCaptured = DEFAULT_CURRENT_LOW_CAPTURED;
    currentCal.highCaptured = DEFAULT_CURRENT_HIGH_CAPTURED;
    currentCal.lowRaw = DEFAULT_CURRENT_LOW_RAW;
    currentCal.lowReal = DEFAULT_CURRENT_LOW_REAL;
    currentCal.highRaw = DEFAULT_CURRENT_HIGH_RAW;
    currentCal.highReal = DEFAULT_CURRENT_HIGH_REAL;
}

void saveCalibration() {
    preferences.begin("am32cli", false);

    preferences.putFloat("v_scale", voltageCal.scale);
    preferences.putFloat("v_offset", voltageCal.offset);
    preferences.putBool("v_low_cap", voltageCal.lowCaptured);
    preferences.putBool("v_high_cap", voltageCal.highCaptured);
    preferences.putFloat("v_low_raw", voltageCal.lowRaw);
    preferences.putFloat("v_low_real", voltageCal.lowReal);
    preferences.putFloat("v_high_raw", voltageCal.highRaw);
    preferences.putFloat("v_high_real", voltageCal.highReal);

    preferences.putFloat("i_scale", currentCal.scale);
    preferences.putFloat("i_offset", currentCal.offset);
    preferences.putBool("i_low_cap", currentCal.lowCaptured);
    preferences.putBool("i_high_cap", currentCal.highCaptured);
    preferences.putFloat("i_low_raw", currentCal.lowRaw);
    preferences.putFloat("i_low_real", currentCal.lowReal);
    preferences.putFloat("i_high_raw", currentCal.highRaw);
    preferences.putFloat("i_high_real", currentCal.highReal);

    preferences.end();
}

void loadCalibration() {
    preferences.begin("am32cli", true);

    voltageCal.scale = preferences.getFloat("v_scale", DEFAULT_VOLTAGE_SCALE);
    voltageCal.offset = preferences.getFloat("v_offset", DEFAULT_VOLTAGE_OFFSET);
    voltageCal.lowCaptured = preferences.getBool("v_low_cap", DEFAULT_VOLTAGE_LOW_CAPTURED);
    voltageCal.highCaptured = preferences.getBool("v_high_cap", DEFAULT_VOLTAGE_HIGH_CAPTURED);
    voltageCal.lowRaw = preferences.getFloat("v_low_raw", DEFAULT_VOLTAGE_LOW_RAW);
    voltageCal.lowReal = preferences.getFloat("v_low_real", DEFAULT_VOLTAGE_LOW_REAL);
    voltageCal.highRaw = preferences.getFloat("v_high_raw", DEFAULT_VOLTAGE_HIGH_RAW);
    voltageCal.highReal = preferences.getFloat("v_high_real", DEFAULT_VOLTAGE_HIGH_REAL);

    currentCal.scale = preferences.getFloat("i_scale", DEFAULT_CURRENT_SCALE);
    currentCal.offset = preferences.getFloat("i_offset", DEFAULT_CURRENT_OFFSET);
    currentCal.lowCaptured = preferences.getBool("i_low_cap", DEFAULT_CURRENT_LOW_CAPTURED);
    currentCal.highCaptured = preferences.getBool("i_high_cap", DEFAULT_CURRENT_HIGH_CAPTURED);
    currentCal.lowRaw = preferences.getFloat("i_low_raw", DEFAULT_CURRENT_LOW_RAW);
    currentCal.lowReal = preferences.getFloat("i_low_real", DEFAULT_CURRENT_LOW_REAL);
    currentCal.highRaw = preferences.getFloat("i_high_raw", DEFAULT_CURRENT_HIGH_RAW);
    currentCal.highReal = preferences.getFloat("i_high_real", DEFAULT_CURRENT_HIGH_REAL);

    preferences.end();
}

void resetCalibration() {
    setCalibrationDefaults();
    saveCalibration();
}

void printCalibrationStatus() {
    Serial.println("Calibration:");
    Serial.print("  Voltage scale=");
    Serial.print(voltageCal.scale, 6);
    Serial.print(" offset=");
    Serial.println(voltageCal.offset, 6);
    printPendingCalibrationState("  Voltage points:", voltageCal);

    Serial.print("  Current scale=");
    Serial.print(currentCal.scale, 6);
    Serial.print(" offset=");
    Serial.println(currentCal.offset, 6);
    printPendingCalibrationState("  Current points:", currentCal);
}

bool parseCalibrationCommand(const String& cmd, const char* prefix, float& outValue) {
    String command = cmd;
    command.trim();

    String prefixText = prefix;
    prefixText.trim();

    String commandLower = command;
    commandLower.toLowerCase();
    String prefixLower = prefixText;
    prefixLower.toLowerCase();

    if (!commandLower.startsWith(prefixLower)) {
        return false;
    }

    String valueText = command.substring(prefixText.length());
    valueText.trim();
    if (valueText.length() == 0) {
        return false;
    }

    bool hasDigit = false;
    bool hasDecimalPoint = false;
    for (size_t i = 0; i < valueText.length(); ++i) {
        const char c = valueText.charAt(i);
        if (c >= '0' && c <= '9') {
            hasDigit = true;
            continue;
        }

        if (c == '.' && !hasDecimalPoint) {
            hasDecimalPoint = true;
            continue;
        }

        if (i == 0 && (c == '+' || c == '-')) {
            continue;
        }

        return false;
    }

    if (!hasDigit) {
        return false;
    }

    outValue = valueText.toFloat();
    return true;
}

void captureVoltageLow(float realValue) {
    if (!lastTlm.valid) {
        Serial.println("No valid telemetry yet");
        return;
    }

    float capturedRaw = 0.0f;
    size_t sampleCount = 0;
    if (!waitForTelemetryAverage(getRawVoltageVolts, capturedRaw, sampleCount)) {
        Serial.println("Voltage calibration failed: no fresh telemetry samples captured");
        return;
    }

    voltageCal.lowRaw = capturedRaw;
    voltageCal.lowReal = realValue;
    voltageCal.lowCaptured = true;
    applySinglePointCalibration(voltageCal, voltageCal.lowRaw, voltageCal.lowReal);
    saveCalibration();

    Serial.print("Voltage low captured: raw=");
    Serial.print(voltageCal.lowRaw, 4);
    Serial.print(" real=");
    Serial.print(voltageCal.lowReal, 4);
    Serial.print(" samples=");
    Serial.println(sampleCount);

    if (voltageCal.highCaptured) {
        if (recomputeCalibration(voltageCal)) {
            saveCalibration();
            Serial.println("Voltage calibration updated");
        } else {
            Serial.println("Voltage calibration failed: low/high raw are identical");
        }
    } else {
        Serial.println("Voltage offset updated from the low point. Capture the high point to refine scale.");
    }
}

void captureVoltageHigh(float realValue) {
    if (!lastTlm.valid) {
        Serial.println("No valid telemetry yet");
        return;
    }

    float capturedRaw = 0.0f;
    size_t sampleCount = 0;
    if (!waitForTelemetryAverage(getRawVoltageVolts, capturedRaw, sampleCount)) {
        Serial.println("Voltage calibration failed: no fresh telemetry samples captured");
        return;
    }

    voltageCal.highRaw = capturedRaw;
    voltageCal.highReal = realValue;
    voltageCal.highCaptured = true;
    applySinglePointCalibration(voltageCal, voltageCal.highRaw, voltageCal.highReal);
    saveCalibration();

    Serial.print("Voltage high captured: raw=");
    Serial.print(voltageCal.highRaw, 4);
    Serial.print(" real=");
    Serial.print(voltageCal.highReal, 4);
    Serial.print(" samples=");
    Serial.println(sampleCount);

    if (voltageCal.lowCaptured) {
        if (recomputeCalibration(voltageCal)) {
            saveCalibration();
            Serial.println("Voltage calibration updated");
        } else {
            Serial.println("Voltage calibration failed: low/high raw are identical");
        }
    } else {
        Serial.println("Voltage offset updated from the high point. Capture the low point to refine scale.");
    }
}

void captureCurrentLow(float realValue) {
    if (!lastTlm.valid) {
        Serial.println("No valid telemetry yet");
        return;
    }

    float capturedRaw = 0.0f;
    size_t sampleCount = 0;
    if (!waitForTelemetryAverage(getRawCurrentAmps, capturedRaw, sampleCount)) {
        Serial.println("Current calibration failed: no fresh telemetry samples captured");
        return;
    }

    currentCal.lowRaw = capturedRaw;
    currentCal.lowReal = realValue;
    currentCal.lowCaptured = true;
    applySinglePointCalibration(currentCal, currentCal.lowRaw, currentCal.lowReal);
    saveCalibration();

    Serial.print("Current low captured: raw=");
    Serial.print(currentCal.lowRaw, 4);
    Serial.print(" real=");
    Serial.print(currentCal.lowReal, 4);
    Serial.print(" samples=");
    Serial.println(sampleCount);

    if (currentCal.highCaptured) {
        if (recomputeCalibration(currentCal)) {
            saveCalibration();
            Serial.println("Current calibration updated");
        } else {
            Serial.println("Current calibration failed: low/high raw are identical");
        }
    } else {
        Serial.println("Current offset updated from the low point. Capture the high point to refine scale.");
    }
}

void captureCurrentHigh(float realValue) {
    if (!lastTlm.valid) {
        Serial.println("No valid telemetry yet");
        return;
    }

    float capturedRaw = 0.0f;
    size_t sampleCount = 0;
    if (!waitForTelemetryAverage(getRawCurrentAmps, capturedRaw, sampleCount)) {
        Serial.println("Current calibration failed: no fresh telemetry samples captured");
        return;
    }

    currentCal.highRaw = capturedRaw;
    currentCal.highReal = realValue;
    currentCal.highCaptured = true;
    applySinglePointCalibration(currentCal, currentCal.highRaw, currentCal.highReal);
    saveCalibration();

    Serial.print("Current high captured: raw=");
    Serial.print(currentCal.highRaw, 4);
    Serial.print(" real=");
    Serial.print(currentCal.highReal, 4);
    Serial.print(" samples=");
    Serial.println(sampleCount);

    if (currentCal.lowCaptured) {
        if (recomputeCalibration(currentCal)) {
            saveCalibration();
            Serial.println("Current calibration updated");
        } else {
            Serial.println("Current calibration failed: low/high raw are identical");
        }
    } else {
        Serial.println("Current offset updated from the high point. Capture the low point to refine scale.");
    }
}
