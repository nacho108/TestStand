#include "test_runner.h"

#include <LittleFS.h>
#include <math.h>

#include "app_config.h"
#include "app_state.h"
#include "calibration.h"
#include "esc_telemetry.h"
#include "ir_manager.h"
#include "motor_control.h"
#include "scale_manager.h"
#include "web_server.h"

namespace {

constexpr const char* kTestStorageDir = "/tests";

void serviceTestLoop() {
    pollEscTelemetry();
    pollScale();
    updateRamp();
    updateWebServer();
}

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

String buildTestCsv() {
    String csv;
    csv.reserve(1024);
    csv += "ThrottlePercent,VoltageV,CurrentA,PowerW,RPM,ESCTemperatureC,MotorTemperatureC,WeightGrams\n";

    for (int i = 0; i < testResultCount; i++) {
        const TestResultRow& r = testResults[i];
        csv += String(r.throttlePercent);
        csv += ",";
        csv += String(r.voltageV, 3);
        csv += ",";
        csv += String(r.currentA, 3);
        csv += ",";
        csv += String(r.powerW, 3);
        csv += ",";
        csv += String(r.rpm, 1);
        csv += ",";
        csv += String(r.escTemperatureC, 2);
        csv += ",";
        csv += String(r.motorTemperatureC, 2);
        csv += ",";
        csv += String(r.weightGrams, 3);
        csv += "\n";
    }

    return csv;
}

void printTestResultsCsv() {
    Serial.println();
    Serial.println("CSV BEGIN");
    Serial.print(buildTestCsv());

    Serial.println("CSV END");
    Serial.println();
}

String buildTestPath(const String& normalizedFilename) {
    return String(kTestStorageDir) + "/" + normalizedFilename;
}

}

bool beginTestStorage() {
    if (!LittleFS.begin(true)) {
        return false;
    }

    if (!LittleFS.exists(kTestStorageDir) && !LittleFS.mkdir(kTestStorageDir)) {
        return false;
    }

    return true;
}

bool hasTestResults() {
    return testResultCount > 0;
}

String normalizeTestFilename(const String& filename) {
    String trimmed = filename;
    trimmed.trim();

    if (trimmed.length() == 0) {
        return "";
    }

    String normalized;
    normalized.reserve(trimmed.length() + 4);

    for (size_t i = 0; i < trimmed.length(); i++) {
        char c = trimmed.charAt(i);
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.') {
            normalized += c;
        } else if (c == ' ') {
            normalized += '_';
        } else {
            return "";
        }
    }

    if (normalized.length() == 0 || normalized == "." || normalized == "..") {
        return "";
    }

    if (!normalized.endsWith(".csv")) {
        normalized += ".csv";
    }

    return normalized;
}

bool saveLastTestToLittleFS(const String& filename) {
    if (!hasTestResults()) {
        Serial.println("No test results available to save");
        return false;
    }

    const String normalized = normalizeTestFilename(filename);
    if (normalized.length() == 0) {
        Serial.println("Invalid file name. Use letters, digits, space, '-', '_' or '.'");
        return false;
    }

    const String path = buildTestPath(normalized);
    File file = LittleFS.open(path, FILE_WRITE);
    if (!file) {
        Serial.print("Failed to open file for writing: ");
        Serial.println(path);
        return false;
    }

    const String csv = buildTestCsv();
    const size_t written = file.print(csv);
    file.close();

    if (written != csv.length()) {
        Serial.print("Failed to fully write file: ");
        Serial.println(path);
        return false;
    }

    Serial.print("Saved test to ");
    Serial.println(path);
    return true;
}

void listSavedTests() {
    File dir = LittleFS.open(kTestStorageDir);
    if (!dir || !dir.isDirectory()) {
        Serial.println("Test storage directory is not available");
        return;
    }

    File entry = dir.openNextFile();
    if (!entry) {
        Serial.println("No saved tests");
        return;
    }

    Serial.println("Saved tests:");
    while (entry) {
        if (!entry.isDirectory()) {
            String name = entry.name();
            const int slashPos = name.lastIndexOf('/');
            if (slashPos >= 0) {
                name = name.substring(slashPos + 1);
            }

            Serial.print("  ");
            Serial.print(name);
            Serial.print(" (");
            Serial.print((unsigned long)entry.size());
            Serial.println(" bytes)");
        }

        entry = dir.openNextFile();
    }
}

