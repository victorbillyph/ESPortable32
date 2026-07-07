#include "Home.h"
#include "../icons/Icons.h"
#include "../core/GUI.h"
#include "../core/State.h"

HomeApp::HomeApp(AppManager* mgr) : _mgr(mgr), _mode(WATCH_FACE),
    _itemCount(0), _bootTime(0), _steps(0), _stepGoal(10000), _lastTimeUpdate(0) {}

void HomeApp::init() {
    _bootTime = millis();
    _mode = WATCH_FACE;
    _timeStr[0] = '\0';
    _dateStr[0] = '\0';
    updateTime();

    _itemCount = 11;
    _items[0] = "Apps";
    _items[1] = "Wi-Fi";
    _items[2] = "Bluetooth";
    _items[3] = "Futebol";
    _items[4] = "Jogos";
    _items[5] = "GPIOs";
    _items[6] = "Musica";
    _items[7] = "Sistema";
    _items[8] = "Config";
    _items[9] = "Impressoras";
    _items[10] = "Relogio";
    _icons[0] = ICON_TOOLS;
    _icons[1] = ICON_WIFI;
    _icons[2] = ICON_BLUETOOTH;
    _icons[3] = ICON_GAME;
    _icons[4] = ICON_GAME;
    _icons[5] = ICON_CPU;
    _icons[6] = ICON_INFO;
    _icons[7] = ICON_SYSTEM;
    _icons[8] = ICON_SETTINGS;
    _icons[9] = ICON_PRINTER;
    _icons[10] = ICON_CLOCK;
    _menu.setItems(_items, _itemCount);
    _menu.setIcons(_icons);
    _menu.setTitle("Apps");
    _menu.setTitleIcon(ICON_HOME);
    _menu.reset();
}

void HomeApp::update() {
    if (millis() - _lastTimeUpdate > 1000) {
        _lastTimeUpdate = millis();
        updateTime();
    }
}

void HomeApp::draw(Display& d) {
    if (_mode == WATCH_FACE) drawWatchFace(d);
    else _menu.draw(d);
}

void HomeApp::buttonClick() {
    if (_mode == WATCH_FACE) {
        _steps += random(1, 5);
        if (_steps > _stepGoal) _steps = 0;
    } else {
        _menu.next();
    }
}

void HomeApp::buttonHold() {
    if (_mode == WATCH_FACE) {
        _mode = APP_LAUNCHER;
        _menu.reset();
    } else {
        int sel = _menu.select();
        int appIdx = sel + 1; // apps start at index 1
        if (appIdx < _mgr->appCount()) {
            _mode = WATCH_FACE;
            _mgr->pushApp(appIdx);
        }
    }
}

void HomeApp::buttonVeryLong() { _mode = WATCH_FACE; }
void HomeApp::buttonDoubleClick() { _mode = APP_LAUNCHER; _menu.reset(); }
void HomeApp::exit() { _mode = WATCH_FACE; }

void HomeApp::updateTime() {
    if (state.timeSynced) {
        snprintf(_timeStr, sizeof(_timeStr), "%02d:%02d", state.hour, state.minute);
        snprintf(_dateStr, sizeof(_dateStr), "%02d/%02d/%04d", state.day, state.month, state.year);
    } else {
        unsigned long ms = millis();
        unsigned long totalSec = ms / 1000;
        int h = (totalSec / 3600) % 24;
        int m = (totalSec % 3600) / 60;
        int s = totalSec % 60;
        snprintf(_timeStr, sizeof(_timeStr), "%02d:%02d", h, m);
        int days = totalSec / 86400;
        snprintf(_dateStr, sizeof(_dateStr), "Dia %d", days + 1);
    }
}

void HomeApp::drawWatchFace(Display& d) {
    d.clear();

    // top bar
    d.drawTopBar(_timeStr, state.wifiConnected, false, 85);

    // big clock
    d.oled().setTextSize(3);
    d.oled().setTextColor(SSD1306_WHITE);
    int16_t x1, y1; uint16_t w, h;
    d.oled().getTextBounds(_timeStr, 0, 0, &x1, &y1, &w, &h);
    d.oled().setCursor((SCREEN_W - w) / 2, 14);
    d.oled().print(_timeStr);

    // date
    d.oled().setTextSize(1);
    d.oled().setCursor((SCREEN_W - strlen(_dateStr) * 6) / 2, 36);
    d.oled().print(_dateStr);

    d.drawHLine(10, 44, SCREEN_W - 20);

    // weather or steps text
    char buf[24];
    if (state.weatherLoaded) {
        snprintf(buf, sizeof(buf), "%dC %s", state.weatherTemp, state.weatherDesc);
        d.oled().setCursor(2, 48);
        d.oled().setTextSize(1);
        d.oled().print(buf);
        // steps count on the right
        snprintf(buf, sizeof(buf), "%d", _steps);
        d.oled().setCursor(SCREEN_W - strlen(buf) * 6 - 2, 48);
        d.oled().print(buf);
    } else {
        snprintf(buf, sizeof(buf), "%d passos", _steps);
        d.drawCenteredText(48, buf, 1);
    }

    int stepPct = (_steps * 100) / _stepGoal;
    if (stepPct > 100) stepPct = 100;
    d.drawProgressBar(14, 57, SCREEN_W - 28, 5, stepPct);

    // bottom hint
    d.oled().setCursor(SCREEN_W - 30, 1);
    d.oled().setTextSize(1);
    d.oled().print("Hold");

    d.show();
}
