#include <Arduino.h>
#include <Wire.h>

#include "app_state.h"
#include "calibration.h"
#include "commands.h"
#include "console_ui.h"
#include "display.h"
#include "esc_telemetry.h"
#include "ir_manager.h"
#include "motor_control.h"
#include "scale_manager.h"

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("ESP32 + AM32 CLI (PWM + telemetry + scale)");

    setCalibrationDefaults();
    loadCalibration();

    Wire.begin();
    Wire.setClock(100000);

    if (beginIrManager()) {
        Serial.println("MLX90614 IR sensor detected.");
    } else {
        Serial.println("WARNING: MLX90614 IR sensor not detected.");
    }

    beginScaleManager();
    if (scaleDetected) {
        Serial.println("NAU7802 scale detected.");
        Serial.print("Scale zero offset: ");
        Serial.println(scale.getZeroOffset());
        Serial.print("Scale calibration factor: ");
        Serial.println(scale.getCalibrationFactor(), 6);
    } else {
        Serial.println("WARNING: NAU7802 scale not detected.");
    }

    beginMotorControl();
    beginEscTelemetry();

    Serial.println("Ready.");
    printHelp();
    printCalibrationStatus();
    showPrompt();
}

void loop() {
    readConsole();
    updateRamp();
    pollEscTelemetry();
    pollScale();

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
