#include "commands.h"

#include "app_state.h"
#include "calibration.h"
#include "display.h"
#include "ir_manager.h"
#include "motor_control.h"
#include "scale_manager.h"
#include "safety_manager.h"
#include "test_runner.h"
#include "web_server.h"

namespace {

struct CommandHelpEntry {
    const char* command;
    const char* description;
};

constexpr CommandHelpEntry kCommandHelpEntries[] = {
    {"motor start", "start motor at 5%"},
    {"motor stop", "ramp down to 0% in 3 seconds"},
    {"motor stop now", "immediate stop"},
    {"motor set <0-100>", "softly ramp to target (1 s per 30%)"},
    {"motor ramp", "ramp from current throttle to 100% in 10 seconds"},
    {"motor test", "full automatic motor test and CSV output"},
    {"motor test puller", "run motor test in puller mode"},
    {"motor test pusher", "run motor test in pusher mode"},
    {"motor test mode puller", "set thrust display/log mode to puller"},
    {"motor test mode pusher", "set thrust display/log mode to pusher"},
    {"motor test cooling on", "enable 30% post-test cooldown hold"},
    {"motor test cooling off", "disable post-test cooldown hold"},
    {"motor test stop", "stop the running motor test"},
    {"restart", "reboot the ESP32 board"},
    {"pass", "reboot into ESC passthrough mode"},
    {"esc params", "read one AM32 ESC parameter with debug output"},
    {"esc dump", "read and print the full AM32 EEPROM region"},
    {"esc reverse", "toggle the AM32 dir_reversed parameter"},
    {"esc poles <value>", "write ESC motor pole count and use it in RPM calculations"},
    {"esc kv <value>", "write ESC motor KV estimate"},
    {"X", "emergency ramp-down to 0% with soft ramp"},
    {"status", "print latest telemetry once"},
    {"telemetry on", "start periodic telemetry output"},
    {"telemetry off", "stop periodic telemetry output"},
    {"scale status", "print NAU7802 status"},
    {"scale read", "print live 0.5 s moving-average load cell value"},
    {"scale tare", "tare / zero the load cell with 1 second average"},
    {"scale calibration <grams>", "calibrate scale using known weight in grams with 1 second average"},
    {"scale cal <grams>", "alias for scale calibration <grams>"},
    {"scale factor <value>", "set scale calibration factor directly"},
    {"calibrate current low <A>", ""},
    {"calibrate current high <A>", ""},
    {"calibrate voltage low <V>", ""},
    {"calibrate voltage high <V>", ""},
    {"set current hi <A>", "warn when current reaches HI"},
    {"set current hihi <A>", "stop motor/test when current reaches HIHI"},
    {"set motor temp hi <C>", "warn when motor temperature reaches HI"},
    {"set motor temp hihi <C>", "stop motor/test when motor temperature reaches HIHI"},
    {"set esc temp hi <C>", "warn when ESC temperature reaches HI"},
    {"set esc temp hihi <C>", "stop motor/test when ESC temperature reaches HIHI"},
    {"safety show", "print current safety thresholds"},
    {"calibration show", ""},
    {"calibration reset", ""},
    {"ir status", "print MLX90614 status"},
    {"ir read", "read ambient and object temperatures"},
    {"wifi select", "scan networks, choose by number, then enter password"},
    {"wifi connect", "retry connecting to saved Wi-Fi networks"},
    {"wifi ap", "force Wi-Fi access point mode immediately"},
    {"wifi forget", "list saved networks and choose one to remove"},
    {"wifi status", "print current Wi-Fi mode, connection, and saved networks"},
    {"help", "show all commands"},
    {"help <text>", "search command help for matching text"}
};

bool commandHelpMatches(const CommandHelpEntry& entry, const String& filterLower) {
    if (filterLower.length() == 0) {
        return true;
    }

    String command = entry.command;
    command.toLowerCase();
    if (command.indexOf(filterLower) >= 0) {
        return true;
    }

    String description = entry.description;
    description.toLowerCase();
    return description.indexOf(filterLower) >= 0;
}

void printCommandHelpEntry(const CommandHelpEntry& entry) {
    Serial.print("  ");
    Serial.print(entry.command);
    if (strlen(entry.description) > 0) {
        Serial.print("                  -> ");
        Serial.println(entry.description);
        return;
    }

    Serial.println();
}

void printHelpFiltered(const String& filter) {
    String trimmedFilter = filter;
    trimmedFilter.trim();

    String filterLower = trimmedFilter;
    filterLower.toLowerCase();

    if (filterLower.length() == 0) {
        Serial.println("Commands:");
    } else {
        Serial.print("Help matches for \"");
        Serial.print(trimmedFilter);
        Serial.println("\":");
    }

    size_t matchCount = 0;
    for (const CommandHelpEntry& entry : kCommandHelpEntries) {
        if (!commandHelpMatches(entry, filterLower)) {
            continue;
        }

        printCommandHelpEntry(entry);
        ++matchCount;
    }

    if (matchCount == 0) {
        Serial.println("  No matching commands found.");
        return;
    }

    if (filterLower.length() > 0) {
        Serial.print("Found ");
        Serial.print(matchCount);
        Serial.println(" matching command(s).");
    }
}

bool parseIntegerArgument(const String& cmd, const char* prefix, int& value) {
    if (!cmd.startsWith(prefix)) {
        return false;
    }

    String valueText = cmd.substring(strlen(prefix));
    valueText.trim();
    if (valueText.length() == 0) {
        return false;
    }

    bool hasDigit = false;
    for (size_t i = 0; i < valueText.length(); ++i) {
        const char c = valueText.charAt(i);
        if (c >= '0' && c <= '9') {
            hasDigit = true;
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

    value = valueText.toInt();
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
    printHelpFiltered(String());
}

void handleCommand(String cmd) {
    cmd.trim();

    if (cmd.length() == 0) {
        return;
    }

    String cmdLower = cmd;
    cmdLower.toLowerCase();

    if (cmd.equalsIgnoreCase("wifi ap")) {
        forceAccessPointMode();
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

    if (cmd.equalsIgnoreCase("motor test stop")) {
        if (!testRunning) {
            Serial.println("No motor test is running");
            return;
        }

        requestMotorTestStop();
        Serial.println("Stopping motor test...");
        return;
    }

    if (cmd.equalsIgnoreCase("motor test cooling on")) {
        setMotorTestCooldownEnabled(true);
        Serial.println("Motor test cooldown enabled");
        return;
    }

    if (cmd.equalsIgnoreCase("motor test cooling off")) {
        setMotorTestCooldownEnabled(false);
        Serial.println("Motor test cooldown disabled");
        return;
    }

    if (cmd.equalsIgnoreCase("motor test mode puller")) {
        if (testRunning) {
            Serial.println("Cannot change motor test direction while a test is running");
            return;
        }
        setMotorTestPusherMode(false);
        Serial.println("Motor test direction set to puller");
        return;
    }

    if (cmd.equalsIgnoreCase("motor test mode pusher")) {
        if (testRunning) {
            Serial.println("Cannot change motor test direction while a test is running");
            return;
        }
        setMotorTestPusherMode(true);
        Serial.println("Motor test direction set to pusher");
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

    if (cmdLower.startsWith("help ")) {
        printHelpFiltered(cmd.substring(strlen("help ")));
        return;
    }

    if (cmd.equalsIgnoreCase("x")) {
        emergencyStopRamp();
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

    if (cmd.equalsIgnoreCase("motor test puller")) {
        runMotorTest(false);
        return;
    }

    if (cmd.equalsIgnoreCase("motor test pusher")) {
        runMotorTest(true);
        return;
    }

    if (cmd.equalsIgnoreCase("restart") || cmd.equalsIgnoreCase("reset")) {
        requestBoardRestart();
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

    int poleCount = 0;
    if (parseIntegerArgument(cmd, "esc poles ", poleCount)) {
        setEscMotorPolesAndSync(poleCount);
        return;
    }

    int motorKv = 0;
    if (parseIntegerArgument(cmd, "esc kv ", motorKv)) {
        setEscMotorKv(motorKv);
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

        if (value > 0.0f) {
            authorizeMotorOutput();
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

    if (parseScaleFactorCommand(cmd, calValue)) {
        setScaleCalibrationFactor(calValue);
        return;
    }

    if (cmd.equalsIgnoreCase("calibration show")) {
        printCalibrationStatus();
        printSafetyConfiguration();
        return;
    }

    if (cmd.equalsIgnoreCase("safety show")) {
        printSafetyConfiguration();
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

    if (parseCalibrationCommand(cmd, "set current hi ", calValue)) {
        setCurrentHiThreshold(calValue);
        return;
    }

    if (parseCalibrationCommand(cmd, "set current hihi ", calValue)) {
        setCurrentHiHiThreshold(calValue);
        return;
    }

    if (parseCalibrationCommand(cmd, "set motor temp hi ", calValue)) {
        setMotorTempHiThreshold(calValue);
        return;
    }

    if (parseCalibrationCommand(cmd, "set motor temp hihi ", calValue)) {
        setMotorTempHiHiThreshold(calValue);
        return;
    }

    if (parseCalibrationCommand(cmd, "set esc temp hi ", calValue)) {
        setEscTempHiThreshold(calValue);
        return;
    }

    if (parseCalibrationCommand(cmd, "set esc temp hihi ", calValue)) {
        setEscTempHiHiThreshold(calValue);
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
