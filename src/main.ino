#include "core/Display.h"
#include "core/Input.h"
#include "core/AppManager.h"
#include "core/Config.h"
#include "core/State.h"
#include "core/ModuleManager.h"
#include "core/WebManager.h"
#include "core/PrinterManager.h"
#include "apps/Home.h"
#include "apps/ToolsApp.h"
#include "apps/WiFiApp.h"
#include "apps/BluetoothApp.h"
#include "apps/GamesApp.h"
#include "apps/SystemApp.h"
#include "apps/SettingsApp.h"
#include "apps/QuickSettings.h"
#include "apps/WiFiSetupApp.h"
#include "apps/PrintersApp.h"
#include "apps/FutbolStatusApp.h"
#include "apps/GPIOApp.h"
#include "apps/MusicApp.h"
#include "apps/RelogioApp.h"
#include "core/MediaBuffer.h"
#include "core/Speaker.h"
#include <WiFi.h>
#include <time.h>
#include <HTTPClient.h>

#define FIRMWARE "ESPortable32 v3.2.0"

Config config;
Display display;
Input input;
AppManager* appManager = nullptr;

HomeApp* homeApp = nullptr;
ToolsApp* toolsApp = nullptr;
WiFiApp* wifiApp = nullptr;
BluetoothApp* btApp = nullptr;
GamesApp* gamesApp = nullptr;
SystemApp* systemApp = nullptr;
SettingsApp* settingsApp = nullptr;
QuickSettingsApp* quickSettings = nullptr;
PrintersApp* printersApp = nullptr;
FutbolStatusApp* futbolApp = nullptr;
GPIOApp* gpioApp = nullptr;
MusicApp* musicApp = nullptr;
RelogioApp* relogioApp = nullptr;

ModuleManager moduleManager;
PrinterManager printerManager;
WebManager webManager(&config, &moduleManager, &printerManager);

SystemState state;

unsigned long lastActivity = 0;
const unsigned long SLEEP_TIMEOUT = 30000;
bool asleep = false;
bool aodMode = false;
unsigned long weatherFetchTime = 0;

void showSplash();
void onDoubleClick();
void enterAOD();
void exitAOD();
void runWiFiSetup();
void connectSavedWiFi();
void runSelfTest();
void syncNTP();
void fetchWeather();

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n" FIRMWARE);

    display.begin();
    input.begin();
    config.begin();

    runSelfTest();

    if (input.isHeldAtBoot()) {
        display.clear();
        display.drawCenteredText(20, "Recovery Mode", 1);
        display.drawCenteredText(36, "Limpar config?", 1);
        display.drawCenteredText(52, "Hold=sim Click=nao", 1);
        display.show();
        delay(2000);
        if (digitalRead(0) == LOW) {
            config.clear();
            display.drawCenteredText(28, "Config limpa!", 1);
            display.show();
            delay(1500);
            ESP.restart();
        }
    }

    if (!config.isConfigured()) {
        runWiFiSetup();
        return;
    }

    mediaInit();
    showSplash();
    Speaker::begin();
    Speaker::playStartup();
    connectSavedWiFi();

    moduleManager.begin();
    printerManager.begin();
    if (state.wifiConnected) webManager.begin();

    appManager = new AppManager(display);
    homeApp = new HomeApp(appManager);
    toolsApp = new ToolsApp(appManager);
    wifiApp = new WiFiApp(appManager);
    btApp = new BluetoothApp(appManager, &printerManager);
    futbolApp = new FutbolStatusApp(appManager);
    gamesApp = new GamesApp(appManager);
    systemApp = new SystemApp(appManager);
    settingsApp = new SettingsApp(appManager);
    quickSettings = new QuickSettingsApp(appManager);
    printersApp = new PrintersApp(appManager, &printerManager);
    gpioApp = new GPIOApp(appManager);
    musicApp = new MusicApp(appManager);
    relogioApp = new RelogioApp(appManager);

    appManager->addApp(homeApp);
    appManager->addApp(toolsApp);
    appManager->addApp(wifiApp);
    appManager->addApp(btApp);
    appManager->addApp(futbolApp);
    appManager->addApp(gamesApp);
    appManager->addApp(gpioApp);
    appManager->addApp(musicApp);
    appManager->addApp(systemApp);
    appManager->addApp(settingsApp);
    appManager->addApp(printersApp);
    appManager->addApp(relogioApp);

    appManager->begin();
    lastActivity = millis();
}

