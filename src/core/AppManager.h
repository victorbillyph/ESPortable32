#ifndef APP_MANAGER_H
#define APP_MANAGER_H

#include <Arduino.h>
#include "Display.h"
#include "Input.h"

class App {
public:
    virtual ~App() {}
    virtual void init() = 0;
    virtual void update() = 0;
    virtual void draw(Display& d) = 0;
    virtual void buttonClick() {}
    virtual void buttonHold() {}
    virtual void buttonDoubleClick() {}
    virtual void buttonVeryLong() {}
    virtual void exit() {}
    virtual const char* name() = 0;
};

class AppManager {
public:
    AppManager(Display& display);
    void addApp(App* app);
    void begin();
    void update(ButtonEvent btn);
    void draw();
    void switchTo(int idx);
    void goBack();
    void pushApp(int idx);
    void popApp();
    Display& display() { return _display; }

    int currentApp() { return _current; }
    int appCount() { return _appCount; }
    App* getApp(int idx) { return _apps[idx]; }
    App* getCurrentApp() { return _apps[_current]; }

private:
    static const int MAX_APPS = 16;
    static const int MAX_STACK = 8;
    Display& _display;
    App* _apps[MAX_APPS];
    int _appCount;
    int _current;
    int _stack[MAX_STACK];
    int _stackDepth;
    bool _initialized;
};

#endif
