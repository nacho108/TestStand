#pragma once

#include <Arduino.h>

bool beginWebServer();
void updateWebServer();
void beginWifiSelection();
void beginWifiForget();
bool handleWifiSelectionInput(const String& input);
bool wifiSelectionPending();
void printWifiStatus();
