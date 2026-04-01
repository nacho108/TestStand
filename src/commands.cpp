#include "commands.h"

#include "app_state.h"
#include "calibration.h"
#include "display.h"
#include "ir_manager.h"
#include "motor_control.h"
#include "scale_manager.h"
#include "test_runner.h"

void printHelp() {
    Serial.println("Commands:");
    Serial.println("  motor start                  -> start motor at 5%");
    Serial.println("  motor stop                   -> ramp down to 0% in 3 seconds");
    Serial.println("  motor stop now               -> immediate stop");
    Serial.println("  motor set <0-100>            -> softly ramp to target (1 s per 30%)");
    Serial.println("  motor ramp                   -> ramp from current throttle to 100% in 10 seconds");
    Serial.println("  motor test                   -> full automatic motor test and CSV output");
    Serial.println("  X                            -> emergency ramp-down to 0% with soft ramp");
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

        unsigned long durationMs = calculateThrottleRampDurationMs(throttlePercent, value);
        startRamp(value, durationMs);

        if (durationMs == 0) {
            Serial.print("Throttle already at ");
            Serial.print(throttlePercent, 1);
            Serial.println("%");
            return;
        }

        Serial.print("Throttle ramping to ");
        Serial.print(value, 1);
        Serial.print("% in ");
        Serial.print(durationMs);
        Serial.println(" ms");
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
