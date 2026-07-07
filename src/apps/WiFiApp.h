#ifndef APP_WIFI_H
#define APP_WIFI_H

#include "../core/AppManager.h"
#include "../core/Menu.h"
#include <WiFi.h>

class WiFiApp : public App {
public:
    WiFiApp(AppManager* mgr);
    void init() override;
    void update() override;
    void draw(Display& d) override;
    void buttonClick() override;
    void buttonHold() override;
    void buttonVeryLong() override;
    void buttonDoubleClick() override;
    void exit() override;
    const char* name() override { return "WiFi"; }

private:
    enum View { WIFI_MENU, WIFI_SCAN, WIFI_DETAILS, WIFI_SAVED, WIFI_INFO };
    AppManager* _mgr;
    Menu _menu;
    View _view;
    const char* _items[5];
    const uint8_t* _icons[5];

    struct NetInfo {
        String ssid;
        int rssi;
        int channel;
        String security;
        String bssid;
    };
    NetInfo _nets[30];
    int _netCount;
    int _netSel;

    void scan(Display& d);
    void drawScanResult(Display& d);
    void drawDetails(Display& d);
    void drawSaved(Display& d);
    void drawInfo(Display& d);
};

#endif
