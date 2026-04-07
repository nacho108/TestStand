#pragma once

#include <Arduino.h>

static constexpr int ESC_PWM_PIN = 18;
static constexpr int ESC_TLM_RX_PIN = 16;

static constexpr int PWM_FREQ = 50;
static constexpr int PWM_CHANNEL = 0;
static constexpr int PWM_RESOLUTION = 16;

static constexpr float DEFAULT_SCALE_CAL_FACTOR = 1.0f;
static constexpr int32_t DEFAULT_SCALE_ZERO_OFFSET = 0;
static constexpr unsigned long SCALE_AVG_WINDOW_MS = 500;
static constexpr unsigned long SCALE_CAL_WINDOW_MS = 1000;
static constexpr unsigned long SCALE_TARE_WINDOW_MS = 1000;

static constexpr int CLI_HISTORY_SIZE = 10;

static constexpr float DEFAULT_VOLTAGE_SCALE = 1.0f;
static constexpr float DEFAULT_VOLTAGE_OFFSET = 0.0f;
static constexpr float DEFAULT_CURRENT_SCALE = 1.0f;
static constexpr float DEFAULT_CURRENT_OFFSET = 0.0f;

static constexpr int MOTOR_MAGNETS = 28;

static constexpr int TEST_STEP_PERCENT = 5;
static constexpr int TEST_MAX_RESULTS = 21;
static constexpr unsigned long AVAILABLE_LED_BLINK_MS = 500;

static constexpr const char* WEB_AP_SSID = "TestStand-ESP32";
static constexpr const char* WEB_AP_PASSWORD = "teststand";
