#pragma once

#include <Arduino.h>

struct RampState {
    bool active = false;
    float startPercent = 0.0f;
    float targetPercent = 0.0f;
    unsigned long startMs = 0;
    unsigned long durationMs = 0;
};

struct EscTelemetry {
    bool valid = false;
    uint8_t temperatureC = 0;
    uint16_t voltageRaw = 0;
    uint16_t currentRaw = 0;
    uint16_t consumptionRaw = 0;
    uint16_t rpmField = 0;
    unsigned long lastUpdateMs = 0;
};

struct LinearCalibration {
    float scale = 1.0f;
    float offset = 0.0f;

    bool lowCaptured = false;
    bool highCaptured = false;
    float lowRaw = 0.0f;
    float lowReal = 0.0f;
    float highRaw = 0.0f;
    float highReal = 0.0f;
};

struct TestResultRow {
    int throttlePercent = 0;
    float voltageV = 0.0f;
    float currentA = 0.0f;
    float powerW = 0.0f;
    float rpm = 0.0f;
    float temperatureC = 0.0f;
    float weightGrams = 0.0f;
    uint32_t sampleCount = 0;
    uint32_t scaleSampleCount = 0;
};
