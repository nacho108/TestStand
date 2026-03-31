#include <Arduino.h>
#include <HardwareSerial.h>
#include <Preferences.h>
#include <math.h>
#include <Wire.h>
#include <SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h>
#include <Adafruit_MLX90614.h>

// -------------------- Pins --------------------
static constexpr int ESC_PWM_PIN = 27;     // ESC S wire
static constexpr int ESC_TLM_RX_PIN = 16;  // ESC T wire

// -------------------- PWM config --------------------
static constexpr int PWM_FREQ = 50;
static constexpr int PWM_CHANNEL = 0;
static constexpr int PWM_RESOLUTION = 16;

// -------------------- Telemetry UART --------------------
HardwareSerial escSerial(2);
Preferences preferences;

// -------------------- IR temperature sensor --------------------
Adafruit_MLX90614 irSensor = Adafruit_MLX90614();
bool irDetected = false;
float lastIrAmbientC = NAN;
float lastIrObjectC = NAN;
unsigned long lastIrReadMs = 0;

// -------------------- Scale / Load Cell --------------------
NAU7802 scale;
bool scaleDetected = false;
unsigned long lastScaleReadMs = 0;
int32_t lastScaleRaw = 0;
float lastScaleWeight = 0.0f;
float lastScaleStdDev = 0.0f;
bool lastScaleSampleValid = false;

static constexpr float DEFAULT_SCALE_CAL_FACTOR = 1.0f;
static constexpr int32_t DEFAULT_SCALE_ZERO_OFFSET = 0;
static constexpr unsigned long SCALE_AVG_WINDOW_MS = 500;
static constexpr unsigned long SCALE_TARE_WINDOW_MS = 1000;

// -------------------- CLI state --------------------
String inputBuffer;
float throttlePercent = 0.0f;
bool telemetryStreaming = false;
bool promptShown = false;

// -------------------- CLI history --------------------
static constexpr int CLI_HISTORY_SIZE = 10;
String commandHistory[CLI_HISTORY_SIZE];
int historyCount = 0;
int historyWriteIndex = 0;
int historyBrowseIndex = -1; // -1 means not browsing history
String historyEditBuffer;

// -------------------- Ramp state --------------------
struct RampState {
    bool active = false;
    float startPercent = 0.0f;
    float targetPercent = 0.0f;
    unsigned long startMs = 0;
    unsigned long durationMs = 0;
};

RampState ramp;

// -------------------- Telemetry --------------------
struct EscTelemetry {
    bool valid = false;
    uint8_t temperatureC = 0;
    uint16_t voltageRaw = 0;      // centivolts
    uint16_t currentRaw = 0;      // centiamps
    uint16_t consumptionRaw = 0;
    uint16_t rpmField = 0;        // raw KISS field
    unsigned long lastUpdateMs = 0;
};

EscTelemetry lastTlm;

// -------------------- Calibration --------------------
struct LinearCalibration {
    float scale = 1.0f;
    float offset = 0.0f;

    bool lowCaptured = false;
    bool highCaptured = false;
    float lowRaw = 0.0f;
    float lowReal = 0.0f;
    float highRaw = 0.0f;
    float highReal = 0.0f;
};

LinearCalibration voltageCal;
LinearCalibration currentCal;

// Default values used if nothing was stored yet
static constexpr float DEFAULT_VOLTAGE_SCALE = 1.0f;
static constexpr float DEFAULT_VOLTAGE_OFFSET = 0.0f;
static constexpr float DEFAULT_CURRENT_SCALE = 1.0f;
static constexpr float DEFAULT_CURRENT_OFFSET = 0.0f;

// Change if your motor has a different magnet count
static constexpr int MOTOR_MAGNETS = 28;

// -------------------- Test state --------------------
struct TestResultRow {
    int throttlePercent = 0;
    float voltageV = 0.0f;
    float currentA = 0.0f;
    float powerW = 0.0f;
    float rpm = 0.0f;
    float temperatureC = 0.0f;
    float weightGrams = 0.0f;
    uint32_t sampleCount = 0;
    uint32_t scaleSampleCount = 0;
};

static constexpr int TEST_STEP_PERCENT = 5;
static constexpr int TEST_MAX_RESULTS = 21; // 0..100 inclusive in 5% steps

TestResultRow testResults[TEST_MAX_RESULTS];
int testResultCount = 0;
bool testRunning = false;

