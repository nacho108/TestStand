#pragma once

#include <Arduino.h>

bool beginTestStorage();
bool updateTelemetryDuringBlockingWait(unsigned long durationMs);
bool runMotorTest();
bool isMotorTestCooldownEnabled();
void setMotorTestCooldownEnabled(bool enabled);
void requestMotorTestStop();
bool isMotorTestStopRequested();
bool hasTestResults();
String getLastTestCsv();
String normalizeTestFilename(const String& filename);
bool saveLastTestToLittleFS(const String& filename);
void listSavedTests();
bool removeSavedTest(const String& filename);
bool printSavedTest(const String& filename);
String buildSavedTestsJson();
bool loadSavedTestCsv(const String& filename, String& csv);
