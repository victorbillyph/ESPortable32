#ifndef INPUT_H
#define INPUT_H

#include <Arduino.h>

enum ButtonEvent {
    BTN_NONE,
    BTN_CLICK,
    BTN_HOLD,
    BTN_VERY_LONG,
    BTN_DOUBLE_CLICK
};

class Input {
public:
    Input(uint8_t pin = 0);
    void begin();
    ButtonEvent update();
    bool isHeldAtBoot();
    void reset();

private:
    uint8_t _pin;
    bool _lastState;
    unsigned long _pressTime;
    unsigned long _releaseTime;
    int _clickCount;
    bool _longDone;
    bool _veryLongDone;
    ButtonEvent _pending;

    static const unsigned long DEBOUNCE = 30;
    static const unsigned long HOLD_MS = 600;
    static const unsigned long VERY_LONG_MS = 2000;
    static const unsigned long DOUBLE_CLICK_WIN = 400;
};

#endif