// -------------------- CLI helpers --------------------
void showPrompt() {
    Serial.print("> ");
    promptShown = true;
}

void redrawInputLine() {
    Serial.print("\r");
    Serial.print("> ");
    Serial.print(inputBuffer);
    Serial.print(" \r");
    Serial.print("> ");
    Serial.print(inputBuffer);
    promptShown = true;
}

void addCommandToHistory(const String& cmd) {
    String trimmed = cmd;
    trimmed.trim();

    if (trimmed.length() == 0) {
        return;
    }

    if (historyCount > 0) {
        int lastIndex = (historyWriteIndex - 1 + CLI_HISTORY_SIZE) % CLI_HISTORY_SIZE;
        if (commandHistory[lastIndex] == trimmed) {
            return;
        }
    }

    commandHistory[historyWriteIndex] = trimmed;
    historyWriteIndex = (historyWriteIndex + 1) % CLI_HISTORY_SIZE;

    if (historyCount < CLI_HISTORY_SIZE) {
        historyCount++;
    }
}

String getHistoryEntryByBrowseIndex(int browseIndex) {
    if (browseIndex < 0 || browseIndex >= historyCount) {
        return "";
    }

    int oldestIndex = (historyWriteIndex - historyCount + CLI_HISTORY_SIZE) % CLI_HISTORY_SIZE;
    int realIndex = (oldestIndex + browseIndex) % CLI_HISTORY_SIZE;
    return commandHistory[realIndex];
}

void browseHistoryUp() {
    if (historyCount == 0) {
        return;
    }

    if (historyBrowseIndex == -1) {
        historyEditBuffer = inputBuffer;
        historyBrowseIndex = historyCount - 1; // newest
    } else if (historyBrowseIndex > 0) {
        historyBrowseIndex--;
    }

    inputBuffer = getHistoryEntryByBrowseIndex(historyBrowseIndex);
    redrawInputLine();
}

void browseHistoryDown() {
    if (historyCount == 0 || historyBrowseIndex == -1) {
        return;
    }

    if (historyBrowseIndex < historyCount - 1) {
        historyBrowseIndex++;
        inputBuffer = getHistoryEntryByBrowseIndex(historyBrowseIndex);
    } else {
        historyBrowseIndex = -1;
        inputBuffer = historyEditBuffer;
    }

    redrawInputLine();
}

void resetHistoryBrowsing() {
    historyBrowseIndex = -1;
    historyEditBuffer = "";
}

// -------------------- CRC8 --------------------
uint8_t updateCrc8(uint8_t crc, uint8_t crcSeed) {
    uint8_t c = crc ^ crcSeed;
    for (int i = 0; i < 8; i++) {
        c = (c & 0x80) ? (uint8_t)(0x07 ^ (c << 1)) : (uint8_t)(c << 1);
    }
    return c;
}

uint8_t calculateCrc8(const uint8_t* buf, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        crc = updateCrc8(buf[i], crc);
    }
    return crc;
}

// -------------------- PWM helpers --------------------
uint32_t microsecondsToDuty(int us) {
    return (uint32_t)((((uint64_t)us) * ((1UL << PWM_RESOLUTION) - 1)) / 20000ULL);
}

void writeThrottlePercent(float percent) {
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;

    throttlePercent = percent;

    int pulseUs = 1000 + (int)(percent * 10.0f);
    uint32_t duty = microsecondsToDuty(pulseUs);
    ledcWrite(PWM_CHANNEL, duty);
}

void cancelRamp() {
    ramp.active = false;
}

void startRamp(float targetPercent, unsigned long durationMs) {
    if (targetPercent < 0.0f) targetPercent = 0.0f;
    if (targetPercent > 100.0f) targetPercent = 100.0f;

    ramp.active = true;
    ramp.startPercent = throttlePercent;
    ramp.targetPercent = targetPercent;
    ramp.startMs = millis();
    ramp.durationMs = durationMs;
}

void updateRamp() {
    if (!ramp.active) {
        return;
    }

    unsigned long now = millis();
    unsigned long elapsed = now - ramp.startMs;

    if (elapsed >= ramp.durationMs) {
        writeThrottlePercent(ramp.targetPercent);
        ramp.active = false;
        return;
    }

    float t = (float)elapsed / (float)ramp.durationMs;
    float newPercent = ramp.startPercent + (ramp.targetPercent - ramp.startPercent) * t;
    writeThrottlePercent(newPercent);
}

