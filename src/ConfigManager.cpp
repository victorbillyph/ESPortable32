#include "ConfigManager.h"

ConfigManager::ConfigManager() : _configured(false) {}

bool ConfigManager::begin() {
    return load();
}

bool ConfigManager::load() {
    _prefs.begin("esportable", false);
    _ssid = _prefs.getString("wifi_ssid", "");
    _pass = _prefs.getString("wifi_pass", "");
    _name = _prefs.getString("device_name", "ESPortable32");
    _pin = _prefs.getString("pin", "");
    _configured = _prefs.getBool("configured", false);
    _prefs.end();

    Serial.printf("[Config] Loaded. WiFi: %s, Name: %s, PIN: %s\n",
        _ssid.length() > 0 ? _ssid.c_str() : "(none)",
        _name.c_str(),
        _pin.length() > 0 ? "set" : "not set");
    return true;
}

bool ConfigManager::save() {
    _prefs.begin("esportable", false);
    _prefs.putString("wifi_ssid", _ssid);
    _prefs.putString("wifi_pass", _pass);
    _prefs.putString("device_name", _name);
    _prefs.putString("pin", _pin);
    _prefs.putBool("configured", _configured);
    _prefs.end();
    Serial.println("[Config] Saved");
    return true;
}

String ConfigManager::getWifiSSID() { return _ssid; }
String ConfigManager::getWifiPass() { return _pass; }
String ConfigManager::getDeviceName() { return _name; }
String ConfigManager::getPin() { return _pin; }
bool ConfigManager::isConfigured() { return _configured; }

void ConfigManager::setWifi(const String& ssid, const String& pass) {
    _ssid = ssid; _pass = pass;
}

void ConfigManager::setDeviceName(const String& name) { _name = name; }
void ConfigManager::setPin(const String& pin) { _pin = pin; }

void ConfigManager::clear() {
    _prefs.begin("esportable", false);
    _prefs.clear();
    _prefs.end();
    _ssid = ""; _pass = ""; _name = "ESPortable32"; _pin = ""; _configured = false;
}
