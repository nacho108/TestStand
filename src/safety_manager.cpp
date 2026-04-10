#include "safety_manager.h"

#include <math.h>

#include "app_config.h"
#include "app_state.h"
#include "calibration.h"
#include "motor_control.h"
#include "test_runner.h"

namespace {

constexpr const char* kSafetyPrefsNamespace = "am32cli";
constexpr const char* kCurrentHiKey = "safe_i_hi";
constexpr const char* kCurrentHiHiKey = "safe_i_hihi";
constexpr const char* kMotorTempHiKey = "safe_mt_hi";
constexpr const char* kMotorTempHiHiKey = "safe_mt_hihi";
constexpr const char* kEscTempHiKey = "safe_et_hi";
constexpr const char* kEscTempHiHiKey = "safe_et_hihi";

bool safetyConfigLoaded = false;
bool safetyMotorTestStopRequested = false;
String safetyMotorTestStopReason;

bool isThresholdEnabled(float value) {
    return isfinite(value) && value >= 0.0f;
}

bool validateThresholdPair(const __FlashStringHelper* label, float hi, float hihi) {
    if (isThresholdEnabled(hi) && isThresholdEnabled(hihi) && hihi <= hi) {
        Serial.print(label);
        Serial.println(" HIHI must be greater than HI.");
        return false;
    }

    return true;
}

void printThresholdLine(const __FlashStringHelper* label, const SafetyThresholdPair& pair, const char* unit) {
    Serial.print(label);
    Serial.print(" HI=");
    if (isThresholdEnabled(pair.hi)) {
        Serial.print(pair.hi, 2);
        Serial.print(" ");
        Serial.print(unit);
    } else {
        Serial.print("disabled");
    }

    Serial.print(" HIHI=");
    if (isThresholdEnabled(pair.hihi)) {
        Serial.print(pair.hihi, 2);
        Serial.print(" ");
        Serial.print(unit);
    } else {
        Serial.print("disabled");
    }
    Serial.println();
}

SafetyLevel evaluateLevel(float value, bool available, const SafetyThresholdPair& pair) {
    if (!available || !isfinite(value)) {
        return SafetyLevel::Unknown;
    }

    if (isThresholdEnabled(pair.hihi) && value >= pair.hihi) {
        return SafetyLevel::HiHi;
    }

    if (isThresholdEnabled(pair.hi) && value >= pair.hi) {
        return SafetyLevel::Hi;
    }

    return SafetyLevel::Normal;
}

void triggerSafetyTrip(const String& reason) {
    if (safetyStatus.tripActive && safetyStatus.tripReason == reason) {
        return;
    }

    safetyStatus.tripActive = true;
    safetyStatus.tripReason = reason;
    safetyMotorTestStopRequested = true;
    safetyMotorTestStopReason = reason;

    Serial.print("SAFETY TRIP: ");
    Serial.println(reason);

    stopMotorImmediate();
    if (testRunning) {
        requestMotorTestStop();
    }
}

bool assignThreshold(
    SafetyThresholdPair& pair,
    bool isHiHi,
    float value,
    const __FlashStringHelper* label,
    const char* unit
) {
    if (!isfinite(value) || value < 0.0f) {
        Serial.print(label);
        Serial.println(" threshold must be a non-negative number.");
        return false;
    }

    const float newHi = isHiHi ? pair.hi : value;
    const float newHiHi = isHiHi ? value : pair.hihi;
    if (!validateThresholdPair(label, newHi, newHiHi)) {
        return false;
    }

    if (isHiHi) {
        pair.hihi = value;
    } else {
        pair.hi = value;
    }

    saveSafetyConfiguration();

    Serial.print(label);
    Serial.print(isHiHi ? " HIHI set to " : " HI set to ");
    Serial.print(value, 2);
    Serial.print(" ");
    Serial.println(unit);
    return true;
}

}  // namespace

void loadSafetyConfiguration() {
    if (safetyConfigLoaded) {
        return;
    }

    preferences.begin(kSafetyPrefsNamespace, true);
    safetyConfig.currentA.hi = preferences.getFloat(kCurrentHiKey, -1.0f);
    safetyConfig.currentA.hihi = preferences.getFloat(kCurrentHiHiKey, -1.0f);
    safetyConfig.motorTemperatureC.hi = preferences.getFloat(kMotorTempHiKey, -1.0f);
    safetyConfig.motorTemperatureC.hihi = preferences.getFloat(kMotorTempHiHiKey, -1.0f);
    safetyConfig.escTemperatureC.hi = preferences.getFloat(kEscTempHiKey, -1.0f);
    safetyConfig.escTemperatureC.hihi = preferences.getFloat(kEscTempHiHiKey, -1.0f);
    preferences.end();

    safetyConfigLoaded = true;
}

