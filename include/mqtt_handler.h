#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>

void initMqtt(const char* broker_host = "YOUR_MQTT_BROKER_IP", uint16_t port = 1883);
void handleMqtt();
bool isMqttConnected();
void publishMqttTelemetry(long distance_cm, long water_level_cm, float percent, float temp_c, float hum_pct);

bool isStayAwakeRequested();
unsigned long getStayAwakeRemainingSec();
void setStayAwake(bool enable, uint32_t duration_sec = 600);

bool isMqttDebugEnabled();
void setMqttDebugEnabled(bool enable);
void publishMqttDebug(const char* message);

#endif // MQTT_HANDLER_H
