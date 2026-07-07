#include "GPIOApp.h"
#include "../icons/Icons.h"
#include "../core/GUI.h"

static const int GPIO_LIST[17] = {2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 23, 25, 26, 27, 32, 33};

GPIOApp::GPIOApp(AppManager* mgr) : _mgr(mgr), _mode(MENU), _sel(0) {}

void GPIOApp::init() {
    _items[0] = "Controlar GPIOs";
    _items[1] = "Voltar";
    _icons[0] = ICON_CPU;
    _icons[1] = ICON_BACK;
    _menu.setItems(_items, 2);
    _menu.setIcons(_icons);
    _menu.setTitle("GPIOs");
    _menu.setTitleIcon(ICON_CPU);
    _menu.reset();
    _mode = MENU;
    initPins();
}

void GPIOApp::initPins() {
    for (int i = 0; i < PIN_COUNT; i++) {
        _pins[i].pin = GPIO_LIST[i];
        pinMode(_pins[i].pin, INPUT);
        _pins[i].mode = INPUT;
        _pins[i].value = digitalRead(_pins[i].pin);
    }
}

void GPIOApp::update() {
    if (_mode == LIST) {
        for (int i = 0; i < PIN_COUNT; i++) {
            if (_pins[i].mode == INPUT) {
                _pins[i].value = digitalRead(_pins[i].pin);
            }
        }
    }
}

void GPIOApp::draw(Display& d) {
    switch (_mode) {
        case MENU: _menu.draw(d); break;
        case LIST: drawList(d); break;
    }
}

void GPIOApp::buttonClick() {
    switch (_mode) {
        case MENU: _menu.next(); break;
        case LIST: _sel = (_sel + 1) % PIN_COUNT; break;
    }
}

void GPIOApp::buttonHold() {
    switch (_mode) {
        case MENU:
            switch (_menu.select()) {
                case 0: _mode = LIST; _sel = 0; break;
                case 1: _mgr->popApp(); break;
            }
            break;
        case LIST: togglePin(_sel); break;
    }
}

void GPIOApp::buttonVeryLong() { _mode = MENU; }
void GPIOApp::buttonDoubleClick() { _mode = MENU; }
void GPIOApp::exit() {}

void GPIOApp::togglePin(int idx) {
    if (idx < 0 || idx >= PIN_COUNT) return;
    GPIOPin& p = _pins[idx];

    if (p.mode == INPUT) {
        p.mode = OUTPUT;
        p.value = LOW;
        pinMode(p.pin, OUTPUT);
        digitalWrite(p.pin, LOW);
    } else if (p.value == LOW) {
        p.value = HIGH;
        digitalWrite(p.pin, HIGH);
    } else {
        p.mode = INPUT;
        p.value = digitalRead(p.pin);
        pinMode(p.pin, INPUT);
    }
}

void GPIOApp::drawList(Display& d) {
    d.clear();
    GUI::drawMenuTitle(d, "GPIOs", ICON_CPU);

    int y = 14;
    int maxShow = 5;
    int s = _sel < maxShow ? 0 : _sel - maxShow + 1;
    int e = min(s + maxShow, PIN_COUNT);

    for (int i = s; i < e; i++) {
        char buf[20];
        GPIOPin& p = _pins[i];
        char prefix = (i == _sel) ? '>' : ' ';
        const char* modeStr = (p.mode == OUTPUT) ? "OUT" : "IN ";
        snprintf(buf, sizeof(buf), "%cGPIO%-2d %s %d", prefix, p.pin, modeStr, p.value);
        d.drawText(1, y, buf, 1);
        y += 10;
    }

    d.drawCenteredText(57, "Hold=toggle", 1);
    d.show();
}
