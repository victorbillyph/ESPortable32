#include "PrinterManager.h"
#include <BluetoothSerial.h>
#include <Preferences.h>

PrinterManager::PrinterManager()
    : _count(0), _scanning(false), _scanStep(0) {}

void PrinterManager::begin() {
    Preferences prefs;
    if (!prefs.begin("esp32prt", true)) {
        Serial.println("[PrinterManager] Falhou ao abrir Preferences");
        return;
    }
    _count = prefs.getInt("count", 0);
    if (_count > MAX_NET_PRINTERS + MAX_BT_PRINTERS)
        _count = MAX_NET_PRINTERS + MAX_BT_PRINTERS;
    for (int i = 0; i < _count; i++) {
        String key = String(i);
        _printers[i].name = prefs.getString(("n_" + key).c_str(), "");
        _printers[i].ip = prefs.getString(("i_" + key).c_str(), "");
        _printers[i].port = prefs.getInt(("p_" + key).c_str(), 9100);
        _printers[i].mac = prefs.getString(("m_" + key).c_str(), "");
        _printers[i].isBluetooth = prefs.getBool(("b_" + key).c_str(), false);
        _printers[i].reachable = false;
    }
    prefs.end();
    Serial.printf("[PrinterManager] %d impressoras carregadas\n", _count);
}

void PrinterManager::update() {
    if (_scanning) {
        scanTick();
    }
}

int PrinterManager::printerCount() {
    return _count;
}

Printer PrinterManager::getPrinter(int idx) {
    if (idx < 0 || idx >= _count) return {};
    return _printers[idx];
}

Printer PrinterManager::getPrinterById(const String& id) {
    for (int i = 0; i < _count; i++) {
        if (_printers[i].ip == id || _printers[i].mac == id) {
            return _printers[i];
        }
    }
    return {};
}

void PrinterManager::startScan() {
    _scanning = true;
    _scanStep = 0;
    _localIP = WiFi.localIP();
    _subnet = WiFi.subnetMask();
    Serial.println("[PrinterManager] Scan iniciado");
}

bool PrinterManager::hasPrinter(const String& mac) {
    for (int i = 0; i < _count; i++) {
        if (_printers[i].mac == mac) return true;
    }
    return false;
}

void PrinterManager::addPrinter(const String& name, const String& ip, int port, const String& mac, bool isBT) {
    if (_count >= MAX_NET_PRINTERS + MAX_BT_PRINTERS) return;
    _printers[_count].name = name;
    _printers[_count].ip = ip;
    _printers[_count].port = port;
    _printers[_count].mac = mac;
    _printers[_count].isBluetooth = isBT;
    _printers[_count].reachable = true;
    _count++;

    Preferences prefs;
    if (prefs.begin("esp32prt", false)) {
        int idx = _count - 1;
        String k = String(idx);
        prefs.putString(("n_" + k).c_str(), name);
        prefs.putString(("i_" + k).c_str(), ip);
        prefs.putInt(("p_" + k).c_str(), port);
        prefs.putString(("m_" + k).c_str(), mac);
        prefs.putBool(("b_" + k).c_str(), isBT);
        prefs.putInt("count", _count);
        prefs.end();
    }
    Serial.printf("[PrinterManager] Adicionada: %s %s\n", name.c_str(), ip.length() ? ip.c_str() : mac.c_str());
}

void PrinterManager::scanTick() {
    uint32_t base = (uint32_t)_localIP[0] << 24 | (uint32_t)_localIP[1] << 16 | (uint32_t)_localIP[2] << 8;
    uint32_t mask = (uint32_t)_subnet[0] << 24 | (uint32_t)_subnet[1] << 16 | (uint32_t)_subnet[2] << 8;
    uint32_t net = base & mask;
    uint32_t broadcast = net | ~mask;
    uint32_t hostMin = (net & 0xFFFFFF00) + 1;
    uint32_t hostMax = (broadcast & 0xFFFFFF00) - 1;

    int total = hostMax - hostMin + 1;
    if (_scanStep >= total) {
        _scanning = false;
        Serial.println("[PrinterManager] Scan concluido");
        return;
    }

    uint32_t ip = hostMin + _scanStep;
    IPAddress addr((ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);

    WiFiClient client;
    if (client.connect(addr, 9100, 100)) {
        client.stop();
        bool exists = false;
        for (int i = 0; i < _count; i++) {
            if (_printers[i].ip == addr.toString()) {
                _printers[i].reachable = true;
                exists = true;
                break;
            }
        }
        if (!exists) {
            addPrinter("Impressora " + addr.toString(), addr.toString(), 9100, "", false);
        }
        Serial.printf("[PrinterManager] Impressora encontrada: %s\n", addr.toString().c_str());
    }
    _scanStep++;
}

String PrinterManager::escposEncode(const String& text) {
    String out;
    out += (char)0x1B; out += (char)0x40;
    out += (char)0x1B; out += (char)0x74; out += (char)0x03;
    out += text;
    out += (char)0x0A;
    out += (char)0x0A;
    out += (char)0x0A;
    out += (char)0x1D; out += (char)0x56; out += (char)0x00;
    return out;
}

String PrinterManager::sendTCP(const String& ip, int port, const uint8_t* data, size_t len) {
    WiFiClient client;
    if (!client.connect(ip.c_str(), port, 3000)) {
        return "Falha ao conectar em " + ip;
    }
    client.write(data, len);
    client.stop();
    return "";
}

String PrinterManager::printText(int idx, const String& text) {
    if (idx < 0 || idx >= _count) {
        return "Indice invalido";
    }
    Printer& p = _printers[idx];
    String encoded = escposEncode(text);
    if (p.isBluetooth) {
        return printTextBT(p.mac, text);
    } else {
        return printTextNet(p.ip, p.port, text);
    }
}

String PrinterManager::printTextBT(const String& mac, const String& text) {
    BluetoothSerial bt;
    if (!bt.connect(mac)) {
        return "Falha ao conectar BT em " + mac;
    }
    String encoded = escposEncode(text);
    bt.print(encoded);
    bt.disconnect();
    return "";
}

String PrinterManager::printTextNet(const String& ip, int port, const String& text) {
    String encoded = escposEncode(text);
    return sendTCP(ip, port, (const uint8_t*)encoded.c_str(), encoded.length());
}
