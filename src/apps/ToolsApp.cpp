#include "ToolsApp.h"
#include "../icons/Icons.h"
#include "../core/Utils.h"
#include "../core/GUI.h"

ToolsApp::ToolsApp(AppManager* mgr) : _mgr(mgr), _view(TOOL_MENU), _itemCount(0) {}

void ToolsApp::init() {
    _itemCount = 5;
    _items[0] = "Info ESP";
    _items[1] = "Aleatorio";
    _items[2] = "Monitor RAM";
    _items[3] = "Sleep";
    _items[4] = "Voltar";
    _icons[0] = ICON_INFO;
    _icons[1] = ICON_RANDOM;
    _icons[2] = ICON_RAM;
    _icons[3] = ICON_SLEEP;
    _icons[4] = ICON_BACK;
    _menu.setItems(_items, _itemCount);
    _menu.setIcons(_icons);
    _menu.setTitle("Apps");
    _menu.setTitleIcon(ICON_TOOLS);
    _menu.reset();
    _view = TOOL_MENU;
}

void ToolsApp::update() {}

void ToolsApp::draw(Display& d) {
    switch (_view) {
        case TOOL_MENU: _menu.draw(d); break;
        case TOOL_INFO: drawInfo(d); break;
        case TOOL_RANDOM: drawRandom(d); break;
        case TOOL_RAM: drawRam(d); break;
        case TOOL_SLEEP: doSleep(d); break;
    }
}

void ToolsApp::buttonClick() {
    if (_view == TOOL_MENU) _menu.next();
    else _view = TOOL_MENU;
}

void ToolsApp::buttonHold() {
    if (_view != TOOL_MENU) return;
    switch (_menu.select()) {
        case 0: _view = TOOL_INFO; break;
        case 1: _view = TOOL_RANDOM; break;
        case 2: _view = TOOL_RAM; break;
        case 3: doSleep(_mgr->display()); break;
        case 4: _mgr->popApp(); break;
    }
}

void ToolsApp::buttonVeryLong() { _view = TOOL_MENU; }
void ToolsApp::buttonDoubleClick() { _view = TOOL_MENU; }
void ToolsApp::exit() {}

void ToolsApp::drawInfo(Display& d) {
    d.clear();
    d.drawCenteredText(2, "ESP32 Info", 1);
    d.drawHLine(0, 11, 128);
    char buf[24];

    // metrics in card-like layout
    GUI::drawMetric(d, 4, 14, "Chip", "ESP32");
    snprintf(buf, sizeof(buf), "%uMB", ESP.getFlashChipSize() / (1024 * 1024));
    GUI::drawMetric(d, 64, 14, "Flash", buf);

    snprintf(buf, sizeof(buf), "%s", Utils::formatBytes(ESP.getFreeHeap()).c_str());
    GUI::drawMetric(d, 4, 32, "Heap", buf);

    snprintf(buf, sizeof(buf), "%u MHz", ESP.getCpuFreqMHz());
    GUI::drawMetric(d, 64, 32, "Clock", buf);

    snprintf(buf, sizeof(buf), "%s", Utils::formatUptime(millis()).c_str());
    d.drawCenteredText(52, buf, 1);

    d.drawCenteredText(60, "Hold=voltar", 1);
    d.show();
}

void ToolsApp::drawRandom(Display& d) {
    d.clear();
    d.drawCenteredText(2, "Aleatorio", 1);
    d.drawHLine(0, 11, 128);
    char buf[32];
    snprintf(buf, sizeof(buf), "%lu", esp_random());
    d.drawCenteredText(28, buf, 2);
    d.drawCenteredText(52, "Hold=voltar", 1);
    d.show();
    delay(200);
}

void ToolsApp::drawRam(Display& d) {
    d.clear();
    d.drawCenteredText(2, "RAM Monitor", 1);
    d.drawHLine(0, 11, 128);
    uint32_t total = ESP.getHeapSize();
    uint32_t free = ESP.getFreeHeap();
    uint32_t used = total - free;
    int pct = (total > 0) ? (used * 100 / total) : 0;

    // bar
    d.drawRoundFrame(8, 16, 112, 14, 3);
    int fillW = (108 * pct) / 100;
    if (fillW > 0) d.fillRoundFrame(10, 18, fillW, 10, 2, true);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    d.drawCenteredText(19, buf, 1);

    char usedS[16], freeS[16];
    snprintf(usedS, sizeof(usedS), "%s", Utils::formatBytes(used).c_str());
    snprintf(freeS, sizeof(freeS), "%s", Utils::formatBytes(free).c_str());
    GUI::drawMetric(d, 4, 36, "Usado", usedS);
    GUI::drawMetric(d, 64, 36, "Livre", freeS);

    d.drawCenteredText(58, "Hold=voltar", 1);
    d.show();
    delay(500);
}

void ToolsApp::doSleep(Display& d) {
    d.clear();
    d.drawCenteredText(28, "Sleep...", 1);
    d.show();
    delay(500);
    d.sleep();
    while (digitalRead(0) == HIGH) { delay(50); }
    d.wake();
    _view = TOOL_MENU;
}