void stopMotorImmediate() {
    cancelRamp();
    writeThrottlePercent(0.0f);
}

void stopMotorSlow() {
    startRamp(0.0f, 3000);
}

void emergencyStopRamp() {
    stopMotorSlow();
    Serial.println();
    Serial.println("EMERGENCY: motor ramping down to 0% in 3 seconds");
}

void startMotorAtFivePercent() {
    cancelRamp();
    writeThrottlePercent(5.0f);
}

void rampToFullPower() {
    startRamp(100.0f, 10000);
}

// -------------------- RPM conversion --------------------
float estimateMechanicalRpm(uint16_t rpmField, int magnetCount) {
    if (magnetCount <= 0) return 0.0f;
    float polePairs = magnetCount / 2.0f;
    return (rpmField * 100.0f) / polePairs;
}

// -------------------- Calibration helpers --------------------
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

// -------------------- Telemetry parsing --------------------
bool tryReadTelemetryFrame(EscTelemetry& outTlm) {
    static uint8_t frame[10];

    while (escSerial.available() >= 10) {
        size_t bytesRead = escSerial.readBytes(frame, 10);
        if (bytesRead != 10) {
            return false;
        }

        uint8_t crc = calculateCrc8(frame, 9);
        if (crc != frame[9]) {
            continue;
        }

        outTlm.valid = true;
        outTlm.temperatureC = frame[0];
        outTlm.voltageRaw = (uint16_t(frame[1]) << 8) | frame[2];
        outTlm.currentRaw = (uint16_t(frame[3]) << 8) | frame[4];
        outTlm.consumptionRaw = (uint16_t(frame[5]) << 8) | frame[6];
        outTlm.rpmField = (uint16_t(frame[7]) << 8) | frame[8];
        outTlm.lastUpdateMs = millis();
        return true;
    }

    return false;
}

// -------------------- Scale helpers --------------------
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

        EscTelemetry tlm;
        if (tryReadTelemetryFrame(tlm)) {
            lastTlm = tlm;
        }

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
    Serial.print(" g  precision(±3σ)=");
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

// -------------------- IR helpers --------------------
bool readIrTemperatures(float& ambientC, float& objectC) {
    if (!irDetected) {
        return false;
    }

    ambientC = irSensor.readAmbientTempC();
    objectC = irSensor.readObjectTempC();

    if (isnan(ambientC) || isnan(objectC)) {
        return false;
    }

    lastIrAmbientC = ambientC;
    lastIrObjectC = objectC;
    lastIrReadMs = millis();
    return true;
}

void printIrStatus() {
    Serial.println("IR sensor:");

    Serial.print("  detected: ");
    Serial.println(irDetected ? "yes" : "no");

    if (!irDetected) {
        Serial.println("  error: MLX90614 not detected");
        return;
    }

    Serial.println("  type: MLX90614 / GY-906 / HW-691");
    Serial.println("  i2c address: 0x5A");

    if (!isnan(lastIrAmbientC) && !isnan(lastIrObjectC)) {
        Serial.print("  last ambient: ");
        Serial.print(lastIrAmbientC, 2);
        Serial.println(" C");

        Serial.print("  last object: ");
        Serial.print(lastIrObjectC, 2);
        Serial.println(" C");

        Serial.print("  last read age: ");
        Serial.print(millis() - lastIrReadMs);
        Serial.println(" ms");
    } else {
        Serial.println("  last reading: none yet");
    }
}

void printIrRead() {
    if (!irDetected) {
        Serial.println("IR sensor not detected");
        return;
    }

    float ambientC = NAN;
    float objectC = NAN;

    if (!readIrTemperatures(ambientC, objectC)) {
        Serial.println("IR read failed");
        return;
    }

    Serial.print("IR ambient=");
    Serial.print(ambientC, 2);
    Serial.print(" C  object=");
    Serial.print(objectC, 2);
    Serial.println(" C");
}

