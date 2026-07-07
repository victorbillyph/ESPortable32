#ifndef APP_WIFI_SETUP_H
#define APP_WIFI_SETUP_H

#include "../core/AppManager.h"
#include "../core/Display.h"
#include "../core/Config.h"
#include <WiFi.h>
#include <WebServer.h>

class WiFiSetupApp : public App {
public:
    WiFiSetupApp(Config* config);
    void init() override;
    void update() override;
    void draw(Display& d) override;
    void buttonClick() override;
    void buttonHold() override {}
    void buttonVeryLong() override {}
    void buttonDoubleClick() override {}
    void exit() override;
    const char* name() override { return "WiFiSetup"; }

    bool isDone() { return _done; }

private:
    enum Phase {
        PHASE_BRAND,
        PHASE_PRESS_START,
        PHASE_WIFI_CONNECT,
        PHASE_SHOW_IP,
        PHASE_DONE
    };

    Config* _config;
    WebServer* _server;
    bool _apStarted;
    bool _done;
    Phase _phase;
    unsigned long _phaseStart;
    bool _waitingBoot;

    // brand animation
    int _animFrame;
    unsigned long _lastAnim;

    String _scanResult;
    unsigned long _scanTime;

    void startAP();
    void advancePhase();
    void drawBrand(Display& d);
    void drawPressStart(Display& d);
    void drawWifiConnect(Display& d);
    void drawShowIP(Display& d);

    // web
    void handleRoot();
    void handleSave();
    String htmlPage();
    String buildOptions();
};

#endif
