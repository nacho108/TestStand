#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>
#include <Preferences.h>
#include <SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h>
#include <Adafruit_MLX90614.h>

#include "app_config.h"
#include "app_types.h"

extern HardwareSerial escSerial;
extern Preferences preferences;

extern Adafruit_MLX90614 irSensor;
extern bool irDetected;
extern float lastIrAmbientC;
extern float lastIrObjectC;
extern unsigned long lastIrReadMs;

extern NAU7802 scale;
extern bool scaleDetected;
extern unsigned long lastScaleReadMs;
extern int32_t lastScaleRaw;
extern float lastScaleWeight;
extern float lastScaleStdDev;
extern uint32_t lastScaleWindowSampleCount;
extern bool lastScaleSampleValid;

extern String inputBuffer;
extern float throttlePercent;
extern bool telemetryStreaming;
extern bool promptShown;
extern bool lastConsoleCharWasCarriageReturn;

extern String commandHistory[CLI_HISTORY_SIZE];
extern int historyCount;
extern int historyWriteIndex;
extern int historyBrowseIndex;
extern String historyEditBuffer;

extern RampState ramp;
extern EscTelemetry lastTlm;
extern LinearCalibration voltageCal;
extern LinearCalibration currentCal;

extern TestResultRow testResults[TEST_MAX_RESULTS];
extern int testResultCount;
extern bool testRunning;
extern bool testSavePromptPending;
extern bool testFilenamePromptPending;
