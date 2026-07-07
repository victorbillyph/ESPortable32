#include "QuickSettings.h"
#include "../icons/Icons.h"
#include "../core/GUI.h"
#include <WiFi.h>

QuickSettingsApp::QuickSettingsApp(AppManager* mgr) : _mgr(mgr), _sel(0),
    _wifiOn(true), _btOn(true), _flashOn(false), _brightness(128) {}

void QuickSettingsApp::init() {
    _wifiOn = (WiFi.status() == WL_CONNECTED);
    _flashOn = false;
    _sel = 0;
    pinMode(32, OUTPUT);
    pinMode(33, OUTPUT);
    digitalWrite(32, LOW);
    digitalWrite(33, LOW);
}

void QuickSettingsApp::update() {}

void QuickSettingsApp::draw(Display& d) {
    d.clear();

    // semi-transparent overlay effect
    d.fillFrame(0, 0, SCREEN_W, SCREEN_H, false);

    // title
    d.drawCenteredText(2, "Quick Settings", 1);
    d.drawHLine(0, 11, SCREEN_W);

    // WiFi toggle
    GUI::drawCardMini(d, 14, SCREEN_W, "Wi-Fi", _wifiOn ? "ON" : "OFF",
                      _sel == 0, ICON_WIFI);

    // BT toggle
    GUI::drawCardMini(d, 27, SCREEN_W, "Bluetooth", _btOn ? "ON" : "OFF",
                      _sel == 1, ICON_BLUETOOTH);

    // Flashlight toggle
    GUI::drawCardMini(d, 40, SCREEN_W, "Lanterna", _flashOn ? "ON" : "OFF",
                      _sel == 2, ICON_INFO);

    // back hint
    d.drawCenteredText(57, "Hold=voltar", 1);
    d.show();
}

void QuickSettingsApp::buttonClick() {
    _sel = (_sel + 1) % 4;
}

void QuickSettingsApp::buttonHold() {
    if (_sel == 0) {
        _wifiOn = !_wifiOn;
        if (_wifiOn) WiFi.mode(WIFI_STA);
        else WiFi.mode(WIFI_OFF);
    } else if (_sel == 1) {
        _btOn = !_btOn;
    } else if (_sel == 2) {
        _flashOn = !_flashOn;
        digitalWrite(33, _flashOn ? HIGH : LOW);
        digitalWrite(32, LOW);
    } else {
        _mgr->popApp();
    }
}

void QuickSettingsApp::buttonVeryLong() { _mgr->popApp(); }
void QuickSettingsApp::buttonDoubleClick() {}
void QuickSettingsApp::exit() {
    _flashOn = false;
    digitalWrite(33, LOW);
}
