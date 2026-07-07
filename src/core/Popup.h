#ifndef POPUP_H
#define POPUP_H

#include <Arduino.h>
#include "Display.h"

enum PopupResult {
    POPUP_NONE,
    POPUP_OK,
    POPUP_CANCEL
};

class Popup {
public:
    static void show(Display& d, const char* title, const char* msg,
                     const uint8_t* icon = nullptr);

    static PopupResult confirm(Display& d, const char* title, const char* msg);

    static void error(Display& d, const char* msg);

    static void warning(Display& d, const char* msg);

    static void info(Display& d, const char* msg);
};

#endif
