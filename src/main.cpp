#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include "WiFiManager.h"
#include <FS.h>  
#include <ArduinoJson.h>
#include <time.h>
#include "sensor_filter.h"
#include "web_server_handler.h"
#include "env_sensor.h"
#include "mqtt_handler.h"

// Forward function declarations
void handleRoot();
void handleNotFound();
void saveConfigCallback();
int readSpiffsConfig();
bool wifiManagerSetup(WiFiManager& wifiManager, bool isDeepSleepWakeup);
void saveConfig();
void createWebServer();
long readSingleDistanceCm();
long getFilteredDistanceCm();

std::unique_ptr<ESP8266WebServer> server;

// Hardware pin definitions matching WaterLevelTempSensorV2
const int trigger = D7;  // Trigger Pin (D7 = GPIO13)
const int echo = D6;     // Echo Pin (D6 = GPIO12)

char tank_bottom_distance[6] = "98";   // max_tank_depth from WaterLevelTempSensorV2.ino (cm)
char max_water_level[6] = "75";       // usable_lenth from WaterLevelTempSensorV2.ino (cm)
char speed_of_sound[6] = "342";       // default, m/s
bool shouldSaveConfig = false;
long previous_level = 0;
long distance_to_water = 0;
long duration = 0;
int fakeCount = 0;
extern const int ledcount = 9;


void handleRoot() {
  if (server) {
    server->send(200, "text/html", "Water Level Sensor OK");
  }
}

void handleNotFound() {
  if (server) {
    server->send(404, "text/plain", "Not Found");
  }
}

void saveConfigCallback() {
  Serial.println("saveConfigCallback: Should save config");
  shouldSaveConfig = true;
}

int readSpiffsConfig() {
  Serial.println("readSpiffsConfig: mounting FS...");
  if (SPIFFS.begin()) {
    Serial.println("readSpiffsConfig: mounted file system");
    if (SPIFFS.exists("/config.json")) {
      Serial.println("readSpiffsConfig: reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        size_t size = configFile.size();
        if (size > 0 && size < 1024) {
          std::unique_ptr<char[]> buf(new char[size + 1]);
          configFile.readBytes(buf.get(), size);
          buf[size] = '\0';
          StaticJsonBuffer<256> jsonBuffer;  // Stack-allocated: no heap fragmentation
          JsonObject& json = jsonBuffer.parseObject(buf.get());
          
          if (json.success()) {
            if(json["speed_of_sound"]) strncpy(speed_of_sound, json["speed_of_sound"], sizeof(speed_of_sound) - 1);
            if(json["tank_bottom_distance"]) strncpy(tank_bottom_distance, json["tank_bottom_distance"], sizeof(tank_bottom_distance) - 1);
            if(json["max_water_level"]) strncpy(max_water_level, json["max_water_level"], sizeof(max_water_level) - 1);
            speed_of_sound[sizeof(speed_of_sound) - 1] = '\0';
            tank_bottom_distance[sizeof(tank_bottom_distance) - 1] = '\0';
            max_water_level[sizeof(max_water_level) - 1] = '\0';
            Serial.printf("readSpiffsConfig: Loaded config -> speed=%s, bottom=%s, max=%s\n", speed_of_sound, tank_bottom_distance, max_water_level);
          }
        }
        configFile.close();
      }
    } else {
      Serial.println("readSpiffsConfig: /config.json not found, using default configuration.");
    }
  } else {
    Serial.println("readSpiffsConfig: failed to mount FS");
  }
  return 0;
}

bool wifiManagerSetup(WiFiManager& wifiManager, bool isDeepSleepWakeup) {
  WiFiManagerParameter custom_speed_of_sound("speedofsound", "speed of sound", speed_of_sound, 6);
  WiFiManagerParameter custom_tank_bottom_distance("tankbottomdistance", "tank bottom distance", tank_bottom_distance, 6);
  WiFiManagerParameter custom_max_water_level("maxwaterlevel", "max water level", max_water_level, 6);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&custom_speed_of_sound);
  wifiManager.addParameter(&custom_tank_bottom_distance);
  wifiManager.addParameter(&custom_max_water_level);

  // Set connection attempt timeout to 15 seconds to prevent hanging on router delays
  wifiManager.setConnectTimeout(15);
  wifiManager.setConfigPortalTimeout(180);
  Serial.println("wifiManagerSetup: AutoConnecting with 180s AP fallback portal...");

  bool res = wifiManager.autoConnect("WATER_LEVEL_SENSOR");

  if (res) {
    if (custom_speed_of_sound.getValue() && strlen(custom_speed_of_sound.getValue()) > 0) {
      strncpy(speed_of_sound, custom_speed_of_sound.getValue(), sizeof(speed_of_sound) - 1);
    }
    if (custom_tank_bottom_distance.getValue() && strlen(custom_tank_bottom_distance.getValue()) > 0) {
      strncpy(tank_bottom_distance, custom_tank_bottom_distance.getValue(), sizeof(tank_bottom_distance) - 1);
    }
    if (custom_max_water_level.getValue() && strlen(custom_max_water_level.getValue()) > 0) {
      strncpy(max_water_level, custom_max_water_level.getValue(), sizeof(max_water_level) - 1);
    }
    if (shouldSaveConfig) {
      saveConfig();
      shouldSaveConfig = false;
    }
  } else {
    Serial.println("wifiManagerSetup: Failed to connect to WiFi or Config Portal timed out.");
  }
  return res;
}

