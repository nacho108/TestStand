#include "web_server.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <math.h>
#include <Preferences.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <freertos/FreeRTOS.h>

#include "app_state.h"
#include "alarm_manager.h"
#include "calibration.h"
#include "commands.h"
#include "console_ui.h"
#include "esc_telemetry.h"
#include "ir_manager.h"
#include "scale_manager.h"
#include "safety_manager.h"
#include "app_config.h"
#include "test_runner.h"

namespace {

AsyncWebServer server(80);
AsyncWebSocket telemetrySocket("/ws");
bool webServerStarted = false;
unsigned long lastTelemetryBroadcastMs = 0;
unsigned long stationConnectStartMs = 0;
bool stationConnectAnnounced = false;
bool stationConnectedAnnounced = false;
bool apModeActive = false;
portMUX_TYPE queuedWebCommandMux = portMUX_INITIALIZER_UNLOCKED;
String queuedWebCommand;
bool queuedWebCommandPending = false;

constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 45000;
constexpr int WIFI_AP_FALLBACK_RETRY_CYCLES = 3;
constexpr int MAX_SCANNED_NETWORKS = 10;
constexpr int MAX_SAVED_WIFI_NETWORKS = 8;
constexpr const char* WIFI_PREF_NAMESPACE = "am32cli";
constexpr const char* WIFI_PREF_COUNT_KEY = "wifi_count";
String scannedSsids[MAX_SCANNED_NETWORKS];
int scannedNetworkCount = 0;
bool wifiSelectionIndexPending = false;
bool wifiSelectionPasswordPending = false;
bool wifiForgetIndexPending = false;
String pendingWifiSsid;
String savedWifiSsids[MAX_SAVED_WIFI_NETWORKS];
String savedWifiPasswords[MAX_SAVED_WIFI_NETWORKS];
int savedWifiCount = 0;
int connectingWifiIndex = -1;
int stationRetryCycles = 0;
bool webUiAssetsReady = false;
constexpr unsigned long TELEMETRY_BROADCAST_INTERVAL_MS = 333;

String buildStatusJson(bool includeTestResults);

void broadcastLatestTelemetrySnapshot() {
    String statusJson;
    bool statusJsonBuilt = false;

    for (AsyncWebSocketClient& wsClient : telemetrySocket.getClients()) {
        if (wsClient.status() != WS_CONNECTED || wsClient.queueIsFull()) {
            continue;
        }

        if (!statusJsonBuilt) {
            statusJson = buildStatusJson(false);
            statusJsonBuilt = true;
        }

        wsClient.text(statusJson);
    }
}

void printSocketClientContext(const AsyncWebSocketClient* client) {
    if (client == nullptr) {
        return;
    }

    Serial.print(" id=");
    Serial.print(client->id());
    Serial.print(" ip=");
    Serial.print(client->remoteIP());
    Serial.print(" heap=");
    Serial.print(ESP.getFreeHeap());
    Serial.print(" ws_clients=");
    Serial.print(telemetrySocket.count());
    Serial.print(" wifi_status=");
    Serial.print((int)WiFi.status());
}

void ensureScanCapableWifiMode();
void handleWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info);

bool checkWebUiAsset(const char* path) {
    if (LittleFS.exists(path)) {
        return true;
    }

    Serial.print("Missing LittleFS web asset: ");
    Serial.println(path);
    return false;
}

bool validateWebUiAssets() {
    bool ok = true;
    ok = checkWebUiAsset("/index.html") && ok;
    ok = checkWebUiAsset("/styles.css") && ok;
    ok = checkWebUiAsset("/app.js") && ok;

    if (!ok) {
        Serial.println("Web UI files are missing from LittleFS.");
        Serial.println("Rebuild and upload the filesystem image with the PlatformIO 'Upload Filesystem Image' task.");
    }

    return ok;
}

String makeWifiSsidKey(int index) {
    return "wifi_ssid_" + String(index);
}

String makeWifiPasswordKey(int index) {
    return "wifi_pass_" + String(index);
}

void clearSavedWifiCache() {
    for (int i = 0; i < MAX_SAVED_WIFI_NETWORKS; ++i) {
        savedWifiSsids[i] = "";
        savedWifiPasswords[i] = "";
    }
    savedWifiCount = 0;
}

