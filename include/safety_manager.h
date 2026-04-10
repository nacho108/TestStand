#pragma once

#include <Arduino.h>

#include "app_types.h"

void loadSafetyConfiguration();
void saveSafetyConfiguration();
void printSafetyConfiguration();

bool setCurrentHiThreshold(float value);
bool setCurrentHiHiThreshold(float value);
bool setMotorTempHiThreshold(float value);
bool setMotorTempHiHiThreshold(float value);
bool setEscTempHiThreshold(float value);
bool setEscTempHiHiThreshold(float value);

void updateSafetyStatus();
bool consumeSafetyMotorTestStopRequest(String& outReason);
