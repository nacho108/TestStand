#pragma once

#include <Arduino.h>

void showPrompt();
void redrawInputLine();
void addCommandToHistory(const String& cmd);
String getHistoryEntryByBrowseIndex(int browseIndex);
void browseHistoryUp();
void browseHistoryDown();
void resetHistoryBrowsing();
void readConsole();
