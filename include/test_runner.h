#pragma once

#include <Arduino.h>

bool beginTestStorage();
bool updateTelemetryDuringBlockingWait(unsigned long durationMs);
bool runMotorTest();
bool hasTestResults();
String normalizeTestFilename(const String& filename);
bool saveLastTestToLittleFS(const String& filename);
void listSavedTests();
bool removeSavedTest(const String& filename);
bool printSavedTest(const String& filename);