// -------------------- Display helpers --------------------
void printTelemetryLine() {
    if (!lastTlm.valid) {
        Serial.println("Telemetry: no valid frame yet");
        return;
    }

    float rawVoltage = getRawVoltageVolts();
    float rawCurrent = getRawCurrentAmps();

    float voltageV = getCalibratedVoltageVolts();
    float currentA = getCalibratedCurrentAmps();
    float powerW = getCalibratedPowerWatts();
    float rpmEst = estimateMechanicalRpm(lastTlm.rpmField, MOTOR_MAGNETS);

    Serial.print("V=");
    Serial.print(voltageV, 2);
    Serial.print("V  I=");
    Serial.print(currentA, 2);
    Serial.print("A  P=");
    Serial.print(powerW, 2);
    Serial.print("W  RPM=");
    Serial.print((int)rpmEst);
    Serial.print("  rawV=");
    Serial.print(rawVoltage, 2);
    Serial.print("  rawI=");
    Serial.print(rawCurrent, 2);
    Serial.print("  T=");
    Serial.print(lastTlm.temperatureC);
    Serial.println("C");
}

void printStatus() {
    Serial.print("Throttle: ");
    Serial.print(throttlePercent, 1);
    Serial.println("%");

    Serial.print("Ramp active: ");
    Serial.println(ramp.active ? "yes" : "no");

    if (!lastTlm.valid) {
        Serial.println("Telemetry: no valid frame yet");
        return;
    }

    float rawVoltage = getRawVoltageVolts();
    float rawCurrent = getRawCurrentAmps();

    float voltageV = getCalibratedVoltageVolts();
    float currentA = getCalibratedCurrentAmps();
    float powerW = getCalibratedPowerWatts();
    float rpmEst = estimateMechanicalRpm(lastTlm.rpmField, MOTOR_MAGNETS);

    Serial.print("Voltage: ");
    Serial.print(voltageV, 3);
    Serial.print(" V  (raw ");
    Serial.print(rawVoltage, 3);
    Serial.println(" V)");

    Serial.print("Current: ");
    Serial.print(currentA, 3);
    Serial.print(" A  (raw ");
    Serial.print(rawCurrent, 3);
    Serial.println(" A)");

    Serial.print("Power: ");
    Serial.print(powerW, 3);
    Serial.println(" W");

    Serial.print("Estimated RPM: ");
    Serial.println((int)rpmEst);

    Serial.print("RPM raw field: ");
    Serial.println(lastTlm.rpmField);

    Serial.print("Temperature: ");
    Serial.print(lastTlm.temperatureC);
    Serial.println(" C");

    Serial.print("Telemetry age: ");
    Serial.print(millis() - lastTlm.lastUpdateMs);
    Serial.println(" ms");

    if (lastScaleSampleValid) {
        Serial.print("Weight: ");
        Serial.print(lastScaleWeight, 3);
        Serial.println(" g");
    }

    if (irDetected && !isnan(lastIrAmbientC) && !isnan(lastIrObjectC)) {
        Serial.print("IR ambient: ");
        Serial.print(lastIrAmbientC, 2);
        Serial.println(" C");
        Serial.print("IR object: ");
        Serial.print(lastIrObjectC, 2);
        Serial.println(" C");
    }

    printCalibrationStatus();
}

// -------------------- Test helpers --------------------
void clearTestResults() {
    testResultCount = 0;
    for (int i = 0; i < TEST_MAX_RESULTS; i++) {
        testResults[i] = TestResultRow{};
    }
}

bool storeTestResult(const TestResultRow& row) {
    if (testResultCount >= TEST_MAX_RESULTS) {
        return false;
    }
    testResults[testResultCount++] = row;
    return true;
}

void printTestResultsCsv() {
    Serial.println();
    Serial.println("CSV BEGIN");
    Serial.println("ThrottlePercent,VoltageV,CurrentA,PowerW,RPM,ESCTemperatureC,WeightGrams");

    for (int i = 0; i < testResultCount; i++) {
        const TestResultRow& r = testResults[i];
        Serial.print(r.throttlePercent);
        Serial.print(",");
        Serial.print(r.voltageV, 3);
        Serial.print(",");
        Serial.print(r.currentA, 3);
        Serial.print(",");
        Serial.print(r.powerW, 3);
        Serial.print(",");
        Serial.print(r.rpm, 1);
        Serial.print(",");
        Serial.print(r.temperatureC, 2);
        Serial.print(",");
        Serial.println(r.weightGrams, 3);
    }

    Serial.println("CSV END");
    Serial.println();
}

