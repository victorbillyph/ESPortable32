#include "Input.h"

Input::Input(uint8_t pin) : _pin(pin) {}

void Input::begin() {
    pinMode(_pin, INPUT_PULLUP);
    _lastState = digitalRead(_pin);
    _pressTime = 0;
    _releaseTime = 0;
    _clickCount = 0;
    _longDone = false;
    _veryLongDone = false;
    _pending = BTN_NONE;
}

bool Input::isHeldAtBoot() {
    return digitalRead(_pin) == LOW;
}

void Input::reset() {
    _clickCount = 0;
    _longDone = false;
    _veryLongDone = false;
    _pending = BTN_NONE;
}

ButtonEvent Input::update() {
    ButtonEvent result = BTN_NONE;
    bool cur = digitalRead(_pin);
    unsigned long now = millis();

    if (cur != _lastState) {
        delay(DEBOUNCE);
        cur = digitalRead(_pin);
        if (cur == _lastState) return BTN_NONE;
        _lastState = cur;

        if (cur == LOW) {
            _pressTime = now;
            _longDone = false;
            _veryLongDone = false;
        } else {
            _releaseTime = now;
            unsigned long held = now - _pressTime;

            if (held < HOLD_MS) {
                _clickCount++;
                if (_clickCount == 2) {
                    _clickCount = 0;
                    result = BTN_DOUBLE_CLICK;
                } else {
                    _pending = BTN_CLICK;
                }
            }
        }
    }

    if (cur == LOW) {
        unsigned long held = now - _pressTime;
        if (!_veryLongDone && held >= VERY_LONG_MS) {
            _veryLongDone = true;
            _longDone = true;
            _clickCount = 0;
            result = BTN_VERY_LONG;
        } else if (!_longDone && held >= HOLD_MS) {
            _longDone = true;
            _clickCount = 0;
            result = BTN_HOLD;
        }
    }

    if (result != BTN_NONE) {
        return result;
    }

    // handle pending single click with timeout
    if (_pending == BTN_CLICK && (now - _releaseTime > DOUBLE_CLICK_WIN)) {
        result = BTN_CLICK;
        _pending = BTN_NONE;
        _clickCount = 0;
    }

    return result;
}
