#include "web_server.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <math.h>
#include <Preferences.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#include "app_state.h"
#include "calibration.h"
#include "esc_telemetry.h"
#include "ir_manager.h"
#include "app_config.h"

namespace {

AsyncWebServer server(80);
AsyncWebSocket telemetrySocket("/ws");
bool webServerStarted = false;
unsigned long lastTelemetryBroadcastMs = 0;
unsigned long stationConnectStartMs = 0;
bool stationConnectAnnounced = false;
bool stationConnectedAnnounced = false;
bool apModeActive = false;

constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 30000;
constexpr int MAX_SCANNED_NETWORKS = 10;
String scannedSsids[MAX_SCANNED_NETWORKS];
int scannedNetworkCount = 0;
bool wifiSelectionIndexPending = false;
bool wifiSelectionPasswordPending = false;
String pendingWifiSsid;
String configuredWifiSsid;
String configuredWifiPassword;

void loadWifiConfiguration() {
    preferences.begin("am32cli", true);
    configuredWifiSsid = preferences.getString("wifi_ssid", "");
    configuredWifiPassword = preferences.getString("wifi_pass", "");
    preferences.end();
}

void saveWifiConfiguration(const String& ssid, const String& password) {
    preferences.begin("am32cli", false);
    preferences.putString("wifi_ssid", ssid);
    preferences.putString("wifi_pass", password);
    preferences.end();

    configuredWifiSsid = ssid;
    configuredWifiPassword = password;
}

void printStationConnectedMessage() {
    Serial.print("Connected to Wi-Fi network: ");
    Serial.println(WiFi.SSID());
    Serial.print("Open http://");
    Serial.println(WiFi.localIP());
}

bool startAccessPointMode() {
    const IPAddress apIp(192, 168, 4, 1);
    const IPAddress gateway(192, 168, 4, 1);
    const IPAddress subnet(255, 255, 255, 0);

    WiFi.disconnect(true, true);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_AP);

    if (!WiFi.softAPConfig(apIp, gateway, subnet)) {
        Serial.println("WARNING: Failed to configure Wi-Fi AP network.");
    }

    if (!WiFi.softAP(WEB_AP_SSID, WEB_AP_PASSWORD)) {
        Serial.println("WARNING: Failed to start Wi-Fi access point.");
        apModeActive = false;
        return false;
    }

    apModeActive = true;
    stationConnectStartMs = 0;
    stationConnectAnnounced = false;
    stationConnectedAnnounced = false;

    Serial.print("Wi-Fi AP started. SSID: ");
    Serial.println(WEB_AP_SSID);
    Serial.print("Wi-Fi password: ");
    Serial.println(WEB_AP_PASSWORD);
    Serial.print("Open http://");
    Serial.println(WiFi.softAPIP());
    return true;
}

void startStationMode(const String& ssid, const String& password, bool saveConfig) {
    if (saveConfig) {
        saveWifiConfiguration(ssid, password);
    }

    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    apModeActive = false;
    stationConnectStartMs = millis();
    stationConnectAnnounced = true;
    stationConnectedAnnounced = false;

    Serial.print("Connecting to Wi-Fi network: ");
    Serial.println(ssid);
    Serial.println("Waiting up to 30 seconds before falling back to AP mode...");
}

void ensureScanCapableWifiMode() {
    wifi_mode_t mode = WiFi.getMode();
    if (mode == WIFI_AP) {
        WiFi.mode(WIFI_AP_STA);
    } else if (mode == WIFI_OFF) {
        WiFi.mode(WIFI_STA);
    }
}

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
    appendJsonBool(json, "wifi_sta_connected", WiFi.status() == WL_CONNECTED);
    appendJsonBool(json, "wifi_ap_active", apModeActive);
    appendJsonUnsigned(json, "telemetry_last_update_ms", lastTlm.lastUpdateMs);
    appendJsonUnsigned(json, "scale_last_read_ms", lastScaleReadMs);
    appendJsonUnsigned(json, "ir_last_read_ms", lastIrReadMs, false);
    json += "}";
    return json;
}

void handleWebSocketEvent(
    AsyncWebSocket* socket,
    AsyncWebSocketClient* client,
    AwsEventType type,
    void* arg,
    uint8_t* data,
    size_t len
) {
    (void)socket;
    (void)arg;
    (void)data;
    (void)len;

    if (type == WS_EVT_CONNECT) {
        client->text(buildStatusJson());
        return;
    }

    if (type == WS_EVT_DISCONNECT) {
        Serial.print("WebSocket client disconnected: ");
        Serial.println(client->id());
    }
}

}

