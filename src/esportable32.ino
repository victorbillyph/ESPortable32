#include <WiFi.h>
#include <LittleFS.h>
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

void processSerialCommand(String cmd) {
    if (cmd.length() == 0) return;

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
