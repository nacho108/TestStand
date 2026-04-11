#pragma once

#include <Arduino.h>

void setCurrentTimeFromEpochMs(uint64_t epochMs);
bool hasCurrentTimeSync();
uint64_t getCurrentTimeMs();
