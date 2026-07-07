#include "Config.h"

Config::Config() {}

void Config::begin() {
    if (!_prefs.begin(_ns, false)) {
        Serial.println("[Config] Falhou ao abrir Preferences");
    }
}

bool Config::isConfigured() {
    return _prefs.getBool("configured", false);
}

String Config::getSSID() {
    return _prefs.getString("ssid", "");
}

String Config::getPass() {
    return _prefs.getString("pass", "");
}

String Config::getDeviceName() {
    return _prefs.getString("name", "ESPortable32");
}

void Config::setWiFi(const char* ssid, const char* pass) {
    _prefs.putString("ssid", ssid);
    _prefs.putString("pass", pass);
    _prefs.putBool("configured", true);
    _prefs.end();
    Serial.printf("[Config] WiFi salvo: %s\n", ssid);
}

void Config::setDeviceName(const char* name) {
    _prefs.putString("name", name);
    _prefs.end();
    Serial.printf("[Config] Nome salvo: %s\n", name);
}

void Config::saveAll(const char* name, const char* ssid, const char* pass) {
    _prefs.putString("name", name);
    _prefs.putString("ssid", ssid);
    _prefs.putString("pass", pass);
    _prefs.putBool("configured", true);
    _prefs.end();
    Serial.printf("[Config] Tudo salvo: %s / %s\n", name, ssid);
}

void Config::clear() {
    _prefs.clear();
    _prefs.end();
    Serial.println("[Config] Configuracoes limpas");
}

void Config::end() {
    _prefs.end();
}

int Config::getInt(const char* key, int def) {
    return _prefs.getInt(key, def);
}

void Config::setInt(const char* key, int val) {
    _prefs.putInt(key, val);
    _prefs.end();
}
