#pragma once

#include <Arduino.h>

void beginScaleManager();
void pollScale();
bool parseScaleCalibrationCommand(const String& cmd, float& outValue);
void saveScaleCalibration();
void loadScaleCalibration();
float rawToWeightGrams(int32_t raw);
bool acquireAveragedScaleSample(
    unsigned long durationMs,
    int32_t& avgRaw,
    float& avgWeight,
    float& stddevWeight,
    uint32_t& sampleCount
);
void printScaleStatus();
void printScaleReading();
void tareScale();
void calibrateScale(float knownWeightGrams);
