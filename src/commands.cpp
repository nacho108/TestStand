#include "commands.h"

#include "app_state.h"
#include "calibration.h"
#include "display.h"
#include "ir_manager.h"
#include "motor_control.h"
#include "scale_manager.h"
#include "test_runner.h"
#include "web_server.h"

namespace {

bool parseFilenameArgument(const String& cmd, const char* prefix, String& filename) {
    if (!cmd.startsWith(prefix)) {
        return false;
    }

    filename = cmd.substring(strlen(prefix));
    filename.trim();
    return true;
}

void handlePendingTestSaveCommand(const String& cmd) {
    if (testSavePromptPending) {
        if (cmd.equalsIgnoreCase("yes")) {
            testSavePromptPending = false;
            testFilenamePromptPending = true;
            Serial.println("Enter file name for the test:");
            return;
        }

        if (cmd.equalsIgnoreCase("no")) {
            testSavePromptPending = false;
            Serial.println("Test was not saved");
            return;
        }

        Serial.println("Please answer YES or NO");
        return;
    }

    if (testFilenamePromptPending) {
        if (saveLastTestToLittleFS(cmd)) {
            testFilenamePromptPending = false;
            return;
        }

        Serial.println("Enter a different file name:");
    }
}

}

void printHelp() {
    Serial.println("Commands:");
    Serial.println("  motor start                  -> start motor at 5%");
    Serial.println("  motor stop                   -> ramp down to 0% in 3 seconds");
    Serial.println("  motor stop now               -> immediate stop");
    Serial.println("  motor set <0-100>            -> softly ramp to target (1 s per 30%)");
    Serial.println("  motor ramp                   -> ramp from current throttle to 100% in 10 seconds");
    Serial.println("  motor test                   -> full automatic motor test and CSV output");
    Serial.println("  test list                    -> list saved test CSV files in LittleFS");
    Serial.println("  test get <filename>          -> print a saved test CSV file");
    Serial.println("  test remove <filename>       -> delete a saved test CSV file");
    Serial.println("  pass                         -> reboot into ESC passthrough mode");
    Serial.println("  esc params                   -> read one AM32 ESC parameter with debug output");
    Serial.println("  esc dump                     -> read and print the full AM32 EEPROM region");
    Serial.println("  esc reverse                  -> toggle the AM32 dir_reversed parameter");
    Serial.println("  X                            -> emergency ramp-down to 0% with soft ramp");
    Serial.println("  status                       -> print latest telemetry once");
    Serial.println("  telemetry on                 -> start periodic telemetry output");
    Serial.println("  telemetry off                -> stop periodic telemetry output");
    Serial.println("  scale status                 -> print NAU7802 status");
    Serial.println("  scale read                   -> print live 0.5 s moving-average load cell value");
    Serial.println("  scale tare                   -> tare / zero the load cell with 1 second average");
    Serial.println("  scale calibration <grams>    -> calibrate scale using known weight in grams with 1 second average");
    Serial.println("  calibrate current low <A>");
    Serial.println("  calibrate current high <A>");
    Serial.println("  calibrate voltage low <V>");
    Serial.println("  calibrate voltage high <V>");
    Serial.println("  calibration show");
    Serial.println("  calibration reset");
    Serial.println("  ir status                    -> print MLX90614 status");
    Serial.println("  ir read                      -> read ambient and object temperatures");
    Serial.println("  wifi select                  -> scan networks, choose by number, then enter password");
    Serial.println("  wifi connect                 -> retry connecting to saved Wi-Fi networks");
    Serial.println("  wifi forget                  -> list saved networks and choose one to remove");
    Serial.println("  wifi status                  -> print current Wi-Fi mode, connection, and saved networks");
    Serial.println("  help");
}

void handleCommand(String cmd) {
    cmd.trim();

    if (cmd.length() == 0) {
        return;
    }

    if (testSavePromptPending || testFilenamePromptPending) {
        handlePendingTestSaveCommand(cmd);
        return;
    }

    if (wifiSelectionPending()) {
        handleWifiSelectionInput(cmd);
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

    if (cmd.equalsIgnoreCase("test list")) {
        listSavedTests();
        return;
    }

    String filename;
    if (parseFilenameArgument(cmd, "test get ", filename)) {
        printSavedTest(filename);
        return;
    }

    if (parseFilenameArgument(cmd, "test remove ", filename)) {
        removeSavedTest(filename);
        return;
    }

    if (cmd.equalsIgnoreCase("pass") || cmd.equalsIgnoreCase("passthrough")) {
        requestEscPassthroughModeAndRestart();
        return;
    }

    if (cmd.equalsIgnoreCase("esc params")) {
        readEscParametersDebug();
        return;
    }

    if (cmd.equalsIgnoreCase("esc dump")) {
        dumpEscEepromDebug();
        return;
    }

    if (cmd.equalsIgnoreCase("esc reverse")) {
        toggleEscDirectionReverseDebug();
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

    if (cmd.equalsIgnoreCase("wifi select")) {
        beginWifiSelection();
        return;
    }

    if (cmd.equalsIgnoreCase("wifi connect")) {
        reconnectSavedWifi();
        return;
    }

    if (cmd.equalsIgnoreCase("wifi forget")) {
        beginWifiForget();
        return;
    }

    if (cmd.equalsIgnoreCase("wifi status")) {
        printWifiStatus();
        return;
    }

    Serial.println("Unknown command. Type: help");
}
