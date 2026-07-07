#ifndef APP_QUICK_SETTINGS_H
#define APP_QUICK_SETTINGS_H

#include "../core/AppManager.h"
#include "../core/Display.h"

class QuickSettingsApp : public App {
public:
    QuickSettingsApp(AppManager* mgr);
    void init() override;
    void update() override;
    void draw(Display& d) override;
    void buttonClick() override;
    void buttonHold() override;
    void buttonVeryLong() override;
    void buttonDoubleClick() override;
    void exit() override;
    const char* name() override { return "QuickSettings"; }

private:
    AppManager* _mgr;
    int _sel;
    bool _wifiOn;
    bool _btOn;
    bool _flashOn;
    uint8_t _brightness;
};

#endif
