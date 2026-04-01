#pragma once

#include <Arduino.h>

#include "app_types.h"

float getRawVoltageVolts();
float getRawCurrentAmps();
float applyCalibration(float rawValue, const LinearCalibration& cal);
float getCalibratedVoltageVolts();
float getCalibratedCurrentAmps();
float getCalibratedPowerWatts();
bool recomputeCalibration(LinearCalibration& cal);
void setCalibrationDefaults();
void saveCalibration();
void loadCalibration();
void resetCalibration();
void printCalibrationStatus();
bool parseCalibrationCommand(const String& cmd, const char* prefix, float& outValue);
void captureVoltageLow(float realValue);
void captureVoltageHigh(float realValue);
void captureCurrentLow(float realValue);
void captureCurrentHigh(float realValue);
