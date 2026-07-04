#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "StateManager.h"
#include "ConfigManager.h"
#include "WebServer.h"
#include "WebSockets.h"
#include "BluetoothService.h"
#include "Display.h"

#define FIRMWARE_VERSION "1.1.0"
#define BTN_PIN      0
#define BTN_DEBOUNCE 50
#define LONG_PRESS_MS 400

StateManager state;
ConfigManager config;
WebSockets webSockets;
WebServer webServer(state, config, webSockets);
BluetoothService bluetooth;
Display display;

unsigned long lastSerialCheck = 0;
bool setupModeEnabled = false;

// OLED menu state
int menuSelection = 0;
int currentScreen = 0; // 0=menu, 1+=apps
int appPage = 0;
unsigned long lastBtnState = HIGH;
unsigned long btnPressTime = 0;
bool btnLongPressHandled = false;

enum Screen { SCR_MENU = 0, SCR_DASHBOARD, SCR_GPIO, SCR_FILES, SCR_SETTINGS, SCR_INFO, SCR_COUNT };

const char* menuItems[] = {
    "Dashboard",
    "GPIO",
    "Files",
    "Settings",
    "Info"
};
const int menuCount = sizeof(menuItems) / sizeof(menuItems[0]);

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n=== ESPortable32 v1.1.0 ===");

    pinMode(BTN_PIN, INPUT_PULLUP);
    lastBtnState = digitalRead(BTN_PIN);

    display.begin();
    display.splash("ESPortable32", FIRMWARE_VERSION);
    delay(1500);

    state.begin();

    if (!config.begin()) {
        Serial.println("[Setup] No configuration found, starting setup mode...");
        display.showMessage("Modo Setup");
        startSetupMode();
        return;
    }

    state.setPin(config.getPin());
    if (config.getPin().length() == 0) {
        state.setLocked(false);
    }
    state.setState(STATE_CONFIGURED);
    connectWiFi();
    renderMenu();
}

