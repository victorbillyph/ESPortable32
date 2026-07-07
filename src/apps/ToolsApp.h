#ifndef APP_TOOLS_H
#define APP_TOOLS_H

#include "../core/AppManager.h"
#include "../core/Menu.h"

class ToolsApp : public App {
public:
    ToolsApp(AppManager* mgr);
    void init() override;
    void update() override;
    void draw(Display& d) override;
    void buttonClick() override;
    void buttonHold() override;
    void buttonVeryLong() override;
    void buttonDoubleClick() override;
    void exit() override;
    const char* name() override { return "Tools"; }

private:
    enum SubView { TOOL_MENU, TOOL_INFO, TOOL_RANDOM, TOOL_RAM, TOOL_SLEEP };
    AppManager* _mgr;
    Menu _menu;
    SubView _view;
    const char* _items[6];
    const uint8_t* _icons[6];
    int _itemCount;

    void drawInfo(Display& d);
    void drawRandom(Display& d);
    void drawRam(Display& d);
    void doSleep(Display& d);
};

#endif
