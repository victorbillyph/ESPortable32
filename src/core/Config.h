#ifndef CORE_CONFIG_H
#define CORE_CONFIG_H

#include <Arduino.h>
#include <Preferences.h>

class Config {
public:
    Config();
    void begin();

    bool isConfigured();
    String getSSID();
    String getPass();
    String getDeviceName();

    void setWiFi(const char* ssid, const char* pass);
    void setDeviceName(const char* name);
    void saveAll(const char* name, const char* ssid, const char* pass);
    void clear();
    void end();
    int getInt(const char* key, int def);
    void setInt(const char* key, int val);

private:
    Preferences _prefs;
    const char* _ns = "esp32watch";
};

#endif
