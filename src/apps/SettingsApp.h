#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include "../core/AppManager.h"
#include "../core/Menu.h"

class SettingsApp : public App {
public:
    SettingsApp(AppManager* mgr);
    void init() override;
    void update() override;
    void draw(Display& d) override;
    void buttonClick() override;
    void buttonHold() override;
    void buttonVeryLong() override;
    void buttonDoubleClick() override;
    void exit() override;
    const char* name() override { return "Settings"; }

private:
    AppManager* _mgr;
    Menu _menu;
    const char* _items[6];
    const uint8_t* _icons[6];
    uint8_t _contrast;
    int _spkPos, _spkNeg;
    int _editPin;
    bool _editing;

    void setContrast(Display& d, uint8_t val);
    void drawSpeakerEdit(Display& d);
    void saveSpeakerPins();
};

#endif
