#ifndef APP_BLUETOOTH_H
#define APP_BLUETOOTH_H

#include "../core/AppManager.h"
#include "../core/Menu.h"
#include "../core/PrinterManager.h"
#include <BLEDevice.h>
#include <BLEAdvertisedDevice.h>
#include <BLEScan.h>

class BluetoothApp : public App {
public:
    BluetoothApp(AppManager* mgr, PrinterManager* pm);
    void init() override;
    void update() override;
    void draw(Display& d) override;
    void buttonClick() override;
    void buttonHold() override;
    void buttonVeryLong() override;
    void buttonDoubleClick() override;
    void exit() override;
    const char* name() override { return "BT"; }

    void onRadarDevice(BLEAdvertisedDevice& device);

private:
    enum Mode { MENU, SCANNING, DEVICES, DETAIL, CONNECTING, CONNECTED, RADAR };
    AppManager* _mgr;
    PrinterManager* _pm;
    Menu _menu;
    const char* _items[4];
    const uint8_t* _icons[4];
    Mode _mode;
    int _scanType;
    int _detailSel;

    bool _scanning;
    int _deviceCount;

    struct BTDevice {
        String name;
        String mac;
        int rssi;
    };
    BTDevice _devices[25];
    int _sel;
    unsigned long _scanStart;
    unsigned long _statusTimer;
    BLEScan* _scanner;
    String _connectMsg;

    int _radarSel;
    int _radarAngle;
    unsigned long _nextRadarScan;
    bool _fromRadar;

    void scanBLE();
    void scanClassic();
    void connectDevice();
    void doConnect();
    void startRadar();
    void stopRadar();
    void drawScanning(Display& d);
    void drawDevices(Display& d);
    void drawDetail(Display& d);
    void drawConnecting(Display& d);
    void drawConnected(Display& d);
    void drawRadar(Display& d);
};

#endif