void loop() {
    if (setupModeEnabled) {
        handleSetupLoop();
        handleButton();
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
    if (millis() - lastStatus > 5000) {
        lastStatus = millis();
        Serial.printf("[Heartbeat] State: %d, Heap: %u, WiFi RSSI: %d\n",
            state.getState(), ESP.getFreeHeap(), WiFi.RSSI());
        webSockets.broadcast("{\"uptime\":" + String(millis() / 1000) +
            ",\"free_heap\":" + String(ESP.getFreeHeap()) +
            ",\"wifi_rssi\":" + String(WiFi.RSSI()) +
            ",\"state\":" + String(state.getState()) + "}");
        if (currentScreen == SCR_MENU) renderMenu();
    }

    handleButton();
    updateScreen();
}

// ── Button handling ─────────────────────────────────────────────

void handleButton() {
    bool reading = digitalRead(BTN_PIN);
    if (reading != lastBtnState) {
        lastBtnState = reading;
        if (reading == LOW) {
            btnPressTime = millis();
            btnLongPressHandled = false;
        } else {
            unsigned long elapsed = millis() - btnPressTime;
            if (elapsed < LONG_PRESS_MS) {
                shortPress();
            }
        }
    }
    if (lastBtnState == LOW && !btnLongPressHandled && (millis() - btnPressTime >= LONG_PRESS_MS)) {
        btnLongPressHandled = true;
        longPress();
    }
}

void shortPress() {
    if (currentScreen == SCR_MENU) {
        menuSelection = (menuSelection + 1) % menuCount;
        renderMenu();
    } else {
        // In app: go back to menu
        currentScreen = SCR_MENU;
        appPage = 0;
        renderMenu();
    }
}

void longPress() {
    if (currentScreen == SCR_MENU) {
        currentScreen = menuSelection + 1;
        appPage = 0;
        renderScreen();
    } else {
        // In app: next page / action
        appPage++;
        renderScreen();
    }
}

// ── Screen rendering ───────────────────────────────────────────

void renderMenu() {
    display.showMenu("ESPortable32", menuItems, menuCount, menuSelection);
}

void renderScreen() {
    switch (currentScreen) {
        case SCR_DASHBOARD: renderDashboard(); break;
        case SCR_GPIO:      renderGPIO(); break;
        case SCR_FILES:     renderFiles(); break;
        case SCR_SETTINGS:  renderSettings(); break;
        case SCR_INFO:      renderInfo(); break;
        default: currentScreen = SCR_MENU; renderMenu(); break;
    }
}

void updateScreen() {
    static unsigned long last = 0;
    if (currentScreen == SCR_DASHBOARD && millis() - last > 2000) {
        last = millis();
        renderDashboard();
    }
}

// ── OLED Apps ───────────────────────────────────────────────────

void renderDashboard() {
    display.clear();
    display.drawTitle("Dashboard");
    int y = 12;
    char buf[22];
    display.drawText(1, y, "WiFi: ", 1); y += 10;
    snprintf(buf, sizeof(buf), "  %s", WiFi.status() == WL_CONNECTED ? WiFi.SSID().c_str() : "---");
    display.drawText(1, y, buf, 1); y += 10;
    snprintf(buf, sizeof(buf), "IP: %s", WiFi.localIP().toString().c_str());
    display.drawText(1, y, buf, 1); y += 10;
    snprintf(buf, sizeof(buf), "Heap: %u KB", ESP.getFreeHeap() / 1024);
    display.drawText(1, y, buf, 1); y += 10;
    snprintf(buf, sizeof(buf), "Uptime: %lus", millis() / 1000);
    display.drawText(1, y, buf, 1);
    display.drawStatusBar("Pressione p/ voltar", nullptr);
    display.show();
}

void renderGPIO() {
    display.clear();
    display.drawTitle("GPIO");
    const int pins[] = {2,4,5,12,13,14,15,16,17,18,19,21,22,23,25,26,27,32,33};
    int n = sizeof(pins) / sizeof(pins[0]);
    int perPage = 6;
    int start = appPage * perPage;
    if (start >= n) { appPage = 0; start = 0; }
    int y = 12;
    char buf[22];
    for (int i = start; i < n && i < start + perPage; i++) {
        int p = pins[i];
        int val = digitalRead(p);
        snprintf(buf, sizeof(buf), "GPIO%02d: %s", p, val ? "HIGH" : "LOW ");
        display.drawText(1, y, buf, 1);
        y += 9;
    }
    char page[8];
    snprintf(page, sizeof(page), "%d/%d", appPage + 1, (n + perPage - 1) / perPage);
    display.drawStatusBar(page, "Segure p/ prox");
    display.show();
}

void renderFiles() {
    display.clear();
    display.drawTitle("Files");
    int y = 12;
    char buf[22];
    if (!LittleFS.exists("/")) {
        display.showCentered(28, "No FS", 1);
        display.show();
        return;
    }
    File root = LittleFS.open("/");
    if (!root || !root.isDirectory()) {
        display.showCentered(28, "Erro ao abrir", 1);
        display.show();
        return;
    }
    int perPage = 5;
    int idx = 0;
    int start = appPage * perPage;
    int shown = 0;
    File f = root.openNextFile();
    while (f && shown < perPage) {
        if (idx >= start) {
            String fn = String(f.name());
            if (fn.length() > 15) fn = fn.substring(0, 14) + "~";
            snprintf(buf, sizeof(buf), " %s", fn.c_str());
            display.drawText(1, y, buf, 1);
            y += 9;
            shown++;
        }
        idx++;
        f = root.openNextFile();
    }
    if (shown == 0) {
        display.drawText(1, 28, " (vazio)", 1);
    }
    char page[8];
    int totalFiles = idx;
    snprintf(page, sizeof(page), "%d/%d", appPage + 1, (totalFiles + perPage - 1) / perPage);
    display.drawStatusBar(page, "Segure p/ prox");
    display.show();
}

void renderSettings() {
    display.clear();
    display.drawTitle("Settings");
    int y = 12;
    char buf[22];
    snprintf(buf, sizeof(buf), "Nome: %s", config.getDeviceName().c_str());
    display.drawText(1, y, buf, 1); y += 10;
    snprintf(buf, sizeof(buf), "WiFi: %s", config.getWifiSSID().c_str());
    display.drawText(1, y, buf, 1); y += 10;
    snprintf(buf, sizeof(buf), "Estado: %s", state.getStateName());
    display.drawText(1, y, buf, 1); y += 10;
    snprintf(buf, sizeof(buf), "PIN: %s", config.getPin().length() > 0 ? "****" : "(none)");
    display.drawText(1, y, buf, 1);
    display.drawStatusBar("Pressione p/ voltar", "Segure=reiniciar");
    display.show();
}

void renderInfo() {
    display.clear();
    int y = 16;
    char buf[22];
    display.showCentered(4, "ESPortable32", 1);
    snprintf(buf, sizeof(buf), "Firmware: %s", FIRMWARE_VERSION);
    display.showCentered(y, buf, 1); y += 12;
    snprintf(buf, sizeof(buf), "Chip: ESP32");
    display.showCentered(y, buf, 1); y += 10;
    snprintf(buf, sizeof(buf), "Flash: 4MB");
    display.showCentered(y, buf, 1); y += 10;
    snprintf(buf, sizeof(buf), "Display: 128x64 OLED");
    display.showCentered(y, buf, 1);
    display.drawStatusBar("Pressione p/ voltar", nullptr);
    display.show();
}

// ── Existing functions (unchanged below) ───────────────────────

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

    display.showMessage("Conectando WiFi...");
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
        display.showMessage("WiFi OK!");
        delay(500);
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
    } else if (cmd == "VERSION") {
        Serial.printf("FIRMWARE_VERSION=%s\n", FIRMWARE_VERSION);
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
    } else if (cmd == "SCAN") {
        int n = WiFi.scanNetworks();
        for (int i = 0; i < n; i++) {
            Serial.print("NET:");
            Serial.println(WiFi.SSID(i));
        }
        Serial.printf("OK:%d networks found\n", n);
    } else {
        Serial.println("ERRO:Comando desconhecido. Digite HELP");
    }
}

void printHelp() {
    Serial.println("╔══════════════════════════════════╗");
    Serial.println("║      ESPortable32 v1.1.0        ║");
    Serial.println("╚══════════════════════════════════╝");
    Serial.println("Comandos:");
    Serial.println("  HELP            - Mostra esta ajuda");
    Serial.println("  VERSION         - Versao do firmware");
    Serial.println("  STATUS          - Status do sistema");
    Serial.println("  WIFI=ssid,pass  - Configurar WiFi");
    Serial.println("  NAME=nome       - Nome do dispositivo");
    Serial.println("  PIN=1234        - Configurar PIN");
    Serial.println("  SCAN            - Escanear redes WiFi");
    Serial.println("  SAVE            - Salvar e reiniciar");
    Serial.println("  RESET           - Reset de fabrica");
    Serial.println("  BT=on|off       - Ligar/desligar Bluetooth");
}