bool updateTelemetryDuringBlockingWait(unsigned long durationMs) {
    unsigned long start = millis();

    while (millis() - start < durationMs) {
        while (Serial.available()) {
            char c = (char)Serial.read();
            if (c == 'X' || c == 'x') {
                emergencyStopRamp();
                return false;
            }
        }

        EscTelemetry tlm;
        if (tryReadTelemetryFrame(tlm)) {
            lastTlm = tlm;
        }

        while (scaleDetected && scale.available()) {
            int32_t raw = scale.getReading();
            lastScaleRaw = raw;
            lastScaleWeight = rawToWeightGrams(raw);
            lastScaleStdDev = 0.0f;
            lastScaleReadMs = millis();
            lastScaleSampleValid = true;
        }

        updateRamp();
        delay(5);
    }

    return true;
}

bool runMotorTest() {
    if (!lastTlm.valid) {
        Serial.println("Cannot start test: no valid telemetry yet");
        return false;
    }

    testRunning = true;
    telemetryStreaming = false;
    cancelRamp();
    clearTestResults();

    Serial.println("Starting motor test...");
    Serial.println("Step rule:");
    Serial.println("  all steps -> 2 seconds total");
    Serial.println("  first 1 second = stabilization");
    Serial.println("  next 1 second = averaging");
    Serial.println("Press X for emergency ramp-down");

    for (int step = 0; step <= 100; step += TEST_STEP_PERCENT) {
        Serial.print("Testing throttle ");
        Serial.print(step);
        Serial.println("%");

        writeThrottlePercent((float)step);

        if (!updateTelemetryDuringBlockingWait(1000)) {
            testRunning = false;
            Serial.println("Motor test aborted by emergency stop");
            return false;
        }

        float sumVoltage = 0.0f;
        float sumCurrent = 0.0f;
        float sumPower = 0.0f;
        float sumRpm = 0.0f;
        float sumTemp = 0.0f;
        float sumWeight = 0.0f;
        uint32_t sampleCount = 0;
        uint32_t scaleSampleCount = 0;

        unsigned long avgStart = millis();
        while (millis() - avgStart < 1000) {
            while (Serial.available()) {
                char c = (char)Serial.read();
                if (c == 'X' || c == 'x') {
                    emergencyStopRamp();
                    testRunning = false;
                    Serial.println("Motor test aborted by emergency stop");
                    return false;
                }
            }

            EscTelemetry tlm;
            if (tryReadTelemetryFrame(tlm)) {
                lastTlm = tlm;

                float voltageV = getCalibratedVoltageVolts();
                float currentA = getCalibratedCurrentAmps();
                float powerW = voltageV * currentA;
                float rpm = estimateMechanicalRpm(lastTlm.rpmField, MOTOR_MAGNETS);
                float tempC = (float)lastTlm.temperatureC;

                sumVoltage += voltageV;
                sumCurrent += currentA;
                sumPower += powerW;
                sumRpm += rpm;
                sumTemp += tempC;
                sampleCount++;
            }

            while (scaleDetected && scale.available()) {
                int32_t raw = scale.getReading();
                float weight = rawToWeightGrams(raw);

                lastScaleRaw = raw;
                lastScaleWeight = weight;
                lastScaleStdDev = 0.0f;
                lastScaleReadMs = millis();
                lastScaleSampleValid = true;

                sumWeight += weight;
                scaleSampleCount++;
            }

            updateRamp();
            delay(5);
        }

        TestResultRow row;
        row.throttlePercent = step;
        row.sampleCount = sampleCount;
        row.scaleSampleCount = scaleSampleCount;

        if (sampleCount > 0) {
            row.voltageV = sumVoltage / sampleCount;
            row.currentA = sumCurrent / sampleCount;
            row.powerW = sumPower / sampleCount;
            row.rpm = sumRpm / sampleCount;
            row.temperatureC = sumTemp / sampleCount;
        } else {
            row.voltageV = NAN;
            row.currentA = NAN;
            row.powerW = NAN;
            row.rpm = NAN;
            row.temperatureC = NAN;
        }

        if (scaleSampleCount > 0) {
            row.weightGrams = sumWeight / scaleSampleCount;
        } else {
            row.weightGrams = NAN;
        }

        if (!storeTestResult(row)) {
            Serial.println("Result storage full");
            testRunning = false;
            stopMotorSlow();
            return false;
        }

        Serial.print("Captured ");
        Serial.print(sampleCount);
        Serial.print(" telemetry samples, ");
        Serial.print(scaleSampleCount);
        Serial.println(" scale samples");
    }

    stopMotorSlow();
    testRunning = false;

    Serial.println("Motor test completed.");
    printTestResultsCsv();
    return true;
}

