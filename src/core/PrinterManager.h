#ifndef CORE_PRINTER_MANAGER_H
#define CORE_PRINTER_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <vector>

#define MAX_NET_PRINTERS 16
#define MAX_BT_PRINTERS 8

struct Printer {
    String name;
    String ip;
    int port;
    String mac;
    bool isBluetooth;
    bool reachable;
};

class PrinterManager {
public:
    PrinterManager();
    void begin();
    void update();

    int printerCount();
    Printer getPrinter(int idx);
    Printer getPrinterById(const String& id);

    void startScan();
    bool isScanning() { return _scanning; }

    String printText(int idx, const String& text);
    String printTextBT(const String& mac, const String& text);
    String printTextNet(const String& ip, int port, const String& text);

    void addPrinter(const String& name, const String& ip, int port, const String& mac, bool isBT);
    bool hasPrinter(const String& mac);

private:
    Printer _printers[MAX_NET_PRINTERS + MAX_BT_PRINTERS];
    int _count;

    bool _scanning;
    int _scanStep;
    IPAddress _localIP;
    IPAddress _subnet;

    void scanTick();
    String escposEncode(const String& text);
    String sendTCP(const String& ip, int port, const uint8_t* data, size_t len);
};

#endif
