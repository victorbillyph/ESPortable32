#ifndef APP_RELOGIO_H
#define APP_RELOGIO_H

#include "../core/AppManager.h"
#include "../core/Menu.h"
#include "../core/Speaker.h"
#include <Preferences.h>

#define MAX_ALARMS 8

struct AlarmData {
    uint8_t hour;
    uint8_t minute;
    uint8_t days;
    bool enabled;
};

class RelogioApp : public App {
public:
    RelogioApp(AppManager* mgr);
    void init() override;
    void update() override;
    void draw(Display& d) override;
    void buttonClick() override;
    void buttonHold() override;
    void buttonVeryLong() override;
    void buttonDoubleClick() override;
    void exit() override;
    const char* name() override { return "Relogio"; }

    void tickAlarm(int h, int m, int wday);
    bool isAlarming() { return _alarmFired; }

private:
    enum Mode { MENU, LIST, EDIT_HOUR, EDIT_MINUTE, EDIT_DAYS, ALARMING };
    AppManager* _mgr;
    Menu _menu;
    Mode _mode;
    int _sel;
    int _editIdx;
    int _scrollPos;
    const char* _items[3];
    const uint8_t* _icons[3];

    AlarmData _alarms[MAX_ALARMS];
    int _count;
    Preferences _prefs;

    bool _alarmFired;
    int _firedIdx;
    int _lastFH, _lastFM;
    bool _snoozed;
    int _snoozeH, _snoozeM;

    unsigned long _alarmStart;
    unsigned long _lastBeep;
    bool _beepOn;

    void load();
    void save();
    void drawList(Display& d);
    void drawEditing(Display& d);
    void drawAlarming(Display& d);
    void dismissAlarm();
    void snoozeAlarm();
};

#endif
