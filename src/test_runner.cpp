#include "test_runner.h"

#include <math.h>

#include "app_config.h"
#include "app_state.h"
#include "calibration.h"
#include "esc_telemetry.h"
#include "motor_control.h"
#include "scale_manager.h"

namespace {

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

        pollEscTelemetry();
        pollScale();
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
