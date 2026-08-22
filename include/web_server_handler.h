#ifndef WEB_SERVER_HANDLER_H
#define WEB_SERVER_HANDLER_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <memory>

// Initializes web server routes (Dashboard, JSON API, Config, OTA Update, WiFi Reset)
void initWebServer(std::unique_ptr<ESP8266WebServer>& server_ptr);

// Handles incoming client HTTP requests
void handleWebRoutes();

#endif // WEB_SERVER_HANDLER_H