void loadWifiConfiguration() {
    clearSavedWifiCache();

    preferences.begin(WIFI_PREF_NAMESPACE, true);
    int count = preferences.getInt(WIFI_PREF_COUNT_KEY, -1);

    if (count < 0) {
        String legacySsid = preferences.getString("wifi_ssid", "");
        String legacyPassword = preferences.getString("wifi_pass", "");
        if (legacySsid.length() > 0) {
            savedWifiSsids[0] = legacySsid;
            savedWifiPasswords[0] = legacyPassword;
            savedWifiCount = 1;
        }
        preferences.end();
        return;
    }

    count = constrain(count, 0, MAX_SAVED_WIFI_NETWORKS);
    for (int i = 0; i < count; ++i) {
        String ssid = preferences.getString(makeWifiSsidKey(i).c_str(), "");
        String password = preferences.getString(makeWifiPasswordKey(i).c_str(), "");
        if (ssid.length() == 0) {
            continue;
        }

        savedWifiSsids[savedWifiCount] = ssid;
        savedWifiPasswords[savedWifiCount] = password;
        ++savedWifiCount;
    }

    preferences.end();
}

void persistWifiConfiguration() {
    preferences.begin(WIFI_PREF_NAMESPACE, false);
    preferences.putInt(WIFI_PREF_COUNT_KEY, savedWifiCount);
    for (int i = 0; i < MAX_SAVED_WIFI_NETWORKS; ++i) {
        const String ssidKey = makeWifiSsidKey(i);
        const String passwordKey = makeWifiPasswordKey(i);
        if (i < savedWifiCount) {
            preferences.putString(ssidKey.c_str(), savedWifiSsids[i]);
            preferences.putString(passwordKey.c_str(), savedWifiPasswords[i]);
        } else {
            preferences.remove(ssidKey.c_str());
            preferences.remove(passwordKey.c_str());
        }
    }

    preferences.remove("wifi_ssid");
    preferences.remove("wifi_pass");
    preferences.end();
}

void saveWifiConfiguration(const String& ssid, const String& password) {
    int existingIndex = -1;
    for (int i = 0; i < savedWifiCount; ++i) {
        if (savedWifiSsids[i] == ssid) {
            existingIndex = i;
            break;
        }
    }

    if (existingIndex >= 0) {
        savedWifiPasswords[existingIndex] = password;

        if (existingIndex > 0) {
            String updatedSsid = savedWifiSsids[existingIndex];
            String updatedPassword = savedWifiPasswords[existingIndex];
            for (int i = existingIndex; i > 0; --i) {
                savedWifiSsids[i] = savedWifiSsids[i - 1];
                savedWifiPasswords[i] = savedWifiPasswords[i - 1];
            }
            savedWifiSsids[0] = updatedSsid;
            savedWifiPasswords[0] = updatedPassword;
        }
    } else {
        int insertIndex = savedWifiCount;
        if (savedWifiCount < MAX_SAVED_WIFI_NETWORKS) {
            ++savedWifiCount;
        } else {
            insertIndex = MAX_SAVED_WIFI_NETWORKS - 1;
        }

        for (int i = insertIndex; i > 0; --i) {
            savedWifiSsids[i] = savedWifiSsids[i - 1];
            savedWifiPasswords[i] = savedWifiPasswords[i - 1];
        }

        savedWifiSsids[0] = ssid;
        savedWifiPasswords[0] = password;
    }

    persistWifiConfiguration();
}

bool removeSavedWifiByIndex(int index) {
    if (index < 0 || index >= savedWifiCount) {
        return false;
    }

    for (int i = index; i < savedWifiCount - 1; ++i) {
        savedWifiSsids[i] = savedWifiSsids[i + 1];
        savedWifiPasswords[i] = savedWifiPasswords[i + 1];
    }

    if (savedWifiCount > 0) {
        savedWifiSsids[savedWifiCount - 1] = "";
        savedWifiPasswords[savedWifiCount - 1] = "";
        --savedWifiCount;
    }

    persistWifiConfiguration();
    return true;
}

