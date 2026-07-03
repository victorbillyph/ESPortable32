#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "StateManager.h"
#include "ConfigManager.h"
#include "WebServer.h"
#include "WebSockets.h"
#include "BluetoothService.h"

StateManager state;
ConfigManager config;
WebSockets webSockets;
WebServer webServer(state, config, webSockets);
BluetoothService bluetooth;

unsigned long lastSerialCheck = 0;
bool setupModeEnabled = false;

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.println("\n=== ESPortable32 v1.0.0 ===");

    state.begin();

    if (!config.begin()) {
        Serial.println("[Setup] No configuration found, starting setup mode...");
        startSetupMode();
        return;
    }

    state.setPin(config.getPin());
    if (config.getPin().length() == 0) {
        state.setLocked(false);
    }
    state.setState(STATE_CONFIGURED);
    connectWiFi();
}

void loop() {
    if (setupModeEnabled) {
        handleSetupLoop();
        return;
    }

    bluetooth.update();
    webServer.handleClient();

    if (millis() - lastSerialCheck > 500) {
        lastSerialCheck = millis();
        if (Serial.available()) {
            String cmd = Serial.readStringUntil('\n');
            cmd.trim();
            processSerialCommand(cmd);
        }
        if (bluetooth.available()) {
            String cmd = bluetooth.readStringUntil('\n');
            cmd.trim();
            processSerialCommand(cmd);
        }
    }

    static unsigned long lastStatus = 0;
    if (millis() - lastStatus > 30000) {
        lastStatus = millis();
        Serial.printf("[Heartbeat] State: %d, Heap: %u, WiFi RSSI: %d\n",
            state.getState(), ESP.getFreeHeap(), WiFi.RSSI());
    }
}

void startSetupMode() {
    setupModeEnabled = true;
    state.setState(STATE_SETUP);

    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESPortable32-Setup", "configurar");

    bluetooth.begin("ESPortable32");
    webServer.begin();

    Serial.println("─── Modo Setup ───");
    printHelp();
    Serial.printf("\nWiFi AP: ESPortable32-Setup\nSenha:   configurar\nURL:     http://192.168.4.1\n");
}

void connectWiFi() {
    String ssid = config.getWifiSSID();
    String pass = config.getWifiPass();

    if (ssid.length() == 0) {
        Serial.println("[WiFi] No SSID configured, starting setup mode...");
        startSetupMode();
        return;
    }

    Serial.printf("[WiFi] Connecting to %s...\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    for (int i = 0; i < 20; i++) {
        if (WiFi.status() == WL_CONNECTED) break;
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        state.setState(STATE_CONNECTED);
        bluetooth.begin(config.getDeviceName());
        webServer.begin();
        webSockets.begin(*webServer.getServer());
        state.setState(STATE_READY);
        Serial.printf("[Server] Ready at http://%s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n[WiFi] Failed to connect");
        startSetupMode();
    }
}

void handleSetupLoop() {
    bluetooth.update();

    if (millis() - lastSerialCheck > 100) {
        lastSerialCheck = millis();
        if (Serial.available()) {
            String cmd = Serial.readStringUntil('\n');
            cmd.trim();
            processSerialCommand(cmd);
        }
        if (bluetooth.available()) {
            String cmd = bluetooth.readStringUntil('\n');
            cmd.trim();
            processSerialCommand(cmd);
        }
    }
}

String apiResponseJson(bool ok, const String& data) {
    return "{\"status\":\"" + String(ok ? "ok" : "error") + "\",\"data\":" + data + "}";
}

