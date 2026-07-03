#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

class ConfigManager {
public:
    ConfigManager();
    bool begin();
    bool load();
    bool save();

    String getWifiSSID();
    String getWifiPass();
    String getDeviceName();
    String getPin();
    bool isConfigured();

    void setWifi(const String& ssid, const String& pass);
    void setDeviceName(const String& name);
    void setPin(const String& pin);

    JsonDocument& getDoc();

private:
    String _path;
    String _ssid;
    String _pass;
    String _name;
    String _pin;
    bool _configured;
};