void saveSafetyConfiguration() {
    preferences.begin(kSafetyPrefsNamespace, false);
    preferences.putFloat(kCurrentHiKey, safetyConfig.currentA.hi);
    preferences.putFloat(kCurrentHiHiKey, safetyConfig.currentA.hihi);
    preferences.putFloat(kMotorTempHiKey, safetyConfig.motorTemperatureC.hi);
    preferences.putFloat(kMotorTempHiHiKey, safetyConfig.motorTemperatureC.hihi);
    preferences.putFloat(kEscTempHiKey, safetyConfig.escTemperatureC.hi);
    preferences.putFloat(kEscTempHiHiKey, safetyConfig.escTemperatureC.hihi);
    preferences.end();

    safetyConfigLoaded = true;
}

void printSafetyConfiguration() {
    loadSafetyConfiguration();

    Serial.println("Safety:");
    printThresholdLine(F("  Current:"), safetyConfig.currentA, "A");
    printThresholdLine(F("  Motor temp:"), safetyConfig.motorTemperatureC, "C");
    printThresholdLine(F("  ESC temp:"), safetyConfig.escTemperatureC, "C");
}

bool setCurrentHiThreshold(float value) {
    loadSafetyConfiguration();
    return assignThreshold(safetyConfig.currentA, false, value, F("Current"), "A");
}

bool setCurrentHiHiThreshold(float value) {
    loadSafetyConfiguration();
    return assignThreshold(safetyConfig.currentA, true, value, F("Current"), "A");
}

bool setMotorTempHiThreshold(float value) {
    loadSafetyConfiguration();
    return assignThreshold(safetyConfig.motorTemperatureC, false, value, F("Motor temp"), "C");
}

bool setMotorTempHiHiThreshold(float value) {
    loadSafetyConfiguration();
    return assignThreshold(safetyConfig.motorTemperatureC, true, value, F("Motor temp"), "C");
}

bool setEscTempHiThreshold(float value) {
    loadSafetyConfiguration();
    return assignThreshold(safetyConfig.escTemperatureC, false, value, F("ESC temp"), "C");
}

bool setEscTempHiHiThreshold(float value) {
    loadSafetyConfiguration();
    return assignThreshold(safetyConfig.escTemperatureC, true, value, F("ESC temp"), "C");
}

void updateSafetyStatus() {
    loadSafetyConfiguration();

    const bool telemetryAvailable = lastTlm.valid;
    const bool motorTempAvailable = isfinite(lastIrObjectC);
    const float currentA = telemetryAvailable ? getCalibratedCurrentAmps() : NAN;
    const float escTempC = telemetryAvailable ? (float)lastTlm.temperatureC : NAN;
    const float motorTempC = motorTempAvailable ? lastIrObjectC : NAN;

    safetyStatus.currentLevel = evaluateLevel(currentA, telemetryAvailable, safetyConfig.currentA);
    safetyStatus.escTemperatureLevel = evaluateLevel(escTempC, telemetryAvailable, safetyConfig.escTemperatureC);
    safetyStatus.motorTemperatureLevel = evaluateLevel(
        motorTempC,
        motorTempAvailable,
        safetyConfig.motorTemperatureC
    );

    String tripReason;
    if (safetyStatus.currentLevel == SafetyLevel::HiHi) {
        tripReason = "Current exceeded HIHI threshold";
    } else if (safetyStatus.escTemperatureLevel == SafetyLevel::HiHi) {
        tripReason = "ESC temperature exceeded HIHI threshold";
    } else if (safetyStatus.motorTemperatureLevel == SafetyLevel::HiHi) {
        tripReason = "Motor temperature exceeded HIHI threshold";
    }

    if (tripReason.length() > 0) {
        triggerSafetyTrip(tripReason);
    } else if (safetyStatus.tripActive && throttlePercent <= 0.0f && !testRunning) {
        safetyStatus.tripActive = false;
        safetyStatus.tripReason = "";
        safetyMotorTestStopRequested = false;
        safetyMotorTestStopReason = "";
    }
}

bool consumeSafetyMotorTestStopRequest(String& outReason) {
    if (!safetyMotorTestStopRequested) {
        return false;
    }

    outReason = safetyMotorTestStopReason;
    safetyMotorTestStopRequested = false;
    safetyMotorTestStopReason = "";
    return true;
}
