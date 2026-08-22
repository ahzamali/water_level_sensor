#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include "WiFiManager.h"
#include <FS.h>  
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
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
void wifiManagerSetup(WiFiManager& wifiManager);
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

HTTPClient http;

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
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        
        if (json.success()) {
          if(json["speed_of_sound"]) strcpy(speed_of_sound, json["speed_of_sound"]);
          if(json["tank_bottom_distance"]) strcpy(tank_bottom_distance, json["tank_bottom_distance"]);
          if(json["max_water_level"]) strcpy(max_water_level, json["max_water_level"]);
        }
        configFile.close();
      }
    }
  }
  return 0;
}

void wifiManagerSetup(WiFiManager& wifiManager) {
  WiFiManagerParameter custom_speed_of_sound("speedofsound", "speed of sound", speed_of_sound, 6);
  WiFiManagerParameter custom_tank_bottom_distance("tankbottomdistance", "tank bottom distance", tank_bottom_distance, 6);
  WiFiManagerParameter custom_max_water_level("maxwaterlevel", "max water level", max_water_level, 6);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&custom_speed_of_sound);
  wifiManager.addParameter(&custom_tank_bottom_distance);
  wifiManager.addParameter(&custom_max_water_level);
  Serial.println("wifiManagerSetup: Trying AutoConnect ...");
  wifiManager.autoConnect("WATER_LEVEL_SENSOR");
  strcpy(speed_of_sound, custom_speed_of_sound.getValue());
  strcpy(tank_bottom_distance, custom_tank_bottom_distance.getValue());
  strcpy(max_water_level, custom_max_water_level.getValue());
}

void saveConfig() {
  Serial.println("saveConfig: Saving Config to SPIFFS");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["speed_of_sound"] = speed_of_sound;
  json["tank_bottom_distance"] = tank_bottom_distance;
  json["max_water_level"] = max_water_level;

  File configFile = SPIFFS.open("/config.json", "w");
  if (configFile) {
    json.printTo(configFile);
    configFile.close();
  }
}

void createWebServer() {
  initWebServer(server);
}

int rrr = readSpiffsConfig();

#ifndef UNIT_TEST
void setup() {
  Serial.begin(115200);
  Serial.println();
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  http.setReuse(true);

  WiFiManager wifiManager;
  wifiManagerSetup(wifiManager);
  Serial.print("setup: Connected to Router: IP = ");
  Serial.println(WiFi.localIP());

  // Sync NTP time (IST UTC+5:30 = 19800 seconds)
  configTime(19800, 0, "pool.ntp.org", "time.nist.gov");

  // Enforce tank dimensions from WaterLevelTempSensorV2.ino: 98cm depth, 75cm usable water height
  strcpy(tank_bottom_distance, "98");
  strcpy(max_water_level, "75");
  saveConfig(); // Persist 98cm and 75cm to SPIFFS config.json

  createWebServer();  
  Serial.println("setup: HTTP server started.");

  initEnvSensors(D5);
  initMqtt("192.168.211.175", 1883);

  // Only start 3-minute stay-awake window on Cold Boot / Reset button press (not on Deep Sleep wake-up)
  rst_info* rstInfo = ESP.getResetInfoPtr();
  if (rstInfo != nullptr && rstInfo->reason != REASON_DEEP_SLEEP_AWAKE) {
    Serial.println("setup: Cold boot / Reset button detected. Activating initial 3-min Web UI stay-awake window.");
    setStayAwake(true, 180);
  } else {
    Serial.println("setup: Deep-Sleep timer wake-up. Skipping initial stay-awake window.");
  }

  pinMode(trigger, OUTPUT);
  pinMode(echo, INPUT);

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
  long tank_max = String(tank_bottom_distance).toInt();

  if (!isSampleValid(dist, tank_max)) {
    return -1;
  }
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
  digitalWrite(LED_BUILTIN, HIGH);

  handleMqtt();

  // 1. Obtain outlier-filtered median distance reading
  long raw_median = getFilteredDistanceCm();

  // 2. Apply Exponential Moving Average (EMA) smoothing
  if (distance_to_water == 0 && raw_median > 0) {
    distance_to_water = raw_median; // Seed initial value
  } else {
    distance_to_water = applyEma(distance_to_water, raw_median, 0.25f);
  }

  long water_level = String(tank_bottom_distance).toInt() - distance_to_water;

  float max_level_float = String(max_water_level).toFloat();
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
      raw_median, distance_to_water, String(tank_bottom_distance).toInt(), water_level, String(max_water_level).toInt(), pct, env.temperature, env.humidity);
    publishMqttDebug(dbgBuf);
  }

  Serial.println("raw_median = " + String(raw_median));
  Serial.println("distance_to_water (smoothed) = " + String(distance_to_water));
  Serial.println("water_level = " + String(water_level));
  Serial.println("normalized_level = " + String(normalized_level));

  digitalWrite(LED_BUILTIN, LOW);

  // Give 2-second window for incoming MQTT control commands and Web UI requests
  for (int i = 0; i < 20; i++) {
    handleWebRoutes();
    handleMqtt();
    delay(100);
  }

  // Check if stay-awake is active (via Web UI / MQTT control topic 'roof/tank_water/control')
  if (isStayAwakeRequested()) {
    Serial.printf("Stay-Awake Active: %lu sec remaining. Maintaining Web UI mode...\n", getStayAwakeRemainingSec());
    for (int i = 0; i < 20; i++) {
      handleWebRoutes();
      handleMqtt();
      delay(100);
    }
  } else {
    // Adaptive sleep duration based on time of day (NTP)
    uint32_t sleep_sec = 60; // default 1 min
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);

    if (timeinfo && timeinfo->tm_year > 120) { // Valid NTP time acquired (> year 2020)
      int minOfDay = timeinfo->tm_hour * 60 + timeinfo->tm_min;
      // 5:30 AM = 330 min, 11:59 PM = 1439 min
      if (minOfDay >= 330 && minOfDay <= 1439) {
        sleep_sec = 60; // 1 minute sleep during daytime (5:30 AM - 11:59 PM)
        Serial.printf("NTP Time %02d:%02d -> Daytime active. Sleep set to 1 min (60s).\n", timeinfo->tm_hour, timeinfo->tm_min);
      } else {
        sleep_sec = 900; // 15 minutes sleep during nighttime (12:00 AM - 5:29 AM)
        Serial.printf("NTP Time %02d:%02d -> Nighttime active. Sleep set to 15 min (900s).\n", timeinfo->tm_hour, timeinfo->tm_min);
      }
    } else {
      Serial.println("NTP Time not yet synced -> Defaulting sleep to 1 min (60s).");
    }

    if (isMqttDebugEnabled()) {
      char dbgBuf[128];
      snprintf(dbgBuf, sizeof(dbgBuf), "ADAPTIVE SLEEP: Entering Deep Sleep for %u seconds.", sleep_sec);
      publishMqttDebug(dbgBuf);
    }

    Serial.printf("Entering Deep Sleep for %u seconds...\n", sleep_sec);
    ESP.deepSleep((uint64_t)sleep_sec * 1000000ULL);
  }
}
#endif
