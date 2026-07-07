#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include "Display.h"

class Menu {
public:
    Menu();
    void setItems(const char* items[], int count);
    void setIcons(const uint8_t* icons[]);
    void setTitle(const char* title);
    void setTitleIcon(const uint8_t* icon);

    void next();
    void prev();
    int select();
    int getSelection() { return _sel; }
    int getCount() { return _count; }
    void reset() { _sel = 0; }

    void draw(Display& d);

private:
    const char** _items;
    const uint8_t** _icons;
    const char* _title;
    const uint8_t* _titleIcon;
    int _count;
    int _sel;
};

#endif
