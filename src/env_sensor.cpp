#include "env_sensor.h"
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <DHT.h>

static Adafruit_BME280 bme;
static DHT* dht_sensor = nullptr;
static bool bme_ok = false;
static bool dht_ok = false;

void initEnvSensors(uint8_t dht_pin) {
  Wire.begin();
  
  if (bme.begin(0x76)) {
    bme_ok = true;
    Serial.println("initEnvSensors: BME280 sensor initialized at 0x76");
  } else if (bme.begin(0x77)) {
    bme_ok = true;
    Serial.println("initEnvSensors: BME280 sensor initialized at 0x77");
  } else {
    Serial.println("initEnvSensors: BME280 sensor not detected");
  }

  if (dht_sensor == nullptr) {
    dht_sensor = new DHT(dht_pin, DHT11);
    dht_sensor->begin();
    dht_ok = true;
    Serial.printf("initEnvSensors: DHT11 initialized on pin %d\n", dht_pin);
  }
}

EnvReadings readEnvSensors() {
  EnvReadings res;
  res.temperature = NAN;
  res.humidity = NAN;
  res.pressure = NAN;
  res.bme280_present = bme_ok;
  res.dht_present = dht_ok;

  if (bme_ok) {
    float t = bme.readTemperature();
    float h = bme.readHumidity();
    float p = bme.readPressure() / 100.0f; // Pa to hPa
    if (!isnan(t)) res.temperature = t;
    if (!isnan(h)) res.humidity = h;
    if (!isnan(p)) res.pressure = p;
  }

  if (isnan(res.temperature) && dht_ok && dht_sensor != nullptr) {
    float t = dht_sensor->readTemperature();
    float h = dht_sensor->readHumidity();
    if (!isnan(t)) res.temperature = t;
    if (!isnan(h)) res.humidity = h;
  }

  return res;
}
