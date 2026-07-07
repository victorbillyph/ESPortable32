#include "GUI.h"

static const int CARD_H = 22;
static const int CARD_R = 4;

void GUI::drawCard(Display& d, int y, int w, const char* text,
                   bool selected, const uint8_t* icon) {
    int x = 2;
    int cw = w - 4;
    if (selected) {
        d.fillRoundFrame(x, y, cw, CARD_H, CARD_R, true);
        d.oled().setTextColor(SSD1306_BLACK);
        d.oled().setTextSize(1);
    } else {
        d.drawRoundFrame(x, y, cw, CARD_H, CARD_R);
        d.oled().setTextColor(SSD1306_WHITE);
    }

    int tx = x + 8;
    if (icon) {
        d.oled().drawBitmap(tx, y + (CARD_H - 12) / 2, icon, 12, 12, SSD1306_WHITE);
        tx += 18;
    }

    d.oled().setCursor(tx, y + (CARD_H - 6) / 2);
    d.oled().print(text);
    d.oled().setTextColor(SSD1306_WHITE);

    // arrow indicator for selected
    if (selected) {
        d.oled().setCursor(x + cw - 10, y + (CARD_H - 6) / 2);
        d.oled().print(">");
    }
}

void GUI::drawCardMini(Display& d, int y, int w, const char* text,
                       const char* value, bool selected,
                       const uint8_t* icon) {
    int x = 2;
    int cw = w - 4;
    if (selected) {
        d.fillRoundFrame(x, y, cw, CARD_H, CARD_R, true);
        d.oled().setTextColor(SSD1306_BLACK);
        d.oled().setTextSize(1);
    } else {
        d.drawRoundFrame(x, y, cw, CARD_H, CARD_R);
        d.oled().setTextColor(SSD1306_WHITE);
    }

    int tx = x + 6;
    if (icon) {
        d.oled().drawBitmap(tx, y + (CARD_H - 12) / 2, icon, 12, 12, SSD1306_WHITE);
        tx += 16;
    }
    d.oled().setCursor(tx, y + (CARD_H - 6) / 2);
    d.oled().print(text);

    if (value) {
        d.oled().setCursor(x + cw - strlen(value) * 6 - 6, y + (CARD_H - 6) / 2);
        d.oled().print(value);
    }
    d.oled().setTextColor(SSD1306_WHITE);
}

void GUI::drawCheckbox(Display& d, int x, int y, bool checked) {
    d.oled().drawRect(x, y, 10, 10, SSD1306_WHITE);
    if (checked) {
        d.oled().fillRect(x + 2, y + 2, 6, 6, SSD1306_WHITE);
    }
}

void GUI::drawScrollbar(Display& d, int x, int y, int h, int pos, int total, int visible) {
    if (total <= visible) return;
    int barH = (h * visible) / total;
    if (barH < 4) barH = 4;
    int barY = y + (h - barH) * pos / (total - visible);
    d.drawRoundFrame(x - 1, barY, 3, barH, 1);
    d.fillFrame(x - 1, barY, 3, barH, true);
}

void GUI::drawIndicator(Display& d, int x, int y, int w, int h, int val, int maxVal) {
    if (maxVal <= 0) return;
    int fill = (w - 2) * val / maxVal;
    if (fill > w - 2) fill = w - 2;
    d.drawRoundFrame(x, y, w, h, 2);
    if (fill > 0) d.fillRoundFrame(x + 1, y + 1, fill, h - 2, 2, true);
}

void GUI::drawMenuTitle(Display& d, const char* title, const uint8_t* icon) {
    d.oled().fillRect(0, 0, SCREEN_W, 10, SSD1306_WHITE);
    d.oled().setTextColor(SSD1306_BLACK);
    d.oled().setTextSize(1);
    int x = 3;
    if (icon) {
        d.oled().drawBitmap(x, 0, icon, 10, 10, SSD1306_BLACK);
        x += 13;
    }
    d.oled().setCursor(x, 1);
    d.oled().print(title);
    d.oled().setTextColor(SSD1306_WHITE);
}

void GUI::drawSubtitle(Display& d, int y, const char* text) {
    d.oled().setTextColor(SSD1306_WHITE);
    d.oled().setTextSize(1);
    d.oled().setCursor(1, y);
    d.oled().print(text);
}

void GUI::drawMetric(Display& d, int x, int y, const char* label, const char* value) {
    d.oled().setTextSize(1);
    d.oled().setTextColor(SSD1306_WHITE);
    d.oled().setCursor(x, y);
    d.oled().print(label);
    d.oled().setCursor(x, y + 8);
    d.oled().print(value);
}
