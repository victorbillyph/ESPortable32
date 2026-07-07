#include "Menu.h"
#include "GUI.h"

static const int CARD_H = 22;
static const int CARD_GAP = 3;

Menu::Menu() : _items(nullptr), _icons(nullptr), _title(""),
               _titleIcon(nullptr), _count(0), _sel(0) {}

void Menu::setItems(const char* items[], int count) {
    _items = items; _count = count; _sel = 0;
}

void Menu::setIcons(const uint8_t* icons[]) { _icons = icons; }

void Menu::setTitle(const char* title) { _title = title; }

void Menu::setTitleIcon(const uint8_t* icon) { _titleIcon = icon; }

void Menu::next() {
    if (_count == 0) return;
    _sel = (_sel + 1) % _count;
}

void Menu::prev() {
    if (_count == 0) return;
    _sel = (_sel - 1 + _count) % _count;
}

int Menu::select() { return _sel; }

void Menu::draw(Display& d) {
    d.clear();

    // title bar
    GUI::drawMenuTitle(d, _title, _titleIcon);

    // cards
    int visible = 2;
    int start = _sel;
    if (start > _count - visible) start = _count - visible;
    if (start < 0) start = 0;

    int vy = 13;
    int cardW = SCREEN_W;

    for (int i = start; i < _count && i < start + visible; i++) {
        const uint8_t* icon = _icons ? _icons[i] : nullptr;
        GUI::drawCard(d, vy, cardW, _items[i], i == _sel, icon);
        vy += CARD_H + CARD_GAP;
    }

    // page indicator
    if (_count > 1) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%d/%d", _sel + 1, _count);
        d.oled().setCursor(SCREEN_W - strlen(buf) * 6 - 2, SCREEN_H - 9);
        d.oled().setTextSize(1);
        d.oled().setTextColor(SSD1306_WHITE);
        d.oled().print(buf);
    }

    // hint
    d.drawCenteredText(SCREEN_H - 9, "Click=prox  Hold=sel", 1);

    d.show();
}
