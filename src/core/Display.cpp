#include "Display.h"

Display::Display(uint8_t sda, uint8_t scl)
    : _oled(SCREEN_W, SCREEN_H, &Wire, OLED_RESET)
    , _sda(sda), _scl(scl), _asleep(false) {}

bool Display::begin() {
    Wire.begin(_sda, _scl);
    delay(100);
    if (!_oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("[Display] FAILED");
        return false;
    }
    Serial.println("[Display] OK");
    _oled.clearDisplay();
    _oled.setTextColor(SSD1306_WHITE);
    _oled.setTextSize(1);
    _oled.display();
    _asleep = false;
    return true;
}

void Display::clear() { _oled.clearDisplay(); }
void Display::show() { _oled.display(); }

void Display::setContrast(uint8_t val) {
    _oled.ssd1306_command(SSD1306_SETCONTRAST);
    _oled.ssd1306_command(val);
}

Adafruit_SSD1306& Display::oled() { return _oled; }

void Display::drawText(int x, int y, const char* text, uint8_t size) {
    _oled.setTextSize(size);
    _oled.setCursor(x, y);
    _oled.print(text);
}

void Display::drawCenteredText(int y, const char* text, uint8_t size) {
    int16_t x1, y1; uint16_t w, h;
    _oled.setTextSize(size);
    _oled.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    _oled.setCursor((SCREEN_W - w) / 2, y);
    _oled.print(text);
}

void Display::drawFrame(int x, int y, int w, int h) {
    _oled.drawRect(x, y, w, h, SSD1306_WHITE);
}

void Display::fillFrame(int x, int y, int w, int h, bool color) {
    if (color) _oled.fillRect(x, y, w, h, SSD1306_WHITE);
    else _oled.fillRect(x, y, w, h, SSD1306_BLACK);
}

void Display::drawRoundFrame(int x, int y, int w, int h, int r) {
    _oled.drawRoundRect(x, y, w, h, r, SSD1306_WHITE);
}

void Display::fillRoundFrame(int x, int y, int w, int h, int r, bool color) {
    if (color) _oled.fillRoundRect(x, y, w, h, r, SSD1306_WHITE);
    else _oled.fillRoundRect(x, y, w, h, r, SSD1306_BLACK);
}

void Display::drawProgressBar(int x, int y, int w, int h, int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    _oled.drawRoundRect(x, y, w, h, 2, SSD1306_WHITE);
    int fillW = (w - 4) * percent / 100;
    if (fillW > 0) _oled.fillRoundRect(x + 2, y + 2, fillW, h - 4, 2, SSD1306_WHITE);
}

void Display::drawHLine(int x, int y, int w) {
    _oled.drawFastHLine(x, y, w, SSD1306_WHITE);
}

void Display::drawVLine(int x, int y, int h) {
    _oled.drawFastVLine(x, y, h, SSD1306_WHITE);
}

void Display::drawIcon(int x, int y, const uint8_t* bitmap, int w, int h) {
    _oled.drawBitmap(x, y, bitmap, w, h, SSD1306_WHITE);
}

void Display::drawBattery(int x, int y, int pct, bool charging) {
    _oled.drawRect(x, y, 16, 8, SSD1306_WHITE);       // body
    _oled.fillRect(x + 16, y + 2, 2, 4, SSD1306_WHITE); // tip
    int fill = (pct * 12) / 100;
    if (fill > 12) fill = 12;
    if (fill > 0) _oled.fillRect(x + 2, y + 2, fill, 4, SSD1306_WHITE);
    if (charging) {
        // lightning bolt inside
        _oled.drawLine(x + 5, y + 1, x + 9, y + 4, SSD1306_WHITE);
        _oled.drawLine(x + 9, y + 4, x + 6, y + 4, SSD1306_WHITE);
        _oled.drawLine(x + 6, y + 4, x + 10, y + 7, SSD1306_WHITE);
    }
}

void Display::drawSignal(int x, int y, int bars) {
    if (bars > 5) bars = 5;
    if (bars < 0) bars = 0;
    for (int i = 0; i < 5; i++) {
        int h = 2 + i * 2;
        if (i < bars) {
            _oled.fillRect(x + i * 3, y + 10 - h, 2, h, SSD1306_WHITE);
        } else {
            _oled.drawRect(x + i * 3, y + 10 - h, 2, h, SSD1306_WHITE);
        }
    }
}

void Display::drawTopBar(const char* timeStr, bool wifi, bool bt, int batPct) {
    char buf[24];
    // time on left
    _oled.setCursor(1, 1);
    _oled.setTextSize(1);
    _oled.setTextColor(SSD1306_WHITE);
    _oled.print(timeStr);

    // WiFi icon
    int x = SCREEN_W - 24;
    if (wifi) {
        _oled.fillRect(x, 4, 2, 1, SSD1306_WHITE);
        _oled.fillRect(x - 1, 5, 4, 1, SSD1306_WHITE);
        _oled.fillRect(x - 2, 6, 6, 1, SSD1306_WHITE);
        _oled.fillRect(x - 3, 7, 8, 1, SSD1306_WHITE);
    }
    x += 10;

    // BT icon
    if (bt) {
        _oled.setCursor(x, 1);
        _oled.print("b");
        x += 8;
    }

    // battery
    drawBattery(x, 0, batPct, false);
}

void Display::sleep() {
    _oled.ssd1306_command(SSD1306_DISPLAYOFF);
    _asleep = true;
}

void Display::wake() {
    _oled.ssd1306_command(SSD1306_DISPLAYON);
    _asleep = false;
}