int findNextAvailableSavedWifiIndex(int startIndex) {
    if (startIndex < 0) {
        startIndex = 0;
    }

    if (startIndex >= savedWifiCount) {
        return -1;
    }

    ensureScanCapableWifiMode();

    Serial.println("Scanning for saved Wi-Fi networks...");
    const int networkCount = WiFi.scanNetworks(false, true);
    if (networkCount <= 0) {
        WiFi.scanDelete();
        return -1;
    }

    int matchedIndex = -1;
    for (int savedIndex = startIndex; savedIndex < savedWifiCount; ++savedIndex) {
        for (int scanIndex = 0; scanIndex < networkCount; ++scanIndex) {
            const String scannedSsid = WiFi.SSID(scanIndex);
            if (scannedSsid.length() == 0) {
                continue;
            }

            if (savedWifiSsids[savedIndex] == scannedSsid) {
                matchedIndex = savedIndex;
                break;
            }
        }

        if (matchedIndex >= 0) {
            break;
        }
    }

    WiFi.scanDelete();
    return matchedIndex;
}

bool startSavedWifiConnectionByIndex(int index) {
    const int availableIndex = findNextAvailableSavedWifiIndex(index);
    if (availableIndex < 0 || availableIndex >= savedWifiCount) {
        return false;
    }

    connectingWifiIndex = availableIndex;
    pendingWifiSsid = "";
    wifiSelectionIndexPending = false;
    wifiSelectionPasswordPending = false;
    wifiForgetIndexPending = false;

    const String& ssid = savedWifiSsids[availableIndex];
    const String& password = savedWifiPasswords[availableIndex];

    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid.c_str(), password.c_str());

    apModeActive = false;
    stationConnectStartMs = millis();
    stationConnectAnnounced = true;
    stationConnectedAnnounced = false;

    Serial.print("Connecting to Wi-Fi network: ");
    Serial.println(ssid);
    if (availableIndex > index) {
        Serial.println("Earlier saved networks were not visible, so they were skipped.");
    }
    if (savedWifiCount > 1 && availableIndex + 1 < savedWifiCount) {
        Serial.println("If it does not connect, the next saved network will be tried automatically.");
    } else {
        Serial.println("Waiting up to 30 seconds before falling back to AP mode...");
    }
    return true;
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
    connectingWifiIndex = -1;
    stationRetryCycles = 0;

    Serial.print("Wi-Fi AP started. SSID: ");
    Serial.println(WEB_AP_SSID);
    Serial.print("Wi-Fi password: ");
    Serial.println(WEB_AP_PASSWORD);
    Serial.print("Open http://");
    Serial.println(WiFi.softAPIP());
    return true;
}

void startStationMode(const String& ssid, const String& password, bool saveConfig) {
    stationRetryCycles = 0;

    if (saveConfig) {
        saveWifiConfiguration(ssid, password);
    }

    loadWifiConfiguration();

    int savedIndex = -1;
    for (int i = 0; i < savedWifiCount; ++i) {
        if (savedWifiSsids[i] == ssid) {
            savedIndex = i;
            break;
        }
    }

    if (savedIndex >= 0 && startSavedWifiConnectionByIndex(savedIndex)) {
        return;
    }

    connectingWifiIndex = -1;
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
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

void handleWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        Serial.print("Wi-Fi STA disconnected. reason=");
        Serial.print((int)info.wifi_sta_disconnected.reason);
        Serial.print(" ssid=");
        Serial.println(WiFi.SSID());
        return;
    }

    if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        Serial.print("Wi-Fi STA got IP: ");
        Serial.println(WiFi.localIP());
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

void appendJsonString(String& json, const char* key, const String& value, bool withComma = true) {
    json += "\"";
    json += key;
    json += "\":\"";
    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value.charAt(i);
        if (c == '\\' || c == '"') {
            json += "\\";
        }
        json += c;
    }
    json += "\"";
    if (withComma) {
        json += ",";
    }
}

void appendJsonRaw(String& json, const char* key, const String& rawValue, bool withComma = true) {
    json += "\"";
    json += key;
    json += "\":";
    json += rawValue;
    if (withComma) {
        json += ",";
    }
}

const char* safetyLevelToString(SafetyLevel level) {
    switch (level) {
        case SafetyLevel::Normal:
            return "normal";
        case SafetyLevel::Hi:
            return "hi";
        case SafetyLevel::HiHi:
            return "hihi";
        case SafetyLevel::Unknown:
        default:
            return "unknown";
    }
}

