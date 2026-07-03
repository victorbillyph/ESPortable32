#pragma once
#include <Arduino.h>
#include <map>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "StateManager.h"
#include "ConfigManager.h"
#include "WebSockets.h"

class WebServer {
public:
    WebServer(StateManager& state, ConfigManager& config, WebSockets& ws);
    void begin();
    void handleClient();
    AsyncWebServer* getServer() { return &_server; }
    bool isCaptive();

private:
    AsyncWebServer _server;
    StateManager& _state;
    ConfigManager& _config;
    WebSockets& _ws;
    bool _captive;
    std::map<String, String> _bodyBuffer;

    void handleRequest(AsyncWebServerRequest* request);
    void handleFileRequest(AsyncWebServerRequest* request);
    void handleCaptive(AsyncWebServerRequest* request);
    void handleApiStatus(AsyncWebServerRequest* request);
    void handleApiUnlock(AsyncWebServerRequest* request);
    void handleApiRestart(AsyncWebServerRequest* request);
    String getContentType(const String& path);
    void handleSerialCommand(const String& cmd, AsyncWebServerRequest* request);
};
