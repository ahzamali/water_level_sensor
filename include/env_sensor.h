#ifndef ENV_SENSOR_H
#define ENV_SENSOR_H

#include <Arduino.h>

struct EnvReadings {
  float temperature; // °C (NAN if invalid)
  float humidity;    // % (NAN if invalid)
  float pressure;    // hPa (NAN if invalid)
  bool bme280_present;
  bool dht_present;
};

void initEnvSensors(uint8_t dht_pin = 14); // D5 is GPIO 14 on ESP8266
EnvReadings readEnvSensors();

#endif // ENV_SENSOR_H
