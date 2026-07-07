#ifndef CORE_STATE_H
#define CORE_STATE_H

#include <Arduino.h>

struct SystemState {
    bool timeSynced = false;
    int hour = 0, minute = 0, second = 0;
    int day = 1, month = 1, year = 2026;
    int wday = 0;
    int weatherTemp = 0;
    char weatherDesc[16] = "";
    bool weatherLoaded = false;
    bool wifiConnected = false;
};

extern SystemState state;

#endif
