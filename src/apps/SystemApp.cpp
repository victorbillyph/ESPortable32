#include "SystemApp.h"
#include "../icons/Icons.h"
#include "../core/Utils.h"

SystemApp::SystemApp(AppManager* mgr) : _mgr(mgr), _view(SYS_MENU) {}

void SystemApp::init() {
    _items[0] = "Reiniciar";
    _items[1] = "Sobre";
    _items[2] = "Info Sistema";
    _items[3] = "Voltar";
    _icons[0] = ICON_ERROR;
    _icons[1] = ICON_INFO;
    _icons[2] = ICON_SYSTEM;
    _icons[3] = ICON_BACK;
    _menu.setItems(_items, 4);
    _menu.setIcons(_icons);
    _menu.setTitle("Sistema");
    _menu.setTitleIcon(ICON_SYSTEM);
    _menu.reset();
    _view = SYS_MENU;
}

void SystemApp::update() {}

void SystemApp::draw(Display& d) {
    switch (_view) {
        case SYS_MENU: _menu.draw(d); break;
        case SYS_ABOUT: drawAbout(d); break;
        case SYS_INFO: drawSysInfo(d); break;
    }
}

void SystemApp::buttonClick() {
    if (_view == SYS_MENU) _menu.next();
    else _view = SYS_MENU;
}

void SystemApp::buttonHold() {
    if (_view == SYS_MENU) {
        switch (_menu.select()) {
            case 0: // reboot
                _mgr->display().clear();
                _mgr->display().drawCenteredText(28, "Reiniciando...", 1);
                _mgr->display().show();
                delay(500);
                ESP.restart();
                break;
            case 1: _view = SYS_ABOUT; break;
            case 2: _view = SYS_INFO; break;
            case 3: _mgr->popApp(); break;
        }
    } else _view = SYS_MENU;
}

void SystemApp::buttonVeryLong() { _view = SYS_MENU; }
void SystemApp::buttonDoubleClick() { _view = SYS_MENU; }
void SystemApp::exit() {}

void SystemApp::drawAbout(Display& d) {
    d.clear();
    d.oled().setTextSize(1);
    d.oled().setTextColor(SSD1306_WHITE);
    d.drawCenteredText(4, "ESPortable32", 1);
    d.drawHLine(0, 13, 128);
    d.drawCenteredText(18, "Versao 3.0.0", 1);
    d.drawCenteredText(28, "Autor: ESP32 Dev", 1);
    d.drawCenteredText(38, "Plataforma: ESP32", 1);
    d.drawCenteredText(48, "OLED SSD1306 128x64", 1);
    d.drawCenteredText(57, "Segure p/ voltar", 1);
    d.show();
}

void SystemApp::drawSysInfo(Display& d) {
    d.clear();
    d.drawCenteredText(2, "System Info", 1);
    d.drawHLine(0, 11, 128);
    char buf[32];
    int y = 14;
    snprintf(buf, sizeof(buf), "SDK: %s", ESP.getSdkVersion());
    d.drawText(1, y, buf, 1); y += 9;
    snprintf(buf, sizeof(buf), "Rev: %d", ESP.getChipRevision());
    d.drawText(1, y, buf, 1); y += 9;
    snprintf(buf, sizeof(buf), "Clock: %u MHz", ESP.getCpuFreqMHz());
    d.drawText(1, y, buf, 1); y += 9;
    snprintf(buf, sizeof(buf), "Flash: %uMB", ESP.getFlashChipSize() / (1024 * 1024));
    d.drawText(1, y, buf, 1); y += 9;
    snprintf(buf, sizeof(buf), "Heap: %s", Utils::formatBytes(ESP.getFreeHeap()).c_str());
    d.drawText(1, y, buf, 1); y += 9;
    snprintf(buf, sizeof(buf), "Uptime: %s", Utils::formatUptime(millis()).c_str());
    d.drawText(1, y, buf, 1);
    d.drawCenteredText(59, "Segure p/ voltar", 1);
    d.show();
}
