#include "Display.h"

Display::Display(uint8_t sda, uint8_t scl)
    : _oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET)
    , _sda(sda), _scl(scl) {}

bool Display::begin() {
    Wire.begin(_sda, _scl);
    delay(100);
    if (!_oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("[OLED] SSD1306 allocation FAILED");
        return false;
    }
    Serial.println("[OLED] SSD1306 initialized OK");
    _oled.clearDisplay();
    _oled.setTextColor(SSD1306_WHITE);
    _oled.setTextSize(1);
    _oled.display();
    return true;
}

void Display::clear() {
    _oled.clearDisplay();
}

void Display::show() {
    _oled.display();
}

void Display::drawText(int x, int y, const char* text, uint8_t size) {
    _oled.setTextSize(size);
    _oled.setCursor(x, y);
    _oled.print(text);
}

void Display::drawTitle(const char* text) {
    _oled.fillRect(0, 0, SCREEN_WIDTH, 10, SSD1306_WHITE);
    _oled.setTextColor(SSD1306_BLACK);
    _oled.setTextSize(1);
    _oled.setCursor(2, 1);
    _oled.print(text);
    _oled.setTextColor(SSD1306_WHITE);
}

void Display::drawStatusBar(const char* left, const char* right) {
    _oled.drawFastHLine(0, SCREEN_HEIGHT - 9, SCREEN_WIDTH, SSD1306_WHITE);
    _oled.setTextSize(1);
    if (left) {
        _oled.setCursor(1, SCREEN_HEIGHT - 8);
        _oled.print(left);
    }
    if (right) {
        int16_t x, y; uint16_t w, h;
        _oled.getTextBounds(right, 0, 0, &x, &y, &w, &h);
        _oled.setCursor(SCREEN_WIDTH - w - 1, SCREEN_HEIGHT - 8);
        _oled.print(right);
    }
}

void Display::drawItem(int index, const char* text, bool selected, int y) {
    int row = y + index * 10;
    if (row > SCREEN_HEIGHT - 12) return;
    if (selected) {
        _oled.fillRect(0, row, SCREEN_WIDTH, 10, SSD1306_WHITE);
        _oled.setTextColor(SSD1306_BLACK);
    } else {
        _oled.setTextColor(SSD1306_WHITE);
    }
    char buf[22];
    snprintf(buf, sizeof(buf), " %s", text);
    _oled.setCursor(0, row + 1);
    _oled.setTextSize(1);
    _oled.print(buf);
    _oled.setTextColor(SSD1306_WHITE);
}

void Display::splash(const char* line1, const char* line2) {
    clear();
    int y = line2 ? 20 : 28;
    showCentered(y, line1, 2);
    if (line2) showCentered(y + 18, line2, 1);
    show();
}

void Display::showMenu(const char* title, const char* items[], int count, int selection) {
    clear();
    drawTitle(title);
    int startIdx = 0;
    int visible = 5;
    if (selection >= visible) startIdx = selection - visible + 1;
    if (startIdx < 0) startIdx = 0;
    for (int i = 0; i < visible && (startIdx + i) < count; i++) {
        int idx = startIdx + i;
        drawItem(i, items[idx], idx == selection);
    }
    if (count > visible) {
        char page[8];
        int totalPages = (count + visible - 1) / visible;
        int curPage = selection / visible + 1;
        snprintf(page, sizeof(page), "%d/%d", curPage, totalPages);
        drawStatusBar("", page);
    } else {
        drawStatusBar("", nullptr);
    }
    show();
}

void Display::showMessage(const char* msg) {
    clear();
    showCentered(SCREEN_HEIGHT / 2 - 4, msg, 1);
    show();
}

void Display::showCentered(int y, const char* text, uint8_t size) {
    int16_t x1, y1; uint16_t w, h;
    _oled.setTextSize(size);
    _oled.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    _oled.setCursor((SCREEN_WIDTH - w) / 2, y);
    _oled.print(text);
}
