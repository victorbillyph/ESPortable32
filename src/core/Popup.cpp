#include "Popup.h"
#include "../icons/Icons.h"
#include "Input.h"

// draw popup frame centered
static void drawPopupFrame(Display& d, const char* title, const uint8_t* icon) {
    d.clear();
    int pw = 120, ph = 48;
    int px = (SCREEN_W - pw) / 2;
    int py = (SCREEN_H - ph) / 2;
    d.oled().fillRect(px, py, pw, ph, SSD1306_BLACK);
    d.oled().drawRect(px, py, pw, ph, SSD1306_WHITE);
    // title bar
    d.oled().fillRect(px + 1, py + 1, pw - 2, 10, SSD1306_WHITE);
    d.oled().setTextColor(SSD1306_BLACK);
    d.oled().setCursor(px + 3, py + 2);
    d.oled().print(title);
    d.oled().setTextColor(SSD1306_WHITE);
}

void Popup::show(Display& d, const char* title, const char* msg, const uint8_t* icon) {
    drawPopupFrame(d, title, icon);
    int px = (SCREEN_W - 120) / 2;
    int py = (SCREEN_H - 48) / 2;
    // icon + message
    int tx = px + 4;
    if (icon) {
        d.oled().drawBitmap(px + 4, py + 16, icon, 16, 16, SSD1306_WHITE);
        tx += 20;
    }
    d.oled().setCursor(tx, py + 18);
    d.oled().setTextSize(1);
    d.oled().print(msg);
    d.oled().setCursor(px + 40, py + 36);
    d.oled().print("OK");
    d.show();
}

void Popup::info(Display& d, const char* msg) {
    show(d, "Info", msg, ICON_INFO);
}

void Popup::error(Display& d, const char* msg) {
    show(d, "Error", msg, ICON_ERROR);
}

void Popup::warning(Display& d, const char* msg) {
    show(d, "Warning", msg, ICON_WARNING);
}
