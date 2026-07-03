#pragma once
#include <Arduino.h>

enum DeviceState {
    STATE_INIT = 0,
    STATE_SETUP = 1,
    STATE_CONFIGURED = 2,
    STATE_CONNECTED = 3,
    STATE_READY = 4
};

class StateManager {
public:
    StateManager();
    void begin();
    void setState(DeviceState s);
    DeviceState getState();
    const char* getStateName();
    void setLocked(bool locked);
    bool isLocked();
    void setPin(const String& pin);
    bool checkPin(const String& pin);
    bool hasPin();

private:
    DeviceState _state;
    bool _locked;
    String _pin;
};