bool removeSavedTest(const String& filename) {
    const String normalized = normalizeTestFilename(filename);
    if (normalized.length() == 0) {
        Serial.println("Invalid file name");
        return false;
    }

    const String path = buildTestPath(normalized);
    if (!LittleFS.exists(path)) {
        Serial.print("Saved test not found: ");
        Serial.println(path);
        return false;
    }

    if (!LittleFS.remove(path)) {
        Serial.print("Failed to remove file: ");
        Serial.println(path);
        return false;
    }

    Serial.print("Removed ");
    Serial.println(path);
    return true;
}

bool printSavedTest(const String& filename) {
    const String normalized = normalizeTestFilename(filename);
    if (normalized.length() == 0) {
        Serial.println("Invalid file name");
        return false;
    }

    const String path = buildTestPath(normalized);
    File file = LittleFS.open(path, FILE_READ);
    if (!file) {
        Serial.print("Saved test not found: ");
        Serial.println(path);
        return false;
    }

    Serial.println();
    Serial.println("FILE BEGIN");
    while (file.available()) {
        Serial.write(file.read());
    }
    file.close();

    if (!Serial.availableForWrite()) {
        Serial.println();
    }

    Serial.println();
    Serial.println("FILE END");
    return true;
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

        serviceTestLoop();
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
    Serial.println("  each step -> proportional ramp + 1.5 seconds at target");
    Serial.println("  first 0.5 second at target = stabilization");
    Serial.println("  next 1 second at target = averaging");
    Serial.println("Press X for emergency ramp-down");

    for (int step = 0; step <= 100; step += TEST_STEP_PERCENT) {
        unsigned long rampDurationMs = calculateThrottleRampDurationMs(throttlePercent, (float)step);

        Serial.print("Testing throttle ");
        Serial.print(step);
        Serial.print("%");
        if (rampDurationMs > 0) {
            Serial.print(" with ");
            Serial.print(rampDurationMs);
            Serial.println(" ms ramp");
        } else {
            Serial.println();
        }

        startRamp((float)step, rampDurationMs);

        if (rampDurationMs > 0 && !updateTelemetryDuringBlockingWait(rampDurationMs)) {
            testRunning = false;
            Serial.println("Motor test aborted by emergency stop");
            return false;
        }

        if (!updateTelemetryDuringBlockingWait(500)) {
            testRunning = false;
            Serial.println("Motor test aborted by emergency stop");
            return false;
        }

        float sumVoltage = 0.0f;
        float sumCurrent = 0.0f;
        float sumPower = 0.0f;
        float sumRpm = 0.0f;
        float sumEscTemp = 0.0f;
        float sumMotorTemp = 0.0f;
        uint32_t sampleCount = 0;
        uint32_t motorTempSampleCount = 0;
        uint32_t scaleSampleCount = 0;
        float sumWeight = 0.0f;

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
                float rpm = estimateMechanicalRpm(lastTlm.rpmField, motorPoleCount);
                float escTempC = (float)lastTlm.temperatureC;

                sumVoltage += voltageV;
                sumCurrent += currentA;
                sumPower += powerW;
                sumRpm += rpm;
                sumEscTemp += escTempC;
                sampleCount++;
            }

            float ambientC = NAN;
            float objectC = NAN;
            if (readIrTemperatures(ambientC, objectC)) {
                sumMotorTemp += objectC;
                motorTempSampleCount++;
            }

            pollScale();
            if (lastScaleSampleValid) {
                sumWeight += lastScaleWeight;
                scaleSampleCount++;
            }

            updateWebServer();
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
            row.escTemperatureC = sumEscTemp / sampleCount;
        } else {
            row.voltageV = NAN;
            row.currentA = NAN;
            row.powerW = NAN;
            row.rpm = NAN;
            row.escTemperatureC = NAN;
        }

        if (motorTempSampleCount > 0) {
            row.motorTemperatureC = sumMotorTemp / motorTempSampleCount;
        } else {
            row.motorTemperatureC = NAN;
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
        Serial.println(" filtered scale reads");
    }

    stopMotorSlow();
    testRunning = false;

    Serial.println("Motor test completed.");
    printTestResultsCsv();
    testSavePromptPending = true;
    testFilenamePromptPending = false;
    Serial.println("Would you like to save the test? Type YES to save or NO to skip.");
    return true;
}
