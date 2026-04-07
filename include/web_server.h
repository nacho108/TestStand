#pragma once

#include <Arduino.h>

bool beginWebServer();
void updateWebServer();
bool queueWebCommand(const String& cmd);
void processQueuedWebCommand();
void beginWifiSelection();
void beginWifiForget();
bool handleWifiSelectionInput(const String& input);
bool wifiSelectionPending();
void printWifiStatus();
