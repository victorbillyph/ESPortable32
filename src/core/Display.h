#ifndef CORE_DISPLAY_H
#define CORE_DISPLAY_H

#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Wire.h>

#define SCREEN_W 128
#define SCREEN_H 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

class Display {
public:
    Display(uint8_t sda = 21, uint8_t scl = 22);
    bool begin();
    void clear();
    void show();
    void setContrast(uint8_t val);

    Adafruit_SSD1306& oled();

    // text
    void drawText(int x, int y, const char* text, uint8_t size = 1);
    void drawCenteredText(int y, const char* text, uint8_t size = 1);

    // primitives
    void drawFrame(int x, int y, int w, int h);
    void fillFrame(int x, int y, int w, int h, bool color);
    void drawRoundFrame(int x, int y, int w, int h, int r);
    void fillRoundFrame(int x, int y, int w, int h, int r, bool color);
    void drawProgressBar(int x, int y, int w, int h, int percent);
    void drawHLine(int x, int y, int w);
    void drawVLine(int x, int y, int h);

    // icons
    void drawIcon(int x, int y, const uint8_t* bitmap, int w, int h);

    // watch-specific
    void drawBattery(int x, int y, int pct, bool charging);
    void drawSignal(int x, int y, int bars);
    void drawTopBar(const char* timeStr, bool wifi, bool bt, int batPct);

    // power
    void sleep();
    void wake();
    bool isAsleep() { return _asleep; }

private:
    Adafruit_SSD1306 _oled;
    uint8_t _sda, _scl;
    bool _asleep;
};

#endif
