#include "ConfigManager.h"

ConfigManager::ConfigManager() : _path("/config.json"), _configured(false) {}

bool ConfigManager::begin() {
    if (!LittleFS.begin(false)) {
        Serial.println("[Config] LittleFS mount failed, formatting...");
        if (!LittleFS.begin(true)) {
            Serial.println("[Config] LittleFS mount failed!");
            return false;
        }
    }
    Serial.printf("[Storage] Mounted. Total: %u, Used: %u\n",
        LittleFS.totalBytes(), LittleFS.usedBytes());
    return load();
}

bool ConfigManager::load() {
    if (!LittleFS.exists(_path)) {
        Serial.println("[Config] No config file found");
        _configured = false;
        return false;
    }
    File f = LittleFS.open(_path, "r");
    if (!f) {
        Serial.println("[Config] Failed to open config file");
        return false;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("[Config] Parse error: %s\n", err.c_str());
        return false;
    }
    _ssid = doc["wifi_ssid"] | "";
    _pass = doc["wifi_pass"] | "";
    _name = doc["device_name"] | "ESPortable32";
    _pin = doc["pin"] | "";
    _configured = doc["configured"] | false;
    Serial.printf("[Config] Loaded. WiFi: %s, Name: %s, PIN: %s\n",
        _ssid.c_str(), _name.c_str(), _pin.length() > 0 ? "set" : "not set");
    return true;
}

bool ConfigManager::save() {
    JsonDocument doc;
    doc["wifi_ssid"] = _ssid;
    doc["wifi_pass"] = _pass;
    doc["device_name"] = _name;
    doc["pin"] = _pin;
    doc["configured"] = _configured;

    File f = LittleFS.open(_path, "w");
    if (!f) {
        Serial.println("[Config] Failed to write config file");
        return false;
    }
    serializeJson(doc, f);
    f.close();
    Serial.println("[Config] Saved");
    return true;
}

String ConfigManager::getWifiSSID() { return _ssid; }
String ConfigManager::getWifiPass() { return _pass; }
String ConfigManager::getDeviceName() { return _name; }
String ConfigManager::getPin() { return _pin; }
bool ConfigManager::isConfigured() { return _configured; }

void ConfigManager::setWifi(const String& ssid, const String& pass) {
    _ssid = ssid;
    _pass = pass;
}

void ConfigManager::setDeviceName(const String& name) { _name = name; }
void ConfigManager::setPin(const String& pin) { _pin = pin; }

JsonDocument& ConfigManager::getDoc() {
    static JsonDocument doc;
    doc.clear();
    doc["wifi_ssid"] = _ssid;
    doc["wifi_pass"] = _pass;
    doc["device_name"] = _name;
    doc["pin"] = _pin;
    doc["configured"] = _configured;
    return doc;
}
