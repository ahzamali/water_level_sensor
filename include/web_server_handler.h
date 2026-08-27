#ifndef WEB_SERVER_HANDLER_H
#define WEB_SERVER_HANDLER_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <memory>

// Initializes web server routes (Dashboard, JSON API, Config, OTA Update, WiFi Reset)
void initWebServer(std::unique_ptr<ESP8266WebServer>& server_ptr);

// Initializes standard ArduinoOTA (port 8266) for IDE/PlatformIO flashing
void initArduinoOta();

// Handles incoming client HTTP requests & ArduinoOTA packets
void handleWebRoutes();

// Returns true if an OTA upload is currently active
bool isOtaInProgress();

#endif // WEB_SERVER_HANDLER_H
