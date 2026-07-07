#ifndef GUI_H
#define GUI_H

#include <Arduino.h>
#include "Display.h"

class GUI {
public:
    static void drawCard(Display& d, int y, int w, const char* text,
                         bool selected, const uint8_t* icon = nullptr);

    static void drawCardMini(Display& d, int y, int w, const char* text,
                             const char* value, bool selected,
                             const uint8_t* icon = nullptr);

    static void drawCheckbox(Display& d, int x, int y, bool checked);

    static void drawScrollbar(Display& d, int x, int y, int h, int pos, int total, int visible);

    static void drawIndicator(Display& d, int x, int y, int w, int h, int val, int maxVal);

    static void drawMenuTitle(Display& d, const char* title, const uint8_t* icon = nullptr);

    static void drawSubtitle(Display& d, int y, const char* text);

    static void drawMetric(Display& d, int x, int y, const char* label, const char* value);
};

#endif
