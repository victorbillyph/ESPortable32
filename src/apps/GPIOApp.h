#ifndef APP_GPIO_H
#define APP_GPIO_H

#include "../core/AppManager.h"
#include "../core/Menu.h"

struct GPIOPin {
    int pin;
    int mode;
    int value;
};

class GPIOApp : public App {
public:
    GPIOApp(AppManager* mgr);
    void init() override;
    void update() override;
    void draw(Display& d) override;
    void buttonClick() override;
    void buttonHold() override;
    void buttonVeryLong() override;
    void buttonDoubleClick() override;
    void exit() override;
    const char* name() override { return "GPIOs"; }

private:
    enum Mode { MENU, LIST };
    AppManager* _mgr;
    Menu _menu;
    const char* _items[2];
    const uint8_t* _icons[2];
    Mode _mode;
    int _sel;
    static const int PIN_COUNT = 17;
    GPIOPin _pins[PIN_COUNT];

    void initPins();
    void togglePin(int idx);
    void drawList(Display& d);
};

#endif
