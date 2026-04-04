#include "web_server.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <math.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#include "app_state.h"
#include "calibration.h"
#include "esc_telemetry.h"
#include "ir_manager.h"
#include "app_config.h"

namespace {

AsyncWebServer server(80);
bool webServerStarted = false;

void appendJsonBool(String& json, const char* key, bool value, bool withComma = true) {
    json += "\"";
    json += key;
    json += "\":";
    json += value ? "true" : "false";
    if (withComma) {
        json += ",";
    }
}

void appendJsonUnsigned(String& json, const char* key, unsigned long value, bool withComma = true) {
    json += "\"";
    json += key;
    json += "\":";
    json += String(value);
    if (withComma) {
        json += ",";
    }
}

void appendJsonFloat(String& json, const char* key, float value, int decimals, bool withComma = true) {
    json += "\"";
    json += key;
    json += "\":";
    if (isnan(value)) {
        json += "null";
    } else {
        json += String(value, decimals);
    }
    if (withComma) {
        json += ",";
    }
}

String buildStatusJson() {
    float irAmbient = lastIrAmbientC;
    float irObject = lastIrObjectC;

    if (irDetected) {
        float ambientRead = NAN;
        float objectRead = NAN;
        if (readIrTemperatures(ambientRead, objectRead)) {
            irAmbient = ambientRead;
            irObject = objectRead;
        }
    }

    const bool telemetryValid = lastTlm.valid;
    const float voltageV = telemetryValid ? getCalibratedVoltageVolts() : NAN;
    const float currentA = telemetryValid ? getCalibratedCurrentAmps() : NAN;
    const float powerW = telemetryValid ? getCalibratedPowerWatts() : NAN;
    const float rpm = telemetryValid ? estimateMechanicalRpm(lastTlm.rpmField, MOTOR_MAGNETS) : NAN;
    const float escTemperatureC = telemetryValid ? (float)lastTlm.temperatureC : NAN;
    const float thrustGrams = lastScaleSampleValid ? lastScaleWeight : NAN;
    const unsigned long nowMs = millis();

    String json;
    json.reserve(512);
    json += "{";
    appendJsonUnsigned(json, "timestamp_ms", nowMs);
    appendJsonBool(json, "telemetry_valid", telemetryValid);
    appendJsonFloat(json, "voltage_v", voltageV, 3);
    appendJsonFloat(json, "current_a", currentA, 3);
    appendJsonFloat(json, "power_w", powerW, 3);
    appendJsonFloat(json, "rpm", rpm, 1);
    appendJsonFloat(json, "esc_temperature_c", escTemperatureC, 2);
    appendJsonBool(json, "ir_detected", irDetected);
    appendJsonFloat(json, "ir_ambient_c", irAmbient, 2);
    appendJsonFloat(json, "ir_object_c", irObject, 2);
    appendJsonBool(json, "scale_detected", scaleDetected);
    appendJsonBool(json, "scale_valid", lastScaleSampleValid);
    appendJsonFloat(json, "thrust_grams", thrustGrams, 3);
    appendJsonFloat(json, "weight_grams", thrustGrams, 3);
    appendJsonUnsigned(json, "telemetry_last_update_ms", lastTlm.lastUpdateMs);
    appendJsonUnsigned(json, "scale_last_read_ms", lastScaleReadMs);
    appendJsonUnsigned(json, "ir_last_read_ms", lastIrReadMs, false);
    json += "}";
    return json;
}

}

bool beginWebServer() {
    if (webServerStarted) {
        return true;
    }

    const IPAddress apIp(192, 168, 4, 1);
    const IPAddress gateway(192, 168, 4, 1);
    const IPAddress subnet(255, 255, 255, 0);

    WiFi.mode(WIFI_AP);
    WiFi.softAPdisconnect(true);

    if (!WiFi.softAPConfig(apIp, gateway, subnet)) {
        Serial.println("WARNING: Failed to configure Wi-Fi AP network.");
    }

    if (!WiFi.softAP(WEB_AP_SSID, WEB_AP_PASSWORD)) {
        Serial.println("WARNING: Failed to start Wi-Fi access point.");
        return false;
    }

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(LittleFS, "/index.html", "text/html");
    });

    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "application/json", buildStatusJson());
    });

    server.serveStatic("/", LittleFS, "/")
        .setDefaultFile("index.html");

    server.onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "text/plain", "Not found");
    });

    server.begin();
    webServerStarted = true;

    Serial.print("Wi-Fi AP started. SSID: ");
    Serial.println(WEB_AP_SSID);
    Serial.print("Wi-Fi password: ");
    Serial.println(WEB_AP_PASSWORD);
    Serial.print("Open http://");
    Serial.println(WiFi.softAPIP());

    return true;
}
