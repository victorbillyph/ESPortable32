#ifndef APP_HOME_H
#define APP_HOME_H

#include "../core/AppManager.h"
#include "../core/Display.h"
#include "../core/Menu.h"

class HomeApp : public App {
public:
    HomeApp(AppManager* mgr);
    void init() override;
    void update() override;
    void draw(Display& d) override;
    void buttonClick() override;
    void buttonHold() override;
    void buttonVeryLong() override;
    void buttonDoubleClick() override;
    void exit() override;
    const char* name() override { return "Home"; }

    int getSteps() { return _steps; }
    void addStep() { _steps++; }

private:
    enum Mode { WATCH_FACE, APP_LAUNCHER };
    AppManager* _mgr;
    Mode _mode;
    Menu _menu;
    const char* _items[11];
    const uint8_t* _icons[11];
    int _itemCount;

    unsigned long _bootTime;
    int _steps;
    int _stepGoal;
    char _timeStr[12];
    char _dateStr[16];
    unsigned long _lastTimeUpdate;

    void updateTime();
    void drawWatchFace(Display& d);
};

#endif