void appendJsonTestResults(String& json, bool withComma = true) {
    json += "\"test_results\":[";

    for (int i = 0; i < testResultCount; ++i) {
        const TestResultRow& row = testResults[i];
        json += "{";
        appendJsonUnsigned(json, "throttle_percent", (unsigned long)row.throttlePercent);
        appendJsonFloat(json, "voltage_v", row.voltageV, 3);
        appendJsonFloat(json, "current_a", row.currentA, 3);
        appendJsonFloat(json, "power_w", row.powerW, 3);
        appendJsonFloat(json, "rpm", row.rpm, 1);
        appendJsonFloat(json, "esc_temperature_c", row.escTemperatureC, 2);
        appendJsonFloat(json, "motor_temperature_c", row.motorTemperatureC, 2);
        appendJsonFloat(json, "thrust_grams", row.weightGrams, 3);
        appendJsonUnsigned(json, "sample_count", row.sampleCount);
        appendJsonUnsigned(json, "scale_sample_count", row.scaleSampleCount, false);
        json += "}";

        if (i + 1 < testResultCount) {
            json += ",";
        }
    }

    json += "]";
    if (withComma) {
        json += ",";
    }
}

String buildStatusJson(bool includeTestResults = true) {
    loadSafetyConfiguration();

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
    const float rpm = telemetryValid ? estimateMechanicalRpm(lastTlm.rpmField, motorPoleCount) : NAN;
    const float escTemperatureC = telemetryValid ? (float)lastTlm.temperatureC : NAN;
    const float thrustGrams = lastScaleSampleValid ? applyMotorTestDirectionToThrust(lastScaleWeight) : NAN;
    const float thrustStdDevGrams = lastScaleSampleValid ? lastScaleStdDev : NAN;
    const unsigned long nowMs = millis();

    String json;
    json.reserve(includeTestResults ? 3072 : 1536);
    json += "{";
    appendJsonUnsigned(json, "timestamp_ms", nowMs);
    appendJsonBool(json, "telemetry_valid", telemetryValid);
    appendJsonFloat(json, "voltage_v", voltageV, 3);
    appendJsonFloat(json, "current_a", currentA, 3);
    appendJsonFloat(json, "power_w", powerW, 3);
    appendJsonFloat(json, "throttle_percent", throttlePercent, 1);
    appendJsonFloat(json, "rpm", rpm, 1);
    appendJsonFloat(json, "esc_temperature_c", escTemperatureC, 2);
    appendJsonBool(json, "ir_detected", irDetected);
    appendJsonFloat(json, "ir_ambient_c", irAmbient, 2);
    appendJsonFloat(json, "ir_object_c", irObject, 2);
    appendJsonBool(json, "scale_detected", scaleDetected);
    appendJsonBool(json, "scale_valid", lastScaleSampleValid);
    appendJsonFloat(json, "thrust_grams", thrustGrams, 3);
    appendJsonFloat(json, "thrust_stddev_grams", thrustStdDevGrams, 3);
    appendJsonFloat(json, "weight_grams", thrustGrams, 3);
    appendJsonBool(json, "wifi_sta_connected", WiFi.status() == WL_CONNECTED);
    appendJsonBool(json, "wifi_ap_active", apModeActive);
    appendJsonUnsigned(json, "telemetry_last_update_ms", lastTlm.lastUpdateMs);
    appendJsonUnsigned(json, "scale_last_read_ms", lastScaleReadMs);
    appendJsonUnsigned(json, "ir_last_read_ms", lastIrReadMs);
    appendJsonBool(json, "test_running", testRunning);
    appendJsonBool(json, "motor_test_cooling_enabled", isMotorTestCooldownEnabled());
    appendJsonBool(json, "motor_test_pusher_mode", isMotorTestPusherMode());
    appendJsonUnsigned(json, "motor_poles", (unsigned long)motorPoleCount);
    appendJsonUnsigned(json, "motor_kv", (unsigned long)motorKv);
    appendJsonFloat(json, "scale_calibration_factor", getCurrentScaleCalibrationFactor(), 6);
    appendJsonFloat(json, "safety_current_hi_a", safetyConfig.currentA.hi, 3);
    appendJsonFloat(json, "safety_current_hihi_a", safetyConfig.currentA.hihi, 3);
    appendJsonFloat(json, "safety_motor_temp_hi_c", safetyConfig.motorTemperatureC.hi, 2);
    appendJsonFloat(json, "safety_motor_temp_hihi_c", safetyConfig.motorTemperatureC.hihi, 2);
    appendJsonFloat(json, "safety_esc_temp_hi_c", safetyConfig.escTemperatureC.hi, 2);
    appendJsonFloat(json, "safety_esc_temp_hihi_c", safetyConfig.escTemperatureC.hihi, 2);
    appendJsonString(json, "safety_current_state", String(safetyLevelToString(safetyStatus.currentLevel)));
    appendJsonString(json, "safety_motor_temp_state", String(safetyLevelToString(safetyStatus.motorTemperatureLevel)));
    appendJsonString(json, "safety_esc_temp_state", String(safetyLevelToString(safetyStatus.escTemperatureLevel)));
    appendJsonBool(json, "safety_trip_active", safetyStatus.tripActive);
    appendJsonString(json, "safety_trip_reason", safetyStatus.tripReason);
    appendJsonUnsigned(json, "alarm_unread_count", (unsigned long)getUnreadAlarmCount());
    appendJsonUnsigned(json, "alarm_total_count", (unsigned long)getTotalAlarmCount());
    appendJsonRaw(json, "recent_alarms", buildRecentAlarmsJson(10));
    appendJsonUnsigned(json, "test_result_count", (unsigned long)testResultCount, includeTestResults);
    if (includeTestResults) {
        appendJsonTestResults(json, false);
    }
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
        Serial.print("WebSocket client connected:");
        printSocketClientContext(client);
        Serial.println();
        client->setCloseClientOnQueueFull(false);
        if (!client->queueIsFull()) {
            client->text(buildStatusJson(false));
        }
        return;
    }

    if (type == WS_EVT_DISCONNECT) {
        Serial.print("WebSocket client disconnected:");
        printSocketClientContext(client);
        Serial.println();
    }
}

}

