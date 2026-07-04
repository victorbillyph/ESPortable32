#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Wire.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDR     0x3C

class Display {
public:
    Display(uint8_t sda = 21, uint8_t scl = 22);
    bool begin();
    void clear();
    void show();

    void drawText(int x, int y, const char* text, uint8_t size = 1);
    void drawTitle(const char* text);
    void drawStatusBar(const char* left, const char* right = nullptr);
    void drawItem(int index, const char* text, bool selected, int y = 16);

    void splash(const char* line1, const char* line2 = nullptr);
    void showMenu(const char* title, const char* items[], int count, int selection);
    void showMessage(const char* msg);
    void showCentered(int y, const char* text, uint8_t size = 1);

private:
    Adafruit_SSD1306 _oled;
    uint8_t _sda, _scl;
};

#endif
