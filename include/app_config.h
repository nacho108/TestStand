#pragma once

#include <Arduino.h>

static constexpr int ESC_PWM_PIN = 25;
static constexpr int ESC_TLM_RX_PIN = 26;

static constexpr bool ENABLE_SENSOR_SIMULATION = false;
static constexpr unsigned long SENSOR_SIMULATION_UPDATE_MS = 50;
static constexpr float SENSOR_SIMULATION_AMBIENT_TEMP_C = 24.0f;
static constexpr float SENSOR_SIMULATION_SCALE_STDDEV_G = 0.0f;

static constexpr int PWM_FREQ = 50;
static constexpr int PWM_CHANNEL = 0;
static constexpr int PWM_RESOLUTION = 16;

static constexpr float DEFAULT_SCALE_CAL_FACTOR = -106.461f;
static constexpr int32_t DEFAULT_SCALE_ZERO_OFFSET = 0;
static constexpr unsigned long SCALE_AVG_WINDOW_MS = 500;
static constexpr unsigned long SCALE_CAL_WINDOW_MS = 1000;
static constexpr unsigned long SCALE_TARE_WINDOW_MS = 1000;
static constexpr uint32_t SCALE_STARTUP_STABLE_MIN_SAMPLES = 50;
static constexpr float SCALE_STARTUP_STABLE_STDDEV_G = 2.0f;

static constexpr int CLI_HISTORY_SIZE = 10;

static constexpr float DEFAULT_VOLTAGE_SCALE = 1.0f;
static constexpr float DEFAULT_VOLTAGE_OFFSET = 0.0f;
static constexpr float DEFAULT_CURRENT_SCALE = 1.0f;
static constexpr float DEFAULT_CURRENT_OFFSET = 0.0f;

static constexpr int DEFAULT_MOTOR_POLES = 28;

static constexpr int TEST_THROTTLE_LEVELS[] = {0, 5, 10, 20, 30, 35, 40, 45, 50, 55, 60, 65, 70, 80, 90, 100};
static constexpr int TEST_MAX_RESULTS = sizeof(TEST_THROTTLE_LEVELS) / sizeof(TEST_THROTTLE_LEVELS[0]);
static constexpr unsigned long AVAILABLE_LED_BLINK_MS = 500;

static constexpr const char* WEB_AP_SSID = "TestStand-ESP32";
static constexpr const char* WEB_AP_PASSWORD = "teststand";