bool queueWebCommand(const String& cmd) {
    String trimmed = cmd;
    trimmed.trim();
    if (trimmed.length() == 0) {
        return false;
    }

    bool queued = false;
    portENTER_CRITICAL(&queuedWebCommandMux);
    if (!queuedWebCommandPending) {
        queuedWebCommand = trimmed;
        queuedWebCommandPending = true;
        queued = true;
    }
    portEXIT_CRITICAL(&queuedWebCommandMux);
    return queued;
}

void processQueuedWebCommand() {
    String cmd;

    portENTER_CRITICAL(&queuedWebCommandMux);
    if (queuedWebCommandPending) {
        cmd = queuedWebCommand;
        queuedWebCommand = "";
        queuedWebCommandPending = false;
    }
    portEXIT_CRITICAL(&queuedWebCommandMux);

    if (cmd.length() == 0) {
        return;
    }

    addCommandToHistory(cmd);
    handleCommand(cmd);
}

bool beginWebServer() {
    if (webServerStarted) {
        return true;
    }

    webUiAssetsReady = validateWebUiAssets();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!webUiAssetsReady) {
            request->send(503,
                          "text/plain",
                          "Web UI files are missing from LittleFS. Upload the filesystem image and reboot.");
            return;
        }
        AsyncWebServerResponse* response = request->beginResponse(LittleFS, "/index.html", "text/html");
        response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("Expires", "0");
        request->send(response);
    });

    server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!webUiAssetsReady) {
            request->send(404, "text/plain", "styles.css is missing from LittleFS");
            return;
        }
        AsyncWebServerResponse* response = request->beginResponse(LittleFS, "/styles.css", "text/css");
        response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("Expires", "0");
        request->send(response);
    });

    server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!webUiAssetsReady) {
            request->send(404, "text/plain", "app.js is missing from LittleFS");
            return;
        }
        AsyncWebServerResponse* response = request->beginResponse(LittleFS, "/app.js", "application/javascript");
        response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("Expires", "0");
        request->send(response);
    });

    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/favicon.ico")) {
            request->send(LittleFS, "/favicon.ico", "image/x-icon");
            return;
        }

        if (LittleFS.exists("/favicon.ico.gz")) {
            AsyncWebServerResponse* response = request->beginResponse(LittleFS, "/favicon.ico.gz", "image/x-icon");
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
            return;
        }

        request->send(204);
    });

    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "application/json", buildStatusJson());
    });

    server.on("/api/test.csv", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!hasTestResults()) {
            request->send(404, "text/plain", "No test results available");
            return;
        }

        AsyncWebServerResponse* response = request->beginResponse(200, "text/csv", getLastTestCsv());
        response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("Expires", "0");
        response->addHeader("Content-Disposition", "attachment; filename=\"test_results.csv\"");
        request->send(response);
    });

    server.on("/api/command", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!request->hasParam("cmd", true)) {
            request->send(400, "application/json", "{\"ok\":false,\"error\":\"Missing cmd\"}");
            return;
        }

        const String cmd = request->getParam("cmd", true)->value();
        if (cmd.length() == 0) {
            request->send(400, "application/json", "{\"ok\":false,\"error\":\"Empty cmd\"}");
            return;
        }

        if (!queueWebCommand(cmd)) {
            request->send(409, "application/json", "{\"ok\":false,\"error\":\"Command queue busy\"}");
            return;
        }

        request->send(202, "application/json", "{\"ok\":true,\"queued\":true}");
    });

    server.on("/api/alarms/read", HTTP_POST, [](AsyncWebServerRequest* request) {
        markAllAlarmsRead();
        request->send(200, "application/json", "{\"ok\":true}");
    });

    telemetrySocket.onEvent(handleWebSocketEvent);
    server.addHandler(&telemetrySocket);

    if (webUiAssetsReady) {
        server.serveStatic("/", LittleFS, "/")
            .setCacheControl("no-store, no-cache, must-revalidate")
            .setDefaultFile("index.html");
    }

    server.onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "text/plain", "Not found");
    });

    loadWifiConfiguration();
    if (savedWifiCount > 0) {
        if (!startSavedWifiConnectionByIndex(0) && !startAccessPointMode()) {
            return false;
        }
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
            stationConnectStartMs = 0;
            stationRetryCycles = 0;
            if (!stationConnectedAnnounced) {
                printStationConnectedMessage();
                stationConnectedAnnounced = true;
            }
        } else {
            if (stationConnectedAnnounced && stationConnectStartMs == 0) {
                // A previously healthy STA link dropped, so start a fresh recovery window.
                stationConnectStartMs = millis();
                stationConnectedAnnounced = false;
                Serial.println("Wi-Fi station disconnected. Waiting for reconnection before AP fallback.");
            }

            if (stationConnectStartMs != 0 && millis() - stationConnectStartMs >= WIFI_CONNECT_TIMEOUT_MS) {
                if (connectingWifiIndex >= 0 && connectingWifiIndex + 1 < savedWifiCount) {
                    Serial.print("Wi-Fi station connect timed out for ");
                    Serial.print(savedWifiSsids[connectingWifiIndex]);
                    Serial.println(". Trying next saved network.");
                    if (!startSavedWifiConnectionByIndex(connectingWifiIndex + 1)) {
                        Serial.println("No additional saved Wi-Fi networks are currently visible. Falling back to AP mode.");
                        startAccessPointMode();
                    }
                } else {
                    ++stationRetryCycles;
                    if (savedWifiCount > 0 && stationRetryCycles < WIFI_AP_FALLBACK_RETRY_CYCLES) {
                        Serial.print("Wi-Fi station connect timed out. Retrying saved networks (cycle ");
                        Serial.print(stationRetryCycles);
                        Serial.print(" of ");
                        Serial.print(WIFI_AP_FALLBACK_RETRY_CYCLES);
                        Serial.println(") before AP fallback.");
                        if (!startSavedWifiConnectionByIndex(0)) {
                            Serial.println("No saved Wi-Fi networks are currently visible. Staying in station retry mode.");
                            stationConnectStartMs = millis();
                            connectingWifiIndex = -1;
                        }
                    } else {
                        Serial.println("Wi-Fi station connect timed out. Falling back to AP mode.");
                        startAccessPointMode();
                    }
                }
            }
        }
    }

    telemetrySocket.cleanupClients();

    const unsigned long nowMs = millis();
    if (nowMs - lastTelemetryBroadcastMs < TELEMETRY_BROADCAST_INTERVAL_MS) {
        return;
    }

    lastTelemetryBroadcastMs = nowMs;
    if (telemetrySocket.count() > 0) {
        broadcastLatestTelemetrySnapshot();
    }
}

