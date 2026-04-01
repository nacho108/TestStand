#include "calibration.h"

#include <math.h>

#include "app_config.h"
#include "app_state.h"

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
    voltageCal.lowCaptured = false;
    voltageCal.highCaptured = false;
    voltageCal.lowRaw = 0.0f;
    voltageCal.lowReal = 0.0f;
    voltageCal.highRaw = 0.0f;
    voltageCal.highReal = 0.0f;

    currentCal.scale = DEFAULT_CURRENT_SCALE;
    currentCal.offset = DEFAULT_CURRENT_OFFSET;
    currentCal.lowCaptured = false;
    currentCal.highCaptured = false;
    currentCal.lowRaw = 0.0f;
    currentCal.lowReal = 0.0f;
    currentCal.highRaw = 0.0f;
    currentCal.highReal = 0.0f;
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
    voltageCal.lowCaptured = preferences.getBool("v_low_cap", false);
    voltageCal.highCaptured = preferences.getBool("v_high_cap", false);
    voltageCal.lowRaw = preferences.getFloat("v_low_raw", 0.0f);
    voltageCal.lowReal = preferences.getFloat("v_low_real", 0.0f);
    voltageCal.highRaw = preferences.getFloat("v_high_raw", 0.0f);
    voltageCal.highReal = preferences.getFloat("v_high_real", 0.0f);

    currentCal.scale = preferences.getFloat("i_scale", DEFAULT_CURRENT_SCALE);
    currentCal.offset = preferences.getFloat("i_offset", DEFAULT_CURRENT_OFFSET);
    currentCal.lowCaptured = preferences.getBool("i_low_cap", false);
    currentCal.highCaptured = preferences.getBool("i_high_cap", false);
    currentCal.lowRaw = preferences.getFloat("i_low_raw", 0.0f);
    currentCal.lowReal = preferences.getFloat("i_low_real", 0.0f);
    currentCal.highRaw = preferences.getFloat("i_high_raw", 0.0f);
    currentCal.highReal = preferences.getFloat("i_high_real", 0.0f);

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

    Serial.print("  Current scale=");
    Serial.print(currentCal.scale, 6);
    Serial.print(" offset=");
    Serial.println(currentCal.offset, 6);
}

bool parseCalibrationCommand(const String& cmd, const char* prefix, float& outValue) {
    String p = prefix;
    if (!cmd.startsWith(p)) {
        return false;
    }

    String valueText = cmd.substring(p.length());
    valueText.trim();
    outValue = valueText.toFloat();
    return true;
}

void captureVoltageLow(float realValue) {
    if (!lastTlm.valid) {
        Serial.println("No valid telemetry yet");
        return;
    }

    voltageCal.lowRaw = getRawVoltageVolts();
    voltageCal.lowReal = realValue;
    voltageCal.lowCaptured = true;
    saveCalibration();

    Serial.print("Voltage low captured: raw=");
    Serial.print(voltageCal.lowRaw, 4);
    Serial.print(" real=");
    Serial.println(voltageCal.lowReal, 4);

    if (voltageCal.highCaptured) {
        if (recomputeCalibration(voltageCal)) {
            saveCalibration();
            Serial.println("Voltage calibration updated");
        } else {
            Serial.println("Voltage calibration failed: low/high raw are identical");
        }
    }
}

void captureVoltageHigh(float realValue) {
    if (!lastTlm.valid) {
        Serial.println("No valid telemetry yet");
        return;
    }

    voltageCal.highRaw = getRawVoltageVolts();
    voltageCal.highReal = realValue;
    voltageCal.highCaptured = true;
    saveCalibration();

    Serial.print("Voltage high captured: raw=");
    Serial.print(voltageCal.highRaw, 4);
    Serial.print(" real=");
    Serial.println(voltageCal.highReal, 4);

    if (voltageCal.lowCaptured) {
        if (recomputeCalibration(voltageCal)) {
            saveCalibration();
            Serial.println("Voltage calibration updated");
        } else {
            Serial.println("Voltage calibration failed: low/high raw are identical");
        }
    }
}

void captureCurrentLow(float realValue) {
    if (!lastTlm.valid) {
        Serial.println("No valid telemetry yet");
        return;
    }

    currentCal.lowRaw = getRawCurrentAmps();
    currentCal.lowReal = realValue;
    currentCal.lowCaptured = true;
    saveCalibration();

    Serial.print("Current low captured: raw=");
    Serial.print(currentCal.lowRaw, 4);
    Serial.print(" real=");
    Serial.println(currentCal.lowReal, 4);

    if (currentCal.highCaptured) {
        if (recomputeCalibration(currentCal)) {
            saveCalibration();
            Serial.println("Current calibration updated");
        } else {
            Serial.println("Current calibration failed: low/high raw are identical");
        }
    }
}

void captureCurrentHigh(float realValue) {
    if (!lastTlm.valid) {
        Serial.println("No valid telemetry yet");
        return;
    }

    currentCal.highRaw = getRawCurrentAmps();
    currentCal.highReal = realValue;
    currentCal.highCaptured = true;
    saveCalibration();

    Serial.print("Current high captured: raw=");
    Serial.print(currentCal.highRaw, 4);
    Serial.print(" real=");
    Serial.println(currentCal.highReal, 4);

    if (currentCal.lowCaptured) {
        if (recomputeCalibration(currentCal)) {
            saveCalibration();
            Serial.println("Current calibration updated");
        } else {
            Serial.println("Current calibration failed: low/high raw are identical");
        }
    }
}
