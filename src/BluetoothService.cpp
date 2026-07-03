#include "BluetoothService.h"
#include "BluetoothSerial.h"

static BluetoothSerial SerialBT;

BluetoothService::BluetoothService() : _enabled(true) {}

void BluetoothService::begin(const String& deviceName) {
    if (_enabled) {
        if (SerialBT.begin(deviceName)) {
            Serial.printf("[BT] Bluetooth iniciado: %s\n", deviceName.c_str());
        } else {
            Serial.println("[BT] Falha ao iniciar Bluetooth");
            _enabled = false;
        }
    }
}

void BluetoothService::end() {
    if (_enabled) {
        SerialBT.end();
        Serial.println("[BT] Bluetooth finalizado");
    }
}

void BluetoothService::update() {
    // nothing needed for now
}

bool BluetoothService::isConnected() {
    return _enabled && SerialBT.hasClient();
}

bool BluetoothService::isEnabled() { return _enabled; }

void BluetoothService::setEnabled(bool enabled) {
    _enabled = enabled;
}

void BluetoothService::println(const String& msg) {
    if (_enabled && SerialBT.hasClient()) {
        SerialBT.println(msg);
    }
}

void BluetoothService::print(const String& msg) {
    if (_enabled && SerialBT.hasClient()) {
        SerialBT.print(msg);
    }
}

bool BluetoothService::available() {
    return _enabled && SerialBT.available();
}

String BluetoothService::readStringUntil(char c) {
    if (!_enabled || !SerialBT.available()) return "";
    return SerialBT.readStringUntil(c);
}

String BluetoothService::readString() {
    if (!_enabled || !SerialBT.available()) return "";
    return SerialBT.readString();
}