void processSerialCommand(String cmd) {
    if (cmd.length() == 0) return;

    // JSON API via serial
    if (cmd.startsWith("{")) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, cmd);
        if (!err) {
            String action = doc["action"] | "";
            if (action == "gpio_list") {
                const int pins[] = {2,4,5,12,13,14,15,16,17,18,19,21,22,23,25,26,27,32,33};
                String json = "[";
                for (int i = 0; i < 19; i++) {
                    int p = pins[i];
                    if (i > 0) json += ",";
                    json += "{\"pin\":" + String(p) + ",\"state\":" + String(digitalRead(p)) + "}";
                }
                json += "]";
                Serial.println(apiResponseJson(true, json));
                return;
            }
            if (action == "gpio_set") {
                int pin = doc["pin"] | -1;
                int state = doc["state"] | 0;
                if (pin >= 0 && pin < 40) {
                    pinMode(pin, OUTPUT);
                    digitalWrite(pin, state);
                    Serial.println(apiResponseJson(true, "{\"pin\":" + String(pin) + ",\"state\":" + String(state) + "}"));
                    return;
                }
            }
            if (action == "fs_list") {
                String path = doc["path"] | "/";
                File root = LittleFS.open(path);
                if (root && root.isDirectory()) {
                    String json = "[";
                    File f = root.openNextFile();
                    bool first = true;
                    while (f) {
                        if (!first) json += ",";
                        first = false;
                        json += "{\"name\":\"" + String(f.name()) + "\",\"size\":" + String(f.size()) + ",\"dir\":" + String(f.isDirectory() ? "true" : "false") + "}";
                        f = root.openNextFile();
                    }
                    json += "]";
                    Serial.println(apiResponseJson(true, json));
                    return;
                }
            }
            if (action == "fs_read") {
                String path = doc["path"] | "";
                if (LittleFS.exists(path)) {
                    File f = LittleFS.open(path, "r");
                    if (f) {
                        String content = f.readString();
                        f.close();
                        String json = "{\"path\":\"" + path + "\",\"size\":" + String(content.length()) + ",\"content\":\"";
                        for (size_t i = 0; i < content.length(); i++) {
                            char c = content.charAt(i);
                            if (c == '"') json += "\\\"";
                            else if (c == '\\') json += "\\\\";
                            else if (c == '\n') json += "\\n";
                            else if (c == '\r') json += "\\r";
                            else json += c;
                        }
                        json += "\"}";
                        Serial.println(apiResponseJson(true, json));
                        return;
                    }
                }
            }
        }
        Serial.println(apiResponseJson(false, "\"unknown\""));
        return;
    }

    Serial.printf("> %s\n", cmd.c_str());

    if (cmd == "HELP") {
        printHelp();
    } else if (cmd == "STATUS") {
        Serial.printf("State: %d (%s)\n", state.getState(), state.getStateName());
        Serial.printf("WiFi Mode: %d, RSSI: %d\n", WiFi.getMode(), WiFi.RSSI());
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Free Heap: %u bytes\n", ESP.getFreeHeap());
        Serial.printf("Uptime: %lu seconds\n", millis() / 1000);
    } else if (cmd.startsWith("WIFI=")) {
        String rest = cmd.substring(5);
        int comma = rest.indexOf(',');
        if (comma > 0) {
            config.setWifi(rest.substring(0, comma), rest.substring(comma + 1));
            Serial.println("OK:WiFi configurado");
        } else {
            Serial.println("ERRO:Formato WIFI=ssid,pass");
        }
    } else if (cmd.startsWith("NAME=")) {
        config.setDeviceName(cmd.substring(5));
        Serial.println("OK:Nome configurado");
    } else if (cmd.startsWith("PIN=")) {
        String pin = cmd.substring(4);
        config.setPin(pin);
        state.setPin(pin);
        Serial.println("OK:PIN configurado");
    } else if (cmd == "SAVE") {
        config.save();
        Serial.println("OK:Salvo! Reiniciando...");
        delay(500);
        ESP.restart();
    } else if (cmd == "RESET") {
        LittleFS.remove("/config.json");
        Serial.println("OK:Resetado! Reiniciando...");
        delay(500);
        ESP.restart();
    } else if (cmd.startsWith("BT=")) {
        bool on = cmd.substring(3) == "on";
        bluetooth.setEnabled(on);
        if (on) {
            bluetooth.begin(config.getDeviceName());
        } else {
            bluetooth.end();
        }
        Serial.printf("OK:Bluetooth %s\n", on ? "ligado" : "desligado");
    } else {
        Serial.println("ERRO:Comando desconhecido. Digite HELP");
    }
}

void printHelp() {
    Serial.println("╔══════════════════════════════════╗");
    Serial.println("║      ESPortable32 v1.0.0        ║");
    Serial.println("╚══════════════════════════════════╝");
    Serial.println("Comandos:");
    Serial.println("  HELP            - Mostra esta ajuda");
    Serial.println("  STATUS          - Status do sistema");
    Serial.println("  WIFI=ssid,pass  - Configurar WiFi");
    Serial.println("  NAME=nome       - Nome do dispositivo");
    Serial.println("  PIN=1234        - Configurar PIN");
    Serial.println("  SAVE            - Salvar e reiniciar");
    Serial.println("  RESET           - Reset de fabrica");
    Serial.println("  BT=on|off       - Ligar/desligar Bluetooth");
}