bool beginWebServer() {
    if (webServerStarted) {
        return true;
    }

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(LittleFS, "/index.html", "text/html");
    });

    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "application/json", buildStatusJson());
    });

    telemetrySocket.onEvent(handleWebSocketEvent);
    server.addHandler(&telemetrySocket);

    server.serveStatic("/", LittleFS, "/")
        .setDefaultFile("index.html");

    server.onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "text/plain", "Not found");
    });

    loadWifiConfiguration();
    if (configuredWifiSsid.length() > 0) {
        startStationMode(configuredWifiSsid, configuredWifiPassword, false);
    } else if (!startAccessPointMode()) {
        return false;
    }

    server.begin();
    webServerStarted = true;

    return true;
}

void updateWebServer() {
    if (!webServerStarted) {
        return;
    }

    if (!apModeActive) {
        if (WiFi.status() == WL_CONNECTED) {
            if (!stationConnectedAnnounced) {
                printStationConnectedMessage();
                stationConnectedAnnounced = true;
            }
        } else if (stationConnectStartMs != 0 && millis() - stationConnectStartMs >= WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println("Wi-Fi station connect timed out. Falling back to AP mode.");
            startAccessPointMode();
        }
    }

    telemetrySocket.cleanupClients();

    const unsigned long nowMs = millis();
    if (nowMs - lastTelemetryBroadcastMs < 250) {
        return;
    }

    lastTelemetryBroadcastMs = nowMs;
    telemetrySocket.textAll(buildStatusJson());
}

void beginWifiSelection() {
    ensureScanCapableWifiMode();

    wifiSelectionIndexPending = false;
    wifiSelectionPasswordPending = false;
    pendingWifiSsid = "";
    scannedNetworkCount = 0;

    Serial.println("Scanning Wi-Fi networks...");
    int networkCount = WiFi.scanNetworks(false, true);
    if (networkCount <= 0) {
        Serial.println("No Wi-Fi networks found.");
        return;
    }

    for (int i = 0; i < networkCount && scannedNetworkCount < MAX_SCANNED_NETWORKS; ++i) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) {
            continue;
        }

        bool duplicate = false;
        for (int j = 0; j < scannedNetworkCount; ++j) {
            if (scannedSsids[j] == ssid) {
                duplicate = true;
                break;
            }
        }

        if (duplicate) {
            continue;
        }

        scannedSsids[scannedNetworkCount] = ssid;
        ++scannedNetworkCount;
    }

    WiFi.scanDelete();

    if (scannedNetworkCount == 0) {
        Serial.println("No named Wi-Fi networks found.");
        return;
    }

    Serial.println("Available Wi-Fi networks:");
    for (int i = 0; i < scannedNetworkCount; ++i) {
        Serial.print("  ");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.println(scannedSsids[i]);
    }

    wifiSelectionIndexPending = true;
    Serial.println("Choose Wi-Fi network number:");
}

bool handleWifiSelectionInput(const String& input) {
    if (!wifiSelectionIndexPending && !wifiSelectionPasswordPending) {
        return false;
    }

    if (wifiSelectionIndexPending) {
        String trimmed = input;
        trimmed.trim();
        int selectedNumber = trimmed.toInt();
        if (selectedNumber < 1 || selectedNumber > scannedNetworkCount) {
            Serial.println("Invalid Wi-Fi selection. Enter one of the listed numbers:");
            return true;
        }

        pendingWifiSsid = scannedSsids[selectedNumber - 1];
        wifiSelectionIndexPending = false;
        wifiSelectionPasswordPending = true;
        Serial.print("Enter password for ");
        Serial.print(pendingWifiSsid);
        Serial.println(":");
        return true;
    }

    if (wifiSelectionPasswordPending) {
        String password = input;
        wifiSelectionPasswordPending = false;
        startStationMode(pendingWifiSsid, password, true);
        pendingWifiSsid = "";
        return true;
    }

    return false;
}

bool wifiSelectionPending() {
    return wifiSelectionIndexPending || wifiSelectionPasswordPending;
}

void printWifiStatus() {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("Wi-Fi STA connected to ");
        Serial.print(WiFi.SSID());
        Serial.print(" at ");
        Serial.println(WiFi.localIP());
        return;
    }

    if (apModeActive) {
        Serial.print("Wi-Fi AP mode active at ");
        Serial.println(WiFi.softAPIP());
        return;
    }

    if (stationConnectStartMs != 0) {
        Serial.print("Connecting to Wi-Fi network: ");
        Serial.println(configuredWifiSsid);
        return;
    }

    Serial.println("Wi-Fi not configured. AP fallback will be used.");
}