void loop() {
    ButtonEvent btn = input.update();

    moduleManager.update(millis());
    printerManager.update();
    webManager.update();

    if (btn != BTN_NONE) {
        lastActivity = millis();
        if (asleep) {
            display.wake();
            display.setContrast(255);
            asleep = false;
            aodMode = false;
            return;
        }
    }

    if (!asleep) {
        if (btn == BTN_DOUBLE_CLICK && appManager->currentApp() != 9) {
            onDoubleClick();
        } else {
            appManager->update(btn);
        }
        appManager->draw();
        if (!aodMode) {
            moduleManager.drawOverlay(display);
            display.show();
        }
    }

    if (!asleep && !aodMode && (millis() - lastActivity > 15000)) {
        enterAOD();
    }
    if (aodMode && (millis() - lastActivity > SLEEP_TIMEOUT)) {
        display.sleep();
        asleep = true;
        aodMode = false;
    }

    // async weather fetch after boot + WiFi
    if (state.wifiConnected && !state.weatherLoaded && weatherFetchTime == 0 && millis() > 15000) {
        weatherFetchTime = millis();
        fetchWeather();
    }

    // periodic NTP time update
    if (state.timeSynced) {
        time_t now;
        time(&now);
        if (now > 100000) {
            struct tm* ti = localtime(&now);
            state.hour = ti->tm_hour;
            state.minute = ti->tm_min;
            state.second = ti->tm_sec;
            state.day = ti->tm_mday;
            state.month = ti->tm_mon + 1;
            state.year = ti->tm_year + 1900;
            state.wday = ti->tm_wday;
        }
        relogioApp->tickAlarm(state.hour, state.minute, state.wday);
        if (relogioApp->isAlarming() && appManager->currentApp() != 11) {
            appManager->pushApp(11);
        }
    }
}

// ── WiFi Setup ───────────────────────────────────────────

void runWiFiSetup() {
    WiFiSetupApp setupApp(&config);
    setupApp.init();

    unsigned long t = 0;
    while (!setupApp.isDone()) {
        ButtonEvent btn = input.update();
        if (btn == BTN_CLICK) setupApp.buttonClick();
        setupApp.update();
        if (millis() - t > 100) {
            t = millis();
            setupApp.draw(display);
        }
        delay(10);
    }
}

void connectSavedWiFi() {
    String ssid = config.getSSID();
    String pass = config.getPass();
    if (ssid.length() == 0) return;

    display.clear();
    display.drawCenteredText(16, "Conectando WiFi...", 1);
    display.show();

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    for (int i = 0; i < 30; i++) {
        if (WiFi.status() == WL_CONNECTED) {
            state.wifiConnected = true;
            Serial.printf("[WiFi] OK: %s\n", WiFi.localIP().toString().c_str());
            display.drawCenteredText(28, "Conectado!", 1);
            display.show();
            delay(500);
            syncNTP();
            return;
        }
        delay(500);
    }
    Serial.println("[WiFi] Falhou, continuando sem WiFi");
}

// ── NTP ──────────────────────────────────────────────────

void syncNTP() {
    if (!state.wifiConnected) return;

    display.drawCenteredText(40, "Sincronizando NTP...", 1);
    display.show();

    configTime(-3 * 3600, 0, "pool.ntp.org", "time.google.com");

    time_t now = 0;
    for (int i = 0; i < 30; i++) {
        time(&now);
        if (now > 100000) break;
        delay(500);
    }

    if (now > 100000) {
        struct tm* ti = localtime(&now);
        state.hour = ti->tm_hour;
        state.minute = ti->tm_min;
        state.second = ti->tm_sec;
        state.day = ti->tm_mday;
        state.month = ti->tm_mon + 1;
        state.year = ti->tm_year + 1900;
        state.timeSynced = true;
        Serial.printf("[NTP] OK: %04d-%02d-%02d %02d:%02d:%02d\n",
            state.year, state.month, state.day, state.hour, state.minute, state.second);
    } else {
        Serial.println("[NTP] Timeout");
    }
}

