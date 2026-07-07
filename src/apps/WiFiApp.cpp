#include "WiFiApp.h"
#include "../icons/Icons.h"
#include "../core/Utils.h"
#include "../core/GUI.h"

WiFiApp::WiFiApp(AppManager* mgr) : _mgr(mgr), _view(WIFI_MENU),
    _netCount(0), _netSel(0) {}

void WiFiApp::init() {
    _items[0] = "Scan Redes";
    _items[1] = "Redes Salvas";
    _items[2] = "Info WiFi";
    _items[3] = "Voltar";
    _icons[0] = ICON_SIGNAL;
    _icons[1] = ICON_FOLDER;
    _icons[2] = ICON_INFO;
    _icons[3] = ICON_BACK;
    _menu.setItems(_items, 4);
    _menu.setIcons(_icons);
    _menu.setTitle("Wi-Fi");
    _menu.setTitleIcon(ICON_WIFI);
    _menu.reset();
    _view = WIFI_MENU;
}

void WiFiApp::update() {}

void WiFiApp::draw(Display& d) {
    switch (_view) {
        case WIFI_MENU: _menu.draw(d); break;
        case WIFI_SCAN: drawScanResult(d); break;
        case WIFI_DETAILS: drawDetails(d); break;
        case WIFI_SAVED: drawSaved(d); break;
        case WIFI_INFO: drawInfo(d); break;
    }
}

void WiFiApp::buttonClick() {
    switch (_view) {
        case WIFI_MENU: _menu.next(); break;
        case WIFI_SCAN:
            _netSel = (_netSel + 1) % (_netCount > 0 ? _netCount : 1);
            break;
        default: _view = WIFI_MENU; break;
    }
}

void WiFiApp::buttonHold() {
    switch (_view) {
        case WIFI_MENU:
            switch (_menu.select()) {
                case 0: scan(_mgr->display()); break;
                case 1: _view = WIFI_SAVED; break;
                case 2: _view = WIFI_INFO; break;
                case 3: _mgr->popApp(); break;
            }
            break;
        case WIFI_SCAN:
            if (_netCount > 0) _view = WIFI_DETAILS;
            break;
        default:
            _view = WIFI_MENU;
            break;
    }
}

void WiFiApp::buttonVeryLong() { _view = WIFI_MENU; }
void WiFiApp::buttonDoubleClick() { _view = WIFI_MENU; }

void WiFiApp::exit() {}

void WiFiApp::scan(Display& d) {
    d.clear();
    d.drawCenteredText(28, "Escaneando...", 1);
    d.show();
    _netCount = WiFi.scanNetworks();
    if (_netCount > 30) _netCount = 30;
    for (int i = 0; i < _netCount; i++) {
        _nets[i].ssid = WiFi.SSID(i);
        _nets[i].rssi = WiFi.RSSI(i);
        _nets[i].channel = WiFi.channel(i);
        _nets[i].bssid = WiFi.BSSIDstr(i);
        switch (WiFi.encryptionType(i)) {
            case WIFI_AUTH_OPEN: _nets[i].security = "Open"; break;
            case WIFI_AUTH_WEP: _nets[i].security = "WEP"; break;
            case WIFI_AUTH_WPA_PSK: _nets[i].security = "WPA"; break;
            case WIFI_AUTH_WPA2_PSK: _nets[i].security = "WPA2"; break;
            case WIFI_AUTH_WPA_WPA2_PSK: _nets[i].security = "WPA/WPA2"; break;
            default: _nets[i].security = "???"; break;
        }
    }
    _netSel = 0;
    _view = WIFI_SCAN;
}

void WiFiApp::drawScanResult(Display& d) {
    d.clear();
    GUI::drawMenuTitle(d, "Redes", ICON_SIGNAL);
    int visible = 4;
    int start = 0;
    if (_netSel >= visible) start = _netSel - visible + 1;
    if (start < 0) start = 0;
    int y = 12;
    char buf[22];
    for (int i = start; i < _netCount && i < start + visible; i++) {
        if (i == _netSel) {
            d.oled().fillRect(0, y, SCREEN_W, 12, SSD1306_WHITE);
            d.oled().setTextColor(SSD1306_BLACK);
        } else {
            d.oled().setTextColor(SSD1306_WHITE);
        }
        String s = _nets[i].ssid;
        if (s.length() > 14) s = s.substring(0, 13) + "~";
        snprintf(buf, sizeof(buf), " %s [%d]", s.c_str(), _nets[i].rssi);
        d.oled().setCursor(1, y + 2);
        d.oled().print(buf);
        d.oled().setTextColor(SSD1306_WHITE);
        y += 12;
    }
    if (_netCount == 0) {
        d.oled().setCursor(1, y);
        d.oled().print(" Nenhuma rede");
    }
    d.show();
}

void WiFiApp::drawDetails(Display& d) {
    d.clear();
    GUI::drawMenuTitle(d, "Detalhes", ICON_SIGNAL);
    if (_netSel >= _netCount) { _view = WIFI_SCAN; return; }
    NetInfo& n = _nets[_netSel];
    char buf[24];
    int y = 12;
    snprintf(buf, sizeof(buf), "SSID: %s", n.ssid.c_str());
    d.drawText(1, y, buf, 1); y += 9;
    snprintf(buf, sizeof(buf), "RSSI: %d (%s)", n.rssi, Utils::formatRSSI(n.rssi).c_str());
    d.drawText(1, y, buf, 1); y += 9;
    snprintf(buf, sizeof(buf), "Chan: %d", n.channel);
    d.drawText(1, y, buf, 1); y += 9;
    snprintf(buf, sizeof(buf), "Seg: %s", n.security.c_str());
    d.drawText(1, y, buf, 1); y += 9;
    snprintf(buf, sizeof(buf), "BSSID: %s", n.bssid.c_str());
    d.drawText(1, y, buf, 1);
    d.drawCenteredText(56, "Segure p/ voltar", 1);
    d.show();
}

void WiFiApp::drawSaved(Display& d) {
    d.clear();
    GUI::drawMenuTitle(d, "Redes Salvas", ICON_FOLDER);
    d.drawCenteredText(28, "(nenhuma salva)", 1);
    d.drawCenteredText(52, "Segure p/ voltar", 1);
    d.show();
}

void WiFiApp::drawInfo(Display& d) {
    d.clear();
    GUI::drawMenuTitle(d, "Info WiFi", ICON_INFO);
    char buf[24];
    int y = 12;
    snprintf(buf, sizeof(buf), "Status: %s",
        WiFi.status() == WL_CONNECTED ? "Conectado" : "---");
    d.drawText(1, y, buf, 1); y += 9;
    if (WiFi.status() == WL_CONNECTED) {
        snprintf(buf, sizeof(buf), "SSID: %s", WiFi.SSID().c_str());
        d.drawText(1, y, buf, 1); y += 9;
        snprintf(buf, sizeof(buf), "IP: %s", WiFi.localIP().toString().c_str());
        d.drawText(1, y, buf, 1); y += 9;
        snprintf(buf, sizeof(buf), "RSSI: %d", WiFi.RSSI());
        d.drawText(1, y, buf, 1); y += 9;
    }
    d.drawCenteredText(56, "Segure p/ voltar", 1);
    d.show();
}
