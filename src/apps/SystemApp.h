#ifndef APP_SYSTEM_H
#define APP_SYSTEM_H

#include "../core/AppManager.h"
#include "../core/Menu.h"

class SystemApp : public App {
public:
    SystemApp(AppManager* mgr);
    void init() override;
    void update() override;
    void draw(Display& d) override;
    void buttonClick() override;
    void buttonHold() override;
    void buttonVeryLong() override;
    void buttonDoubleClick() override;
    void exit() override;
    const char* name() override { return "System"; }

private:
    enum View { SYS_MENU, SYS_ABOUT, SYS_INFO };
    AppManager* _mgr;
    Menu _menu;
    View _view;
    const char* _items[5];
    const uint8_t* _icons[5];

    void drawAbout(Display& d);
    void drawSysInfo(Display& d);
};

#endif