void saveConfig() {
  Serial.println("saveConfig: Saving Config to SPIFFS");
  StaticJsonBuffer<256> jsonBuffer;  // Stack-allocated: no heap fragmentation
  JsonObject& json = jsonBuffer.createObject();
  json["speed_of_sound"] = speed_of_sound;
  json["tank_bottom_distance"] = tank_bottom_distance;
  json["max_water_level"] = max_water_level;

  File configFile = SPIFFS.open("/config.json", "w");
  if (configFile) {
    json.printTo(configFile);
    configFile.close();
    Serial.println("saveConfig: Config successfully saved to SPIFFS.");
  } else {
    Serial.println("saveConfig: Failed to open /config.json for writing!");
  }
}

void createWebServer() {
  initWebServer(server);
}

#ifndef UNIT_TEST
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.printf("Firmware Version: %s (24/7 Always-On)\n", FIRMWARE_VERSION);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // 1. Initialize SPIFFS and load persisted configuration safely inside setup()
  readSpiffsConfig();

  // 2. Enable WiFi auto-reconnect for resilient 24/7 operation
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  // 3. Connect to WiFi
  WiFiManager wifiManager;
  bool wifiConnected = wifiManagerSetup(wifiManager, false);

  if (!wifiConnected || WiFi.status() != WL_CONNECTED) {
    Serial.println("setup: WiFi connection failed. Will keep retrying in background...");
  } else {
    Serial.print("setup: Connected to Router: IP = ");
    Serial.println(WiFi.localIP());
  }

  // Sync NTP time (IST UTC+5:30 = 19800 seconds)
  configTime(19800, 0, "pool.ntp.org", "time.nist.gov");

  createWebServer();  
  Serial.println("setup: HTTP server started (24/7 Continuous Dashboard).");

  initEnvSensors(D5);
  initMqtt("192.168.211.175", 1883);

  pinMode(trigger, OUTPUT);
  pinMode(echo, INPUT);
  digitalWrite(trigger, LOW);
  delay(50);

  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
}
#endif

// Performs a single ping measurement and converts to cm. Returns -1 if invalid/timeout.
long readSingleDistanceCm() {
  digitalWrite(trigger, LOW);
  delayMicroseconds(2);
  digitalWrite(trigger, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigger, LOW);

  long d = pulseIn(echo, HIGH, 60000); // 60ms timeout (~10m max)
  if (d <= 0) return -1;

  float dist = (d / 2.0) * (String(speed_of_sound).toFloat() / 10000.0);
  if (dist <= 0.0f) return -1;

  return (long)dist;
}

// Takes 9 samples, sorts them, and returns the median sample (outlier rejection)
long getFilteredDistanceCm() {
  const int NUM_SAMPLES = 9;
  long samples[NUM_SAMPLES];
  int validCount = 0;

  for (int i = 0; i < NUM_SAMPLES; i++) {
    long reading = readSingleDistanceCm();
    if (reading > 0) {
      samples[validCount++] = reading;
    }
    handleWebRoutes();
    delay(30); // 30ms delay allows ultrasound echo reflection to decay
  }

  if (validCount == 0) {
    return distance_to_water;
  }

  return calculateMedian(samples, validCount, distance_to_water);
}

#ifndef UNIT_TEST
void loop() {
  // 1. Continuously process Web Server requests and MQTT commands
  handleWebRoutes();
  handleMqtt();

  // 2. If OTA firmware upload is active, dedicate CPU and bandwidth entirely to flashing
  if (isOtaInProgress()) {
    delay(1);
    yield();
    return;
  }

  // 3. Periodic measurement and telemetry publish timer (Every 60 seconds)
  static unsigned long last_sample_time = 0;
  const unsigned long SAMPLE_INTERVAL_MS = 60000;
  unsigned long now = millis();

  if (now - last_sample_time >= SAMPLE_INTERVAL_MS || last_sample_time == 0) {
    last_sample_time = now;
    digitalWrite(LED_BUILTIN, LOW); // Flash LED during measurement

    // A. Outlier-filtered median distance reading
    long raw_median = getFilteredDistanceCm();

    // B. Apply Exponential Moving Average (EMA) smoothing
    if (distance_to_water == 0 && raw_median > 0) {
      distance_to_water = raw_median; // Seed initial value
    } else {
      distance_to_water = applyEma(distance_to_water, raw_median, 0.25f);
    }

    long water_level = atol(tank_bottom_distance) - distance_to_water;
    float max_level_float = atof(max_water_level);
    int normalized_level = calculateNormalizedLevel(water_level, max_level_float, ledcount);

    float pct = (max_level_float > 0.0f) ? ((float)water_level / max_level_float) * 100.0f : 0.0f;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;

    EnvReadings env = readEnvSensors();
    publishMqttTelemetry(distance_to_water, water_level, pct, env.temperature, env.humidity);

    if (isMqttDebugEnabled()) {
      char dbgBuf[256];
      snprintf(dbgBuf, sizeof(dbgBuf),
        "DIAG: raw_median=%ld cm, smoothed_dist=%ld cm, tank_bottom=%ld cm, water_level=%ld cm, max_water=%ld cm, pct=%.1f%%, temp=%.1f C, hum=%.1f %%",
        raw_median, distance_to_water, atol(tank_bottom_distance), water_level, atol(max_water_level), pct, env.temperature, env.humidity);
      publishMqttDebug(dbgBuf);
    }

    Serial.printf("raw_median = %ld, dist = %ld cm, level = %ld cm (%.1f%%, norm=%d), FreeHeap = %u B\n",
      raw_median, distance_to_water, water_level, pct, normalized_level, ESP.getFreeHeap());

    digitalWrite(LED_BUILTIN, HIGH);
  }

  delay(10); // Small yield to prevent CPU hogging while maintaining fast responsiveness
}
#endif