// ── Weather ──────────────────────────────────────────────

void fetchWeather() {
    if (!state.wifiConnected) return;

    Serial.println("[Weather] Fetching...");
    HTTPClient http;
    http.setTimeout(5000);
    http.begin("http://wttr.in?format=%t+%C");
    int code = http.GET();
    if (code == 200) {
        String resp = http.getString();
        resp.trim();
        int degIdx = resp.indexOf("°C");
        if (degIdx > 0) {
            String tempStr = resp.substring(0, degIdx + 2);
            tempStr.replace("+", "");
            state.weatherTemp = tempStr.toInt();
            String desc = resp.substring(degIdx + 2);
            desc.trim();
            strncpy(state.weatherDesc, desc.c_str(), sizeof(state.weatherDesc) - 1);
            state.weatherDesc[sizeof(state.weatherDesc) - 1] = '\0';
            state.weatherLoaded = true;
            Serial.printf("[Weather] %d°C %s\n", state.weatherTemp, state.weatherDesc);
        }
    } else {
        Serial.printf("[Weather] HTTP error: %d\n", code);
    }
    http.end();
}

// ── Self-Test ────────────────────────────────────────────

void runSelfTest() {
    const int NUM = 6;
    const char* names[NUM] = {"OLED", "Botao", "WiFi", "Config", "Heap", "I2C"};
    bool results[NUM];

    display.clear();
    display.drawCenteredText(2, "Self-Test", 1);
    display.drawHLine(0, 11, 128);

    results[0] = true;
    results[1] = (digitalRead(0) == HIGH || digitalRead(0) == LOW);
    WiFi.mode(WIFI_STA);
    results[2] = (WiFi.getMode() == WIFI_STA);
    WiFi.mode(WIFI_OFF);
    results[3] = true;
    results[4] = (ESP.getFreeHeap() > 10000);
    Wire.beginTransmission(0x3C);
    results[5] = (Wire.endTransmission() == 0);

    bool allOk = true;
    for (int i = 0; i < NUM; i++) {
        if (!results[i]) allOk = false;
        display.oled().setCursor(2, 14 + i * 8);
        display.oled().setTextSize(1);
        display.oled().setTextColor(SSD1306_WHITE);
        display.oled().print(results[i] ? "[OK]" : "[!!]");
        display.oled().setCursor(30, 14 + i * 8);
        display.oled().print(names[i]);
    }

    int y = 14 + NUM * 8 + 2;
    display.drawHLine(0, y, 128);
    display.drawCenteredText(y + 4, allOk ? "Sistema OK" : "Falhas detectadas!", 1);
    display.show();
    delay(2000);
}

// ── Splash ───────────────────────────────────────────────

void showSplash() {
    display.clear();
    for (int i = 0; i <= 100; i += 5) {
        display.clear();
        display.setContrast(i * 255 / 100);
        display.drawCenteredText(16, "ESPortable32", 2);
        display.show();
        delay(15);
    }
    display.setContrast(255);
    delay(600);
}

// ── Quick Settings ──────────────────────────────────────

void onDoubleClick() {
    quickSettings->init();
    quickSettings->draw(display);
    unsigned long timeout = millis() + 5000;
    while (millis() < timeout) {
        ButtonEvent e = input.update();
        if (e == BTN_CLICK) { quickSettings->buttonClick(); quickSettings->draw(display); }
        if (e == BTN_HOLD) { quickSettings->buttonHold(); quickSettings->draw(display); }
        if (e == BTN_VERY_LONG) break;
        delay(50);
    }
    lastActivity = millis();
}

void enterAOD() {
    aodMode = true;
    display.setContrast(20);
    display.clear();
    if (state.timeSynced) {
        char buf[6];
        snprintf(buf, sizeof(buf), "%02d:%02d", state.hour, state.minute);
        display.drawCenteredText(28, buf, 2);
    } else {
        display.drawCenteredText(28, ":", 2);
    }
    display.show();
}

void exitAOD() {
    aodMode = false;
    display.setContrast(255);
    display.wake();
}
