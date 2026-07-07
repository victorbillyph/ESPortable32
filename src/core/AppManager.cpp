#include "AppManager.h"

AppManager::AppManager(Display& display) : _display(display),
    _appCount(0), _current(0), _stackDepth(0), _initialized(false) {
    memset(_apps, 0, sizeof(_apps));
    memset(_stack, 0, sizeof(_stack));
}

void AppManager::addApp(App* app) {
    if (_appCount >= MAX_APPS) return;
    _apps[_appCount++] = app;
}

void AppManager::begin() {
    if (_appCount > 0) {
        _initialized = true;
        _current = 0;
        _apps[0]->init();
    }
}

void AppManager::update(ButtonEvent btn) {
    if (_appCount == 0 || !_initialized) return;
    App* app = _apps[_current];
    app->update();
    switch (btn) {
        case BTN_CLICK: app->buttonClick(); break;
        case BTN_HOLD: app->buttonHold(); break;
        case BTN_VERY_LONG: app->buttonVeryLong(); break;
        case BTN_DOUBLE_CLICK: app->buttonDoubleClick(); break;
        default: break;
    }
}

void AppManager::draw() {
    if (_appCount == 0 || !_initialized) return;
    _apps[_current]->draw(_display);
}

void AppManager::switchTo(int idx) {
    if (idx < 0 || idx >= _appCount) return;
    if (_initialized) _apps[_current]->exit();
    _current = idx;
    _apps[idx]->init();
}

void AppManager::goBack() {
    if (_stackDepth > 0) {
        if (_initialized) _apps[_current]->exit();
        _current = _stack[--_stackDepth];
        _apps[_current]->init();
    }
}

void AppManager::pushApp(int idx) {
    if (idx < 0 || idx >= _appCount) return;
    if (_stackDepth < MAX_STACK) {
        _stack[_stackDepth++] = _current;
    }
    if (_initialized) _apps[_current]->exit();
    _current = idx;
    _apps[idx]->init();
}

void AppManager::popApp() {
    goBack();
}
