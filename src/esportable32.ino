#include <WiFi.h>
#include "ConfigManager.h"
#include "BluetoothService.h"
#include "Display.h"

#define FIRMWARE_VERSION "2.0.0"
#define BTN_PIN      0
#define BTN_DEBOUNCE 50
#define LONG_PRESS_MS 400

ConfigManager config;
BluetoothService bluetooth;
Display display;

unsigned long lastSerialCheck = 0;
bool setupModeEnabled = false;

// OLED menu state
int menuSelection = 0;
int currentScreen = 0;
int appPage = 0;
unsigned long lastBtnState = HIGH;
unsigned long btnPressTime = 0;
bool btnLongPressHandled = false;

enum Screen { SCR_MENU = 0, SCR_DASHBOARD, SCR_GPIO, SCR_SETTINGS, SCR_INFO, SCR_WIFI, SCR_COUNT };

const char* menuItems[] = {
    "Dashboard",
    "GPIO",
    "Settings",
    "Info",
    "WiFi"
};
const int menuCount = sizeof(menuItems) / sizeof(menuItems[0]);

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n=== ESPortable32 v2.0.0 ===");

    pinMode(BTN_PIN, INPUT_PULLUP);
    lastBtnState = digitalRead(BTN_PIN);

    display.begin();
    display.splash("ESPortable32", FIRMWARE_VERSION);
    delay(1500);

    config.begin();

    if (config.getWifiSSID().length() == 0) {
        Serial.println("[WiFi] No SSID configured, setup mode");
        setupModeEnabled = true;
        stateSetup();
    } else {
        connectWiFi();
    }

    renderMenu();
}

void loop() {
    bluetooth.update();

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

    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 5000) {
        lastHeartbeat = millis();
        String stateName = WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected";
        Serial.printf("[Heartbeat] WiFi: %s, Heap: %u, RSSI: %d\n",
            stateName.c_str(), ESP.getFreeHeap(), WiFi.RSSI());
    }

    handleButton();
    updateScreen();
}

void stateSetup() {
    setupModeEnabled = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESPortable32-Setup", "configurar");
    bluetooth.begin("ESPortable32");
    Serial.println("[Setup] AP: ESPortable32-Setup / senha: configurar");
}

void connectWiFi() {
    String ssid = config.getWifiSSID();
    String pass = config.getWifiPass();

    if (ssid.length() == 0) {
        stateSetup();
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
        bluetooth.begin(config.getDeviceName());
        display.showMessage("WiFi OK!");
        delay(500);
    } else {
        Serial.println("\n[WiFi] Failed, setup mode");
        stateSetup();
    }
}

// ── Button ──────────────────────────────────────────────────────

void handleButton() {
    bool reading = digitalRead(BTN_PIN);
    if (reading != lastBtnState) {
        lastBtnState = reading;
        if (reading == LOW) {
            btnPressTime = millis();
            btnLongPressHandled = false;
        } else {
            unsigned long elapsed = millis() - btnPressTime;
            if (elapsed < LONG_PRESS_MS) shortPress();
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
        appPage++;
        renderScreen();
    }
}

// ── OLED rendering ──────────────────────────────────────────────

void renderMenu() {
    display.showMenu("ESPortable32", menuItems, menuCount, menuSelection);
}

void renderScreen() {
    switch (currentScreen) {
        case SCR_DASHBOARD: renderDashboard(); break;
        case SCR_GPIO:      renderGPIO(); break;
        case SCR_SETTINGS:  renderSettings(); break;
        case SCR_INFO:      renderInfo(); break;
        case SCR_WIFI:      renderWiFi(); break;
        default: currentScreen = SCR_MENU; renderMenu();
    }
}

void updateScreen() {
    static unsigned long last = 0;
    if (currentScreen == SCR_DASHBOARD && millis() - last > 2000) {
        last = millis();
        renderDashboard();
    }
}

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
        snprintf(buf, sizeof(buf), "GPIO%02d: %s", p, digitalRead(p) ? "HIGH" : "LOW ");
        display.drawText(1, y, buf, 1);
        y += 9;
    }
    char page[8];
    snprintf(page, sizeof(page), "%d/%d", appPage + 1, (n + perPage - 1) / perPage);
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
    snprintf(buf, sizeof(buf), "WiFi: %s", config.getWifiSSID().length() > 0 ? config.getWifiSSID().c_str() : "(none)");
    display.drawText(1, y, buf, 1); y += 10;
    snprintf(buf, sizeof(buf), "IP: %s", WiFi.localIP().toString().c_str());
    display.drawText(1, y, buf, 1); y += 10;
    snprintf(buf, sizeof(buf), "PIN: %s", config.getPin().length() > 0 ? "****" : "(none)");
    display.drawText(1, y, buf, 1);
    display.drawStatusBar("Pressione p/ voltar", "Segure=reiniciar");
    display.show();
}