void beginWifiSelection() {
    ensureScanCapableWifiMode();

    wifiSelectionIndexPending = false;
    wifiSelectionPasswordPending = false;
    wifiForgetIndexPending = false;
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

void beginWifiForget() {
    wifiSelectionIndexPending = false;
    wifiSelectionPasswordPending = false;
    wifiForgetIndexPending = false;
    pendingWifiSsid = "";

    loadWifiConfiguration();
    if (savedWifiCount == 0) {
        Serial.println("No saved Wi-Fi networks to forget.");
        return;
    }

    Serial.println("Saved Wi-Fi networks:");
    for (int i = 0; i < savedWifiCount; ++i) {
        Serial.print("  ");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.println(savedWifiSsids[i]);
    }

    wifiForgetIndexPending = true;
    Serial.println("Choose saved Wi-Fi network number to forget:");
}

bool handleWifiSelectionInput(const String& input) {
    if (!wifiSelectionIndexPending && !wifiSelectionPasswordPending && !wifiForgetIndexPending) {
        return false;
    }

    String trimmed = input;
    trimmed.trim();

    if (wifiSelectionIndexPending) {
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

    if (wifiForgetIndexPending) {
        int selectedNumber = trimmed.toInt();
        if (selectedNumber < 1 || selectedNumber > savedWifiCount) {
            Serial.println("Invalid Wi-Fi selection. Enter one of the listed numbers:");
            return true;
        }

        const int forgetIndex = selectedNumber - 1;
        const String forgottenSsid = savedWifiSsids[forgetIndex];
        const String connectedSsid = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "";
        const bool wasConnectedNetwork = connectedSsid == forgottenSsid;
        const bool wasPendingNetwork = connectingWifiIndex >= 0
            && connectingWifiIndex < savedWifiCount
            && savedWifiSsids[connectingWifiIndex] == forgottenSsid;

        wifiForgetIndexPending = false;
        removeSavedWifiByIndex(forgetIndex);

        Serial.print("Forgot Wi-Fi network: ");
        Serial.println(forgottenSsid);

        if (wasConnectedNetwork || wasPendingNetwork) {
            WiFi.disconnect(true, true);
            stationConnectStartMs = 0;
            stationConnectAnnounced = false;
            stationConnectedAnnounced = false;
            connectingWifiIndex = -1;

            if (savedWifiCount > 0) {
                Serial.println("Reconnecting using the remaining saved networks...");
                startSavedWifiConnectionByIndex(0);
            } else {
                Serial.println("No saved Wi-Fi networks remain. Starting AP mode.");
                startAccessPointMode();
            }
        }

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
    return wifiSelectionIndexPending || wifiSelectionPasswordPending || wifiForgetIndexPending;
}

void reconnectSavedWifi() {
    wifiSelectionIndexPending = false;
    wifiSelectionPasswordPending = false;
    wifiForgetIndexPending = false;
    pendingWifiSsid = "";
    stationRetryCycles = 0;

    loadWifiConfiguration();
    if (savedWifiCount == 0) {
        Serial.println("No saved Wi-Fi networks. Use 'wifi select' first.");
        return;
    }

    Serial.println("Trying saved Wi-Fi networks...");
    if (startSavedWifiConnectionByIndex(0)) {
        return;
    }

    Serial.println("No saved Wi-Fi networks are currently visible. Staying in AP mode.");
    if (!apModeActive) {
        startAccessPointMode();
    }
}

void forceAccessPointMode() {
    wifiSelectionIndexPending = false;
    wifiSelectionPasswordPending = false;
    wifiForgetIndexPending = false;
    pendingWifiSsid = "";

    if (startAccessPointMode()) {
        Serial.println("Forced Wi-Fi AP mode.");
    } else {
        Serial.println("WARNING: Failed to force Wi-Fi AP mode.");
    }
}

void printWifiStatus() {
    loadWifiConfiguration();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("Wi-Fi STA connected to ");
        Serial.print(WiFi.SSID());
        Serial.print(" at ");
        Serial.println(WiFi.localIP());
    } else if (apModeActive) {
        Serial.print("Wi-Fi AP mode active at ");
        Serial.println(WiFi.softAPIP());
    } else if (stationConnectStartMs != 0) {
        Serial.print("Connecting to Wi-Fi network: ");
        if (connectingWifiIndex >= 0 && connectingWifiIndex < savedWifiCount) {
            Serial.println(savedWifiSsids[connectingWifiIndex]);
        } else {
            Serial.println("unknown");
        }
    } else {
        Serial.println("Wi-Fi not configured. AP fallback will be used.");
    }

    if (savedWifiCount == 0) {
        Serial.println("Saved Wi-Fi networks: none");
        return;
    }

    Serial.println("Saved Wi-Fi networks:");
    for (int i = 0; i < savedWifiCount; ++i) {
        Serial.print("  ");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(savedWifiSsids[i]);
        if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == savedWifiSsids[i]) {
            Serial.print(" (connected)");
        } else if (stationConnectStartMs != 0 && connectingWifiIndex == i) {
            Serial.print(" (trying now)");
        }
        Serial.println();
    }
}
