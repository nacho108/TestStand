#pragma once

#include <Arduino.h>

bool beginWebServer();
void updateWebServer();
void beginWifiSelection();
bool handleWifiSelectionInput(const String& input);
bool wifiSelectionPending();
void printWifiStatus();
