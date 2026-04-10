#pragma once

#include <Arduino.h>

void beginScaleManager();
void pollScale();
bool isScaleStartupSequenceComplete();
bool parseScaleCalibrationCommand(const String& cmd, float& outValue);
bool parseScaleFactorCommand(const String& cmd, float& outValue);
void saveScaleCalibration();
void loadScaleCalibration();
float rawToWeightGrams(int32_t raw);
uint32_t getScaleWindowSampleCount();
bool acquireAveragedScaleSample(
    unsigned long durationMs,
    int32_t& avgRaw,
    float& avgWeight,
    float& stddevWeight,
    uint32_t& sampleCount
);
void printScaleStatus();
void printScaleReading();
bool tareScale();
void calibrateScale(float knownWeightGrams);
bool setScaleCalibrationFactor(float newFactor);
