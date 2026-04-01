#include "display.h"

#include <math.h>

#include "app_config.h"
#include "app_state.h"
#include "calibration.h"
#include "esc_telemetry.h"

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