// -------------------- Commands --------------------
void printHelp() {
    Serial.println("Commands:");
    Serial.println("  motor start                  -> start motor at 5%");
    Serial.println("  motor stop                   -> ramp down to 0% in 3 seconds");
    Serial.println("  motor stop now               -> immediate stop");
    Serial.println("  motor set <0-100>            -> set throttle percent immediately");
    Serial.println("  motor ramp                   -> ramp from current throttle to 100% in 10 seconds");
    Serial.println("  motor test                   -> full automatic motor test and CSV output");
    Serial.println("  X                            -> emergency ramp-down to 0% in 3 seconds");
    Serial.println("  status                       -> print latest telemetry once");
    Serial.println("  telemetry on                 -> start periodic telemetry output");
    Serial.println("  telemetry off                -> stop periodic telemetry output");
    Serial.println("  scale status                 -> print NAU7802 status");
    Serial.println("  scale read                   -> read averaged load cell value over 0.5 s");
    Serial.println("  scale tare                   -> tare / zero the load cell with 1 second average");
    Serial.println("  scale calibration <grams>    -> calibrate scale using known weight in grams");
    Serial.println("  calibrate current low <A>");
    Serial.println("  calibrate current high <A>");
    Serial.println("  calibrate voltage low <V>");
    Serial.println("  calibrate voltage high <V>");
    Serial.println("  calibration show");
    Serial.println("  calibration reset");
    Serial.println("  ir status                    -> print MLX90614 status");
    Serial.println("  ir read                      -> read ambient and object temperatures");
    Serial.println("  help");
}

void handleCommand(String cmd) {
    cmd.trim();

    if (cmd.length() == 0) {
        return;
    }

    if (testRunning) {
        Serial.println("Test is running. Command ignored.");
        return;
    }

    float calValue = 0.0f;

    if (cmd.equalsIgnoreCase("help")) {
        printHelp();
        return;
    }

    if (cmd.equalsIgnoreCase("motor start")) {
        startMotorAtFivePercent();
        Serial.println("Motor started at 5%");
        return;
    }

    if (cmd.equalsIgnoreCase("motor stop")) {
        stopMotorSlow();
        Serial.println("Motor ramping down to 0% in 3 seconds");
        return;
    }

    if (cmd.equalsIgnoreCase("motor stop now")) {
        stopMotorImmediate();
        Serial.println("Motor stopped immediately");
        return;
    }

    if (cmd.equalsIgnoreCase("motor ramp")) {
        rampToFullPower();
        Serial.println("Motor ramping to 100% in 10 seconds");
        return;
    }

    if (cmd.equalsIgnoreCase("motor test")) {
        runMotorTest();
        return;
    }

    if (cmd.startsWith("motor set ")) {
        String valueText = cmd.substring(strlen("motor set "));
        valueText.trim();

        float value = valueText.toFloat();
        if (value < 0.0f || value > 100.0f) {
            Serial.println("Throttle must be between 0 and 100");
            return;
        }

        cancelRamp();
        writeThrottlePercent(value);
        Serial.print("Throttle set to ");
        Serial.print(throttlePercent, 1);
        Serial.println("%");
        return;
    }

    if (cmd.equalsIgnoreCase("status")) {
        printStatus();
        return;
    }

    if (cmd.equalsIgnoreCase("telemetry on")) {
        telemetryStreaming = true;
        Serial.println("Telemetry streaming enabled");
        return;
    }

    if (cmd.equalsIgnoreCase("telemetry off")) {
        telemetryStreaming = false;
        Serial.println("Telemetry streaming disabled");
        return;
    }

    if (cmd.equalsIgnoreCase("scale status")) {
        printScaleStatus();
        return;
    }

    if (cmd.equalsIgnoreCase("scale read")) {
        printScaleReading();
        return;
    }

    if (cmd.equalsIgnoreCase("scale tare")) {
        tareScale();
        return;
    }

    if (parseScaleCalibrationCommand(cmd, calValue)) {
        calibrateScale(calValue);
        return;
    }

    if (cmd.equalsIgnoreCase("calibration show")) {
        printCalibrationStatus();
        return;
    }

    if (cmd.equalsIgnoreCase("calibration reset")) {
        resetCalibration();
        Serial.println("Calibration reset to defaults");
        printCalibrationStatus();
        return;
    }

    if (parseCalibrationCommand(cmd, "calibrate current low ", calValue)) {
        captureCurrentLow(calValue);
        return;
    }

    if (parseCalibrationCommand(cmd, "calibrate current high ", calValue)) {
        captureCurrentHigh(calValue);
        return;
    }

    if (parseCalibrationCommand(cmd, "calibrate voltage low ", calValue)) {
        captureVoltageLow(calValue);
        return;
    }

    if (parseCalibrationCommand(cmd, "calibrate voltage high ", calValue)) {
        captureVoltageHigh(calValue);
        return;
    }
    if (cmd.equalsIgnoreCase("ir status")) {
        printIrStatus();
        return;
    }

    if (cmd.equalsIgnoreCase("ir read")) {
        printIrRead();
        return;
    }
    
    Serial.println("Unknown command. Type: help");
}

