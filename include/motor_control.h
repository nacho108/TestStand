#pragma once

#include <Arduino.h>

void beginMotorControl();
uint32_t microsecondsToDuty(int us);
void writeThrottlePercent(float percent);
void cancelRamp();
void startRamp(float targetPercent, unsigned long durationMs);
unsigned long calculateThrottleRampDurationMs(float currentPercent, float targetPercent);
void updateRamp();
void stopMotorImmediate();
void stopMotorSlow();
void emergencyStopRamp();
void startMotorAtFivePercent();
void rampToFullPower();
