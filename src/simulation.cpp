#include "simulation.h"

#include <Arduino.h>
#include <math.h>

#include "app_config.h"
#include "app_state.h"

namespace {

constexpr float THRUST_NOISE_GRAMS = 1.0f;
constexpr float THRUST_ZERO_OFFSET_NOISE_GRAMS = 0.5f;
constexpr float RPM_NOISE = 5.0f;
constexpr float VOLTAGE_NOISE_VOLTS = 0.1f;
constexpr float CURRENT_NOISE_AMPS = 0.2f;

float evaluateCubic(float a, float b, float c, float d, float x) {
    return ((a * x + b) * x + c) * x + d;
}

float clampToNonNegative(float value) {
    return value < 0.0f ? 0.0f : value;
}

float randomSignedNoise(float amplitude) {
    if (amplitude <= 0.0f) {
        return 0.0f;
    }

    long sample = random(-1000, 1001);
    return ((float)sample / 1000.0f) * amplitude;
}

uint16_t clampToUint16FromScaled(float value, float scale) {
    const float scaled = value * scale;
    if (scaled <= 0.0f) {
        return 0;
    }

    if (scaled >= 65535.0f) {
        return 65535;
    }

    return (uint16_t)lroundf(scaled);
}

uint8_t clampToUint8(float value) {
    if (value <= 0.0f) {
        return 0;
    }

    if (value >= 255.0f) {
        return 255;
    }

    return (uint8_t)lroundf(value);
}

void fillSimulatedValues(float throttle, SimulatedSensorValues& outValues) {
    const float thrustBase = clampToNonNegative(
        evaluateCubic(-8.6908e-4f, 0.4395f, 21.778f, -202.83f, throttle)
    );
    outValues.thrustGrams = thrustBase
        + randomSignedNoise(THRUST_NOISE_GRAMS)
        + randomSignedNoise(THRUST_ZERO_OFFSET_NOISE_GRAMS);
    outValues.rpm = throttle <= 0.0f
        ? 0.0f
        : clampToNonNegative(
            evaluateCubic(2.3329e-4f, -0.1905f, 63.682f, 25.633f, throttle)
            + randomSignedNoise(RPM_NOISE)
        );
    outValues.voltageV = clampToNonNegative(
        evaluateCubic(-5.1282e-7f, -1.2826e-4f, 7.9895e-4f, 25.031f, throttle)
        + randomSignedNoise(VOLTAGE_NOISE_VOLTS)
    );
    outValues.currentA = clampToNonNegative(
        evaluateCubic(2.2403e-5f, 8.5542e-4f, 0.04027f, -0.476f, throttle)
        + randomSignedNoise(CURRENT_NOISE_AMPS)
    );
    outValues.escTemperatureC = clampToNonNegative(
        evaluateCubic(-6.3131e-5f, 0.01636f, -0.3891f, 28.50f, throttle)
    );
    outValues.motorTemperatureC = clampToNonNegative(
        evaluateCubic(-7.5758e-5f, 0.0196f, -0.4670f, 34.20f, throttle)
    );
    outValues.ambientTemperatureC = SENSOR_SIMULATION_AMBIENT_TEMP_C;
}

void writeSimulationToSharedState(const SimulatedSensorValues& values, unsigned long nowMs) {
    simulatedSensors = values;

    lastTlm.valid = true;
    lastTlm.temperatureC = clampToUint8(values.escTemperatureC);
    lastTlm.voltageRaw = clampToUint16FromScaled(values.voltageV, 100.0f);
    lastTlm.currentRaw = clampToUint16FromScaled(values.currentA, 100.0f);
    lastTlm.consumptionRaw = 0;
    lastTlm.rpmField = clampToUint16FromScaled(values.rpm * (MOTOR_MAGNETS / 2.0f), 0.01f);
    lastTlm.lastUpdateMs = nowMs;

    irDetected = true;
    lastIrAmbientC = values.ambientTemperatureC;
    lastIrObjectC = values.motorTemperatureC;
    lastIrReadMs = nowMs;

    scaleDetected = true;
    lastScaleReadMs = nowMs;
    lastScaleRaw = (int32_t)lroundf(values.thrustGrams);
    lastScaleWeight = values.thrustGrams;
    lastScaleStdDev = SENSOR_SIMULATION_SCALE_STDDEV_G;
    lastScaleWindowSampleCount = 1;
    lastScaleSampleValid = true;
}

}

bool simulationEnabled() {
    return ENABLE_SENSOR_SIMULATION;
}

void beginSimulation() {
    if (!simulationEnabled()) {
        return;
    }

    randomSeed(micros());
    updateSimulation();
}

void updateSimulation() {
    if (!simulationEnabled()) {
        return;
    }

    static unsigned long lastUpdateMs = 0;
    const unsigned long nowMs = millis();

    if (lastUpdateMs != 0 && nowMs - lastUpdateMs < SENSOR_SIMULATION_UPDATE_MS) {
        return;
    }

    lastUpdateMs = nowMs;

    SimulatedSensorValues values;
    const float throttle = constrain(throttlePercent, 0.0f, 100.0f);
    fillSimulatedValues(throttle, values);
    writeSimulationToSharedState(values, nowMs);
}

const SimulatedSensorValues& getSimulatedSensorValues() {
    return simulatedSensors;
}