void renderInfo() {
    display.clear();
    display.showCentered(4, "ESPortable32", 1);
    char buf[22];
    snprintf(buf, sizeof(buf), "Firmware: %s", FIRMWARE_VERSION);
    display.showCentered(16, buf, 1);
    display.showCentered(28, "Chip: ESP32", 1);
    display.showCentered(40, "Display: 128x64 OLED", 1);
    display.drawStatusBar("Pressione p/ voltar", nullptr);
    display.show();
}

void renderWiFi() {
    display.clear();
    display.drawTitle("WiFi");
    int y = 12;
    char buf[22];
    if (WiFi.status() == WL_CONNECTED) {
        snprintf(buf, sizeof(buf), "SSID: %s", WiFi.SSID().c_str());
        display.drawText(1, y, buf, 1); y += 10;
        snprintf(buf, sizeof(buf), "IP: %s", WiFi.localIP().toString().c_str());
        display.drawText(1, y, buf, 1); y += 10;
        snprintf(buf, sizeof(buf), "RSSI: %d dBm", WiFi.RSSI());
        display.drawText(1, y, buf, 1);
    } else {
        display.drawText(1, y, "Desconectado", 1); y += 10;
        if (config.getWifiSSID().length() > 0) {
            snprintf(buf, sizeof(buf), "SSID: %s", config.getWifiSSID().c_str());
            display.drawText(1, y, buf, 1);
        } else {
            display.drawText(1, y, "Sem WiFi config.", 1);
        }
    }
    display.drawStatusBar("Pressione p/ voltar", nullptr);
    display.show();
}

// ── Serial commands ─────────────────────────────────────────────

void processSerialCommand(String cmd) {
    if (cmd.length() == 0) return;
    Serial.printf("> %s\n", cmd.c_str());

    if (cmd == "HELP") {
        printHelp();
    } else if (cmd == "VERSION") {
        Serial.printf("FIRMWARE_VERSION=%s\n", FIRMWARE_VERSION);
    } else if (cmd == "STATUS") {
        Serial.printf("WiFi: %s\n", WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
        Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("RSSI: %d\n", WiFi.RSSI());
        Serial.printf("Heap: %u bytes\n", ESP.getFreeHeap());
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
        config.setPin(cmd.substring(4));
        Serial.println("OK:PIN configurado");
    } else if (cmd == "SAVE") {
        config.save();
        Serial.println("OK:Salvo! Reiniciando...");
        delay(500);
        ESP.restart();
    } else if (cmd == "RESET") {
        config.clear();
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
    } else if (cmd == "RESTART") {
        Serial.println("OK:Reiniciando...");
        delay(100);
        ESP.restart();
    } else {
        Serial.println("ERRO:Comando desconhecido. Digite HELP");
    }
}

void printHelp() {
    Serial.println("╔══════════════════════════════════╗");
    Serial.println("║      ESPortable32 v2.0.0        ║");
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
    Serial.println("  RESTART         - Reiniciar");
    Serial.println("  BT=on|off       - Ligar/desligar Bluetooth");
}
