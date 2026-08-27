#include "web_server_handler.h"
#include "web_pages.h"
#include "sensor_filter.h"
#include "env_sensor.h"
#include "mqtt_handler.h"
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <Updater.h>

extern char speed_of_sound[6];
extern char tank_bottom_distance[6];
extern char max_water_level[6];
extern long distance_to_water;
extern const int ledcount;
extern void saveConfig();

static ESP8266WebServer* server_ref = nullptr;
static bool ota_in_progress = false;

void initWebServer(std::unique_ptr<ESP8266WebServer>& server_ptr) {
  server_ptr.reset(new ESP8266WebServer(WiFi.localIP(), 80));
  server_ref = server_ptr.get();

  // Root Dashboard Route
  server_ref->on("/", HTTP_GET, []() {
    server_ref->sendHeader("Connection", "close");
    server_ref->send(200, "text/html", INDEX_HTML);
  });

  // Telemetry JSON API Endpoint
  server_ref->on("/api/readings", HTTP_GET, []() {
    long water_level = String(tank_bottom_distance).toInt() - distance_to_water;
    float max_level = String(max_water_level).toFloat();
    float pct = (max_level > 0.0f) ? ((float)water_level / max_level) * 100.0f : 0.0f;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;

    EnvReadings env = readEnvSensors();

    StaticJsonBuffer<640> jsonBuffer;  // Stack-allocated: freed on lambda return, no heap fragmentation
    JsonObject& root = jsonBuffer.createObject();
    root["distance_to_water"] = distance_to_water;
    root["water_level"] = water_level;
    root["normalized_level"] = calculateNormalizedLevel(water_level, max_level, ledcount);
    root["percentage"] = pct;
    root["speed_of_sound"] = String(speed_of_sound).toInt();
    root["tank_bottom_distance"] = String(tank_bottom_distance).toInt();
    root["max_water_level"] = String(max_water_level).toInt();

    if (!isnan(env.temperature)) {
      root["temperature"] = env.temperature;
    } else {
      root["temperature"] = RawJson("null");
    }

    if (!isnan(env.humidity)) {
      root["humidity"] = env.humidity;
    } else {
      root["humidity"] = RawJson("null");
    }

    if (!isnan(env.pressure)) {
      root["pressure"] = env.pressure;
    } else {
      root["pressure"] = RawJson("null");
    }

    root["mqtt_connected"] = isMqttConnected();
    root["mqtt_debug_enabled"] = isMqttDebugEnabled();
    root["stay_awake_remaining_sec"] = getStayAwakeRemainingSec();
    root["uptime_sec"] = millis() / 1000;
    root["wifi_rssi"] = WiFi.RSSI();
    root["wifi_ssid"] = WiFi.SSID();
    root["ip_address"] = WiFi.localIP().toString();
    root["firmware_version"] = FIRMWARE_VERSION;

    char response[640];  // Stack-allocated: avoids heap String churn on every poll
    root.printTo(response, sizeof(response));
    server_ref->sendHeader("Connection", "close");
    server_ref->send(200, "application/json", response);
  });

  // Toggle MQTT Debug Logging Endpoint
  server_ref->on("/api/debug_toggle", HTTP_POST, []() {
    if (server_ref->hasArg("enable")) {
      bool enable = (server_ref->arg("enable") == "1" || server_ref->arg("enable") == "true");
      setMqttDebugEnabled(enable);
      server_ref->send(200, "text/plain", enable ? "DEBUG_ENABLED" : "DEBUG_DISABLED");
    } else {
      bool newState = !isMqttDebugEnabled();
      setMqttDebugEnabled(newState);
      server_ref->send(200, "text/plain", newState ? "DEBUG_ENABLED" : "DEBUG_DISABLED");
    }
  });

  // Save Configuration Endpoint
  server_ref->on("/api/config", HTTP_POST, []() {
    if (server_ref->hasArg("s") && server_ref->arg("s").length() > 0) {
      strncpy(speed_of_sound, server_ref->arg("s").c_str(), sizeof(speed_of_sound) - 1);
      speed_of_sound[sizeof(speed_of_sound) - 1] = '\0';
    }
    if (server_ref->hasArg("t") && server_ref->arg("t").length() > 0) {
      strncpy(tank_bottom_distance, server_ref->arg("t").c_str(), sizeof(tank_bottom_distance) - 1);
      tank_bottom_distance[sizeof(tank_bottom_distance) - 1] = '\0';
    }
    if (server_ref->hasArg("m") && server_ref->arg("m").length() > 0) {
      strncpy(max_water_level, server_ref->arg("m").c_str(), sizeof(max_water_level) - 1);
      max_water_level[sizeof(max_water_level) - 1] = '\0';
    }
    saveConfig();
    server_ref->send(200, "text/plain", "OK");
  });

  // OTA Firmware Upload Route
  server_ref->on("/update", HTTP_POST, []() {
    server_ref->sendHeader("Connection", "close");
    if (Update.hasError()) {
      server_ref->send(500, "text/plain", "OTA Update Failed");
    } else {
      server_ref->send(200, "text/plain", "OK");
      delay(1000);
      ESP.restart();
    }
  }, []() {
    HTTPUpload& upload = server_ref->upload();
    if (upload.status == UPLOAD_FILE_START) {
      ota_in_progress = true;
      WiFi.setSleepMode(WIFI_NONE_SLEEP);
      Serial.printf("OTA Update Started: %s\n", upload.filename.c_str());
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & ~0xFFF;
      if (!Update.begin(maxSketchSpace, U_FLASH)) {
        Update.printError(Serial);
        ota_in_progress = false;
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      yield();
      ESP.wdtFeed();
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      ota_in_progress = false;
      if (Update.end(true)) {
        Serial.printf("OTA Update Success: %u bytes. Rebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      Update.end();
      ota_in_progress = false;
      Serial.println("OTA Update Aborted");
    }
  });

  // Reset WiFi Endpoint
  server_ref->on("/api/wifi_reset", HTTP_POST, []() {
    server_ref->send(200, "text/plain", "Resetting Wi-Fi...");
    delay(1000);
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    ESP.restart();
  });

  // Legacy route compatibility
  server_ref->on("/showconfig", HTTP_GET, []() {
    server_ref->send(200, "text/html", "speed_of_sound=" + String(speed_of_sound) + ",tank_bottom_distance=" + String(tank_bottom_distance) 
      + ",max_water_level = " + String(max_water_level) + ",distance_to_water = " + String(distance_to_water));
  });

  server_ref->on("/getconfig", HTTP_GET, []() {
    server_ref->send(200, "text/html", "speed_of_sound=" + String(speed_of_sound) + ",tank_bottom_distance=" + String(tank_bottom_distance) 
      + ",max_water_level=" + String(max_water_level) + ",distance_to_water=" + String(distance_to_water));
  });

  server_ref->on("/d", HTTP_GET, []() {
    server_ref->send(200, "text/html", "distance_to_water=" + String(distance_to_water));
  });

  server_ref->onNotFound([]() {
    server_ref->send(404, "text/plain", "File Not Found");
  });

  server_ref->begin();
}

void handleWebRoutes() {
  if (server_ref) {
    server_ref->handleClient();
  }
}

bool isOtaInProgress() {
  return ota_in_progress;
}
