#pragma once
#include <Arduino.h>

class BluetoothService {
public:
    BluetoothService();
    void begin(const String& deviceName);
    void end();
    void update();
    bool isConnected();
    bool isEnabled();
    void setEnabled(bool enabled);
    void println(const String& msg);
    void print(const String& msg);
    bool available();
    String readStringUntil(char c);
    String readString();

private:
    bool _enabled;
};
