#include "motor_control.h"

#include <math.h>

#include "app_config.h"
#include "app_state.h"

namespace {
void driveEscSignalLow() {
    pinMode(ESC_PWM_PIN, OUTPUT);
    digitalWrite(ESC_PWM_PIN, LOW);
}
}  // namespace

void beginMotorControl() {
    driveEscSignalLow();
    delay(20);
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcWrite(PWM_CHANNEL, microsecondsToDuty(1000));
    ledcAttachPin(ESC_PWM_PIN, PWM_CHANNEL);
    stopMotorImmediate();
}

uint32_t microsecondsToDuty(int us) {
    return (uint32_t)((((uint64_t)us) * ((1UL << PWM_RESOLUTION) - 1)) / 20000ULL);
}

void writeThrottlePercent(float percent) {
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;

    throttlePercent = percent;

    int pulseUs = 1000 + (int)(percent * 10.0f);
    uint32_t duty = microsecondsToDuty(pulseUs);
    ledcWrite(PWM_CHANNEL, duty);
}

void cancelRamp() {
    ramp.active = false;
}

void startRamp(float targetPercent, unsigned long durationMs) {
    if (targetPercent < 0.0f) targetPercent = 0.0f;
    if (targetPercent > 100.0f) targetPercent = 100.0f;

    if (durationMs == 0) {
        cancelRamp();
        writeThrottlePercent(targetPercent);
        return;
    }

    ramp.active = true;
    ramp.startPercent = throttlePercent;
    ramp.targetPercent = targetPercent;
    ramp.startMs = millis();
    ramp.durationMs = durationMs;
}

unsigned long calculateThrottleRampDurationMs(float currentPercent, float targetPercent) {
    float deltaPercent = fabsf(targetPercent - currentPercent);
    return (unsigned long)((deltaPercent / 30.0f) * 1000.0f);
}

void updateRamp() {
    if (!ramp.active) {
        return;
    }

    unsigned long now = millis();
    unsigned long elapsed = now - ramp.startMs;

    if (elapsed >= ramp.durationMs) {
        writeThrottlePercent(ramp.targetPercent);
        ramp.active = false;
        return;
    }

    float t = (float)elapsed / (float)ramp.durationMs;
    float newPercent = ramp.startPercent + (ramp.targetPercent - ramp.startPercent) * t;
    writeThrottlePercent(newPercent);
}

void stopMotorImmediate() {
    cancelRamp();
    writeThrottlePercent(0.0f);
}

void stopMotorSlow() {
    startRamp(0.0f, 3000);
}

void emergencyStopRamp() {
    unsigned long durationMs = calculateThrottleRampDurationMs(throttlePercent, 0.0f);
    startRamp(0.0f, durationMs);
    Serial.println();
    Serial.print("EMERGENCY: motor ramping down to 0%");
    if (durationMs > 0) {
        Serial.print(" in ");
        Serial.print(durationMs);
        Serial.println(" ms");
    } else {
        Serial.println();
    }
}

void startMotorAtFivePercent() {
    cancelRamp();
    writeThrottlePercent(5.0f);
}

void rampToFullPower() {
    startRamp(100.0f, 10000);
}
