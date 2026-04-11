#pragma once

#include <Arduino.h>

#include "app_types.h"

bool beginAlarmManager();
void logSafetyAlarm(const char* sourceKey, SafetyLevel level, float value, float threshold, const char* unit);
void logSystemEvent(const char* source, const char* message, const char* severity = "INFO");
size_t getUnreadAlarmCount();
size_t getTotalAlarmCount();
void markAllAlarmsRead();
String buildRecentAlarmsJson(size_t limit = 10);
