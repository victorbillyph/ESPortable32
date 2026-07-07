#ifndef APP_PRINTERS_H
#define APP_PRINTERS_H

#include "../core/AppManager.h"
#include "../core/Menu.h"
#include "../core/PrinterManager.h"

class PrintersApp : public App {
public:
    PrintersApp(AppManager* mgr, PrinterManager* printerMgr);
    void init() override;
    void update() override;
    void draw(Display& d) override;
    void buttonClick() override;
    void buttonHold() override;
    void buttonVeryLong() override;
    void buttonDoubleClick() override;
    void exit() override;
    const char* name() override { return "Printer"; }

private:
    enum Mode { MENU, LIST, DETAIL };
    AppManager* _mgr;
    PrinterManager* _pm;
    Mode _mode;
    Menu _menu;
    const char* _items[3];
    const uint8_t* _icons[3];
    int _sel;

    void drawList(Display& d);
    void drawDetail(Display& d);
};

#endif
