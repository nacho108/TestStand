#include "app_state.h"

HardwareSerial escSerial(2);
Preferences preferences;

Adafruit_MLX90614 irSensor = Adafruit_MLX90614();
bool irDetected = false;
float lastIrAmbientC = NAN;
float lastIrObjectC = NAN;
unsigned long lastIrReadMs = 0;

NAU7802 scale;
bool scaleDetected = false;
unsigned long lastScaleReadMs = 0;
int32_t lastScaleRaw = 0;
float lastScaleWeight = 0.0f;
float lastScaleStdDev = 0.0f;
bool lastScaleSampleValid = false;

String inputBuffer;
float throttlePercent = 0.0f;
bool telemetryStreaming = false;
bool promptShown = false;
bool lastConsoleCharWasCarriageReturn = false;

String commandHistory[CLI_HISTORY_SIZE];
int historyCount = 0;
int historyWriteIndex = 0;
int historyBrowseIndex = -1;
String historyEditBuffer;

RampState ramp;
EscTelemetry lastTlm;
LinearCalibration voltageCal;
LinearCalibration currentCal;

TestResultRow testResults[TEST_MAX_RESULTS];
int testResultCount = 0;
bool testRunning = false;
bool testSavePromptPending = false;
bool testFilenamePromptPending = false;
