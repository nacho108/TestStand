#include "web_server.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#include "app_config.h"

namespace {

AsyncWebServer server(80);
bool webServerStarted = false;

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
