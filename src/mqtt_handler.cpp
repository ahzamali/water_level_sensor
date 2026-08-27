#include "mqtt_handler.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

static WiFiClient esp_net_client;
static PubSubClient mqtt_client(esp_net_client);
static char current_broker[64] = "192.168.211.175";  // char[]: stable c_str() pointer, no heap
static uint16_t current_port = 1883;
static unsigned long last_reconnect_attempt = 0;

static bool stay_awake = false;
static unsigned long stay_awake_until = 0;
static bool mqtt_debug_enabled = true; // Default enabled for debugging

void setStayAwake(bool enable, uint32_t duration_sec) {
  stay_awake = enable;
  if (enable) {
    stay_awake_until = millis() + (duration_sec * 1000UL);
  } else {
    stay_awake_until = 0;
  }
}

bool isStayAwakeRequested() {
  if (!stay_awake) return false;
  if (millis() >= stay_awake_until) {
    stay_awake = false;
    stay_awake_until = 0;
    return false;
  }
  return true;
}

unsigned long getStayAwakeRemainingSec() {
  if (!isStayAwakeRequested()) return 0;
  unsigned long now = millis();
  if (stay_awake_until > now) {
    return (stay_awake_until - now) / 1000UL;
  }
  return 0;
}

bool isMqttDebugEnabled() {
  return mqtt_debug_enabled;
}

void setMqttDebugEnabled(bool enable) {
  mqtt_debug_enabled = enable;
  Serial.printf("MQTT Debug Logging: %s\n", enable ? "ENABLED" : "DISABLED");
}

void publishMqttDebug(const char* message) {
  if (!mqtt_debug_enabled || message == nullptr) return;

  if (!mqtt_client.connected()) {
    return;
  }
  mqtt_client.publish("roof/sensor/log", message);
}

static void trimString(char* str) {
  if (str == nullptr) return;
  // Trim leading whitespace
  char* start = str;
  while (*start && (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n' || *start == '\"')) {
    start++;
  }
  // Trim trailing whitespace
  char* end = start + strlen(start) - 1;
  while (end >= start && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n' || *end == '\"')) {
    *end = '\0';
    end--;
  }
  if (start != str) {
    memmove(str, start, strlen(start) + 1);
  }
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char message[64];
  unsigned int len = length < sizeof(message) - 1 ? length : sizeof(message) - 1;
  memcpy(message, payload, len);
  message[len] = '\0';

  trimString(message);
  Serial.printf("MQTT Message arrived [%s]: '%s'\n", topic, message);

  if (strcasecmp(message, "ota") == 0 || strcasecmp(message, "stay_awake") == 0 || 
      strcasecmp(message, "update") == 0 || strcasecmp(message, "awake") == 0 ||
      strcasecmp(message, "on") == 0 || strcasecmp(message, "1") == 0) {
    setStayAwake(true, 600); // 10 minutes stay awake
    Serial.println("MQTT: OTA / Stay-awake mode confirmed for Web UI updates");
    if (mqtt_client.connected()) {
      mqtt_client.publish("roof/log", "OTA Mode Active: Web UI ready at device IP for updates");
      mqtt_client.publish("roof/tank_water/control", "", true); // Clear retained message on broker
    }
  } else if (strcasecmp(message, "reboot") == 0 || strcasecmp(message, "restart") == 0) {
    Serial.println("MQTT: Reboot command received!");
    if (mqtt_client.connected()) {
      mqtt_client.publish("roof/log", "Rebooting device via MQTT command...");
      mqtt_client.publish("roof/tank_water/control", "", true);
      delay(500);
    }
    ESP.restart();
  } else if (strcasecmp(message, "debug_on") == 0 || strcasecmp(message, "debug_enable") == 0) {
    setMqttDebugEnabled(true);
    publishMqttDebug("MQTT Debug Logging ENABLED via MQTT command");
  } else if (strcasecmp(message, "debug_off") == 0 || strcasecmp(message, "debug_disable") == 0) {
    publishMqttDebug("MQTT Debug Logging DISABLING via MQTT command");
    setMqttDebugEnabled(false);
  }
}

void initMqtt(const char* broker_host, uint16_t port) {
  if (broker_host != nullptr && strlen(broker_host) > 0) {
    strncpy(current_broker, broker_host, sizeof(current_broker) - 1);
    current_broker[sizeof(current_broker) - 1] = '\0';  // Ensure null-termination
  }
  current_port = port;
  mqtt_client.setBufferSize(512); // Allow longer debug payloads (default is 256 bytes)
  mqtt_client.setServer(current_broker, current_port);
  mqtt_client.setCallback(mqttCallback);
}

bool isMqttConnected() {
  return mqtt_client.connected();
}

static bool reconnectMqtt() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  
  // Use char[] to avoid heap String allocation on every reconnect attempt
  char client_id[32];
  snprintf(client_id, sizeof(client_id), "roof_water_sensor_%x", ESP.getChipId());
  if (mqtt_client.connect(client_id)) {
    Serial.printf("MQTT connected to broker: %s\n", current_broker);
    mqtt_client.subscribe("roof/tank_water/control");
    mqtt_client.subscribe("roof/sensor/control");
    mqtt_client.publish("roof/log", "ESP8266 Water Sensor Connected (24/7 Always-On)");
    mqtt_client.publish("roof/sensor/version", FIRMWARE_VERSION, true); // Retained: visible without device online
    delay(50); // Allow broker time to push retained message into TCP buffer
    mqtt_client.loop(); // Drain TCP buffer: fires mqttCallback for retained messages immediately
    return true;
  }
  return false;
}

void handleMqtt() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (!mqtt_client.connected()) {
    unsigned long now = millis();
    if (now - last_reconnect_attempt > 3000 || last_reconnect_attempt == 0) {
      last_reconnect_attempt = now;
      reconnectMqtt();
    }
  } else {
    mqtt_client.loop();
  }
}

void publishMqttTelemetry(long distance_cm, long water_level_cm, float percent, float temp_c, float hum_pct) {
  if (!mqtt_client.connected()) {
    reconnectMqtt();
  }

  if (mqtt_client.connected()) {
    char buf[64];

    snprintf(buf, sizeof(buf), "%.1f", percent);
    mqtt_client.publish("roof/tank_water/level", buf, true);

    snprintf(buf, sizeof(buf), "%ld", distance_cm);
    mqtt_client.publish("roof/tank_water/distance", buf, true);

    if (!isnan(temp_c)) {
      snprintf(buf, sizeof(buf), "%.2f", temp_c);
      mqtt_client.publish("roof/sensor/temperature", buf, true);
    }

    if (!isnan(hum_pct)) {
      snprintf(buf, sizeof(buf), "%.2f", hum_pct);
      mqtt_client.publish("roof/sensor/humidity", buf, true);
    }

    mqtt_client.loop(); // Drain TCP receive buffer after batch publish to keep connection alive
    Serial.println("MQTT Published: Water Level = " + String(percent) + "%, Dist = " + String(distance_cm) + " cm");
  }
}
