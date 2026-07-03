#include "StateManager.h"

StateManager::StateManager() : _state(STATE_INIT), _locked(true) {}

void StateManager::begin() {
    _state = STATE_SETUP;
    Serial.printf("[State] %d -> %d\n", STATE_INIT, _state);
}

void StateManager::setState(DeviceState s) {
    DeviceState old = _state;
    _state = s;
    Serial.printf("[State] %d -> %d\n", old, _state);
}

DeviceState StateManager::getState() { return _state; }

const char* StateManager::getStateName() {
    switch (_state) {
        case STATE_INIT: return "INIT";
        case STATE_SETUP: return "SETUP";
        case STATE_CONFIGURED: return "CONFIGURED";
        case STATE_CONNECTED: return "CONNECTED";
        case STATE_READY: return "READY";
    }
    return "UNKNOWN";
}

void StateManager::setLocked(bool locked) { _locked = locked; }
bool StateManager::isLocked() { return _locked; }

void StateManager::setPin(const String& pin) { _pin = pin; }

bool StateManager::checkPin(const String& pin) {
    return _pin.length() > 0 && _pin == pin;
}

bool StateManager::hasPin() { return _pin.length() > 0; }
