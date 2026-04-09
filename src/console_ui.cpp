#include "console_ui.h"

#include "app_state.h"
#include "commands.h"
#include "motor_control.h"

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
        historyBrowseIndex = historyCount - 1;
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

void readConsole() {
    while (Serial.available()) {
        char c = (char)Serial.read();

        if (c == '\n' && lastConsoleCharWasCarriageReturn) {
            lastConsoleCharWasCarriageReturn = false;
            continue;
        }

        lastConsoleCharWasCarriageReturn = (c == '\r');

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
        } else if (c == '\r' || c == '\n') {
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