// -------------------- Console input --------------------
void readConsole() {
    while (Serial.available()) {
        char c = (char)Serial.read();

        if (c == 'X' || c == 'x') {
            inputBuffer = "";
            promptShown = false;
            resetHistoryBrowsing();
            emergencyStopRamp();
            showPrompt();
            continue;
        }

        if ((uint8_t)c == 27) {
            unsigned long startWait = millis();
            while (Serial.available() < 2 && (millis() - startWait < 20)) {
                delay(1);
            }

            if (Serial.available() >= 2) {
                char c1 = (char)Serial.read();
                char c2 = (char)Serial.read();

                if (c1 == '[') {
                    if (c2 == 'A') {
                        browseHistoryUp();
                        continue;
                    } else if (c2 == 'B') {
                        browseHistoryDown();
                        continue;
                    }
                }

                continue;
            }

            continue;
        }

        if (c == 8 || c == 127) {
            resetHistoryBrowsing();
            if (inputBuffer.length() > 0) {
                inputBuffer.remove(inputBuffer.length() - 1);
                Serial.print("\b \b");
            }
        } else if (c == '\r') {
        } else if (c == '\n') {
            Serial.println();
            promptShown = false;

            String cmd = inputBuffer;
            addCommandToHistory(cmd);
            resetHistoryBrowsing();

            handleCommand(cmd);
            inputBuffer = "";
            showPrompt();
        } else {
            resetHistoryBrowsing();
            inputBuffer += c;
            Serial.print(c);
        }
    }
}

// -------------------- Main --------------------
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("ESP32 + AM32 CLI (PWM + telemetry + scale)");

    setCalibrationDefaults();
    loadCalibration();

    Wire.begin();
    Wire.setClock(100000);

    irDetected = irSensor.begin();
    if (irDetected) {
        Serial.println("MLX90614 IR sensor detected.");
    } else {
        Serial.println("WARNING: MLX90614 IR sensor not detected.");
    }

    scaleDetected = scale.begin();
    if (scaleDetected) {
        scale.setSampleRate(NAU7802_SPS_320);
        scale.calibrateAFE();
        loadScaleCalibration();

        Serial.println("NAU7802 scale detected.");
        Serial.print("Scale zero offset: ");
        Serial.println(scale.getZeroOffset());
        Serial.print("Scale calibration factor: ");
        Serial.println(scale.getCalibrationFactor(), 6);
    } else {
        Serial.println("WARNING: NAU7802 scale not detected.");
    }

    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(ESC_PWM_PIN, PWM_CHANNEL);
    stopMotorImmediate();

    escSerial.begin(115200, SERIAL_8N1, ESC_TLM_RX_PIN, -1);

    Serial.println("Ready.");
    printHelp();
    printCalibrationStatus();
    showPrompt();
}

void loop() {
    readConsole();
    updateRamp();

    EscTelemetry tlm;
    if (tryReadTelemetryFrame(tlm)) {
        lastTlm = tlm;
    }

    while (scaleDetected && scale.available()) {
        int32_t raw = scale.getReading();
        lastScaleRaw = raw;
        lastScaleWeight = rawToWeightGrams(raw);
        lastScaleStdDev = 0.0f;
        lastScaleReadMs = millis();
        lastScaleSampleValid = true;
    }

    static unsigned long lastPrint = 0;
    if (!testRunning && telemetryStreaming && millis() - lastPrint >= 1000) {
        lastPrint = millis();

        Serial.println();
        printTelemetryLine();

        if (inputBuffer.length() > 0) {
            showPrompt();
            Serial.print(inputBuffer);
        } else {
            showPrompt();
        }
    }

    delay(10);
}