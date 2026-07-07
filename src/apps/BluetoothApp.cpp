#include "BluetoothApp.h"
#include "../icons/Icons.h"
#include "../core/GUI.h"
#include <BluetoothSerial.h>
#include <math.h>

// BLE callback for radar mode
class RadarCallback : public BLEAdvertisedDeviceCallbacks {
public:
    BluetoothApp* app;
    RadarCallback(BluetoothApp* a) : app(a) {}
    void onResult(BLEAdvertisedDevice device) {
        app->onRadarDevice(device);
    }
};

BluetoothApp::BluetoothApp(AppManager* mgr, PrinterManager* pm) : _mgr(mgr), _pm(pm),
    _mode(MENU), _scanning(false), _deviceCount(0), _sel(0), _detailSel(0),
    _scanStart(0), _statusTimer(0), _scanner(nullptr),
    _radarSel(-1), _radarAngle(0), _nextRadarScan(0), _fromRadar(false) {}

void BluetoothApp::init() {
    _items[0] = "Scan BLE";
    _items[1] = "Radar";
    _items[2] = "Scan Classic";
    _items[3] = "Voltar";
    _icons[0] = ICON_BLUETOOTH;
    _icons[1] = ICON_SIGNAL;
    _icons[2] = ICON_BLUETOOTH;
    _icons[3] = ICON_BACK;
    _menu.setItems(_items, 4);
    _menu.setIcons(_icons);
    _menu.setTitle("Bluetooth");
    _menu.setTitleIcon(ICON_BLUETOOTH);
    _menu.reset();
    _mode = MENU;
    _scanning = false;
}

void BluetoothApp::update() {
    if (_mode == SCANNING && _scanning && _scanType == 0) {
        if (millis() - _scanStart >= 3200) {
            if (_scanner) {
                BLEScanResults results = _scanner->getResults();
                _deviceCount = min((int)results.getCount(), 25);
                for (int i = 0; i < _deviceCount; i++) {
                    BLEAdvertisedDevice d = results.getDevice(i);
                    _devices[i].name = d.getName().c_str();
                    if (_devices[i].name.length() == 0) {
                        if (d.haveName()) _devices[i].name = d.getName().c_str();
                        else _devices[i].name = "(sem nome)";
                    }
                    _devices[i].mac = d.getAddress().toString().c_str();
                    _devices[i].rssi = d.getRSSI();
                }
            }
            _scanning = false;
            _mode = DEVICES;
            _sel = 0;
        }
    }
    if (_mode == CONNECTING) {
        doConnect();
    }
    if (_mode == CONNECTED && millis() - _statusTimer > 2000) {
        _mode = _fromRadar ? RADAR : DEVICES;
    }
    if (_mode == RADAR) {
        _radarAngle = (_radarAngle + 3) % 360;

        if (_radarSel >= _deviceCount) _radarSel = _deviceCount - 1;

        unsigned long now = millis();
        if (_scanning && now - _scanStart > 3200) {
            _scanning = false;
            _nextRadarScan = now + 1000;
        }
        if (!_scanning && now > _nextRadarScan) {
            _scanner = BLEDevice::getScan();
            _scanner->setActiveScan(true);
            _scanner->setInterval(100);
            _scanner->setWindow(99);
            _scanner->start(3, false);
            _scanning = true;
            _scanStart = now;
        }
    }
}

void BluetoothApp::draw(Display& d) {
    switch (_mode) {
        case MENU: _menu.draw(d); break;
        case SCANNING: drawScanning(d); break;
        case DEVICES: drawDevices(d); break;
        case DETAIL: drawDetail(d); break;
        case CONNECTING: drawConnecting(d); break;
        case CONNECTED: drawConnected(d); break;
        case RADAR: drawRadar(d); break;
    }
}

void BluetoothApp::buttonClick() {
    switch (_mode) {
        case MENU: _menu.next(); break;
        case DEVICES: {
            int cnt = _deviceCount > 0 ? _deviceCount : 1;
            _sel = (_sel + 1) % cnt;
            break;
        }
        case DETAIL: _detailSel = (_detailSel + 1) % 2; break;
        case RADAR:
            if (_deviceCount > 0) {
                _radarSel = (_radarSel + 1) % _deviceCount;
            }
            break;
        default: break;
    }
}

void BluetoothApp::buttonHold() {
    switch (_mode) {
        case MENU:
            switch (_menu.select()) {
                case 0: scanBLE(); break;
                case 1: startRadar(); break;
                case 2: scanClassic(); break;
                case 3: _mgr->popApp(); break;
            }
            break;
        case DEVICES:
            if (_deviceCount > 0) { _mode = DETAIL; _detailSel = 0; _fromRadar = false; }
            break;
        case DETAIL:
            if (_detailSel == 0) {
                _mode = CONNECTING;
                _connectMsg = "";
            } else {
                _mode = _fromRadar ? RADAR : DEVICES;
            }
            break;
        case CONNECTED: _mode = _fromRadar ? RADAR : DEVICES; break;
        case SCANNING: _scanning = false; _mode = MENU; break;
        case RADAR:
            if (_radarSel >= 0 && _radarSel < _deviceCount) {
                _sel = _radarSel;
                _mode = DETAIL;
                _detailSel = 0;
                _fromRadar = true;
            }
            break;
    }
}

void BluetoothApp::buttonVeryLong() {
    if (_mode == RADAR) stopRadar();
    _mode = MENU; _scanning = false;
}

void BluetoothApp::buttonDoubleClick() {
    if (_mode == RADAR) stopRadar();
    _mode = MENU; _scanning = false;
}

void BluetoothApp::exit() {
    if (_mode == RADAR) stopRadar();
}

void BluetoothApp::scanBLE() {
    _deviceCount = 0;
    _sel = 0;
    _scanType = 0;
    _scanning = true;
    _mode = SCANNING;
    _scanStart = millis();

    static bool bleInit = false;
    if (!bleInit) {
        BLEDevice::init("");
        bleInit = true;
    }

    _scanner = BLEDevice::getScan();
    _scanner->setActiveScan(true);
    _scanner->setInterval(100);
    _scanner->setWindow(99);
    _scanner->start(3, true);
}

void BluetoothApp::scanClassic() {
    _deviceCount = 0;
    _sel = 0;
    _scanType = 1;
    _devices[0].name = "Classic BT (SPP)";
    _devices[0].mac = "";
    _devices[0].rssi = 0;
    _deviceCount = 1;
    _mode = DEVICES;
    _sel = 0;
}

void BluetoothApp::connectDevice() {
    _mode = CONNECTED;
    _statusTimer = millis();
}

static bool isPrinterName(const String& name) {
    String n = name;
    n.toLowerCase();
    const char* keywords[] = {
        "printer", "thermal", "impressora", "pos", "mp-", "mp_",
        "58mm", "80mm", "t公主", "receipt"
    };
    for (unsigned i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        if (n.indexOf(keywords[i]) >= 0) return true;
    }
    return false;
}

void BluetoothApp::doConnect() {
    String mac = _devices[_sel].mac;
    String name = _devices[_sel].name;

    if (mac.length() == 0) {
        _connectMsg = "MAC invalido";
        _mode = CONNECTED;
        _statusTimer = millis();
        return;
    }

    BluetoothSerial bt;
    bool connected = bt.connect(mac);

    if (connected) {
        bt.disconnect();
        if (isPrinterName(name) || isPrinterName(_devices[_sel].name)) {
            _pm->addPrinter(name, "", 0, mac, true);
            _connectMsg = "Impressora adicionada!";
        } else {
            _connectMsg = "Conectado!";
        }
    } else {
        if (isPrinterName(name)) {
            _pm->addPrinter(name, "", 0, mac, true);
            _connectMsg = "Impressora adicionada!";
        } else {
            _connectMsg = "Falha na conexao";
        }
    }

    _mode = CONNECTED;
    _statusTimer = millis();
}

void BluetoothApp::startRadar() {
    _deviceCount = 0;
    _radarSel = -1;
    _radarAngle = 0;
    _scanning = false;
    _mode = RADAR;
    _nextRadarScan = 0;
    _fromRadar = false;

    static bool bleInit = false;
    if (!bleInit) {
        BLEDevice::init("");
        bleInit = true;
    }

    static RadarCallback* cb = nullptr;
    _scanner = BLEDevice::getScan();
    if (!cb) {
        cb = new RadarCallback(this);
        _scanner->setAdvertisedDeviceCallbacks(cb, false);
    }
    _scanner->setActiveScan(true);
    _scanner->setInterval(100);
    _scanner->setWindow(99);
    _scanner->start(3, false);
    _scanning = true;
    _scanStart = millis();
}

void BluetoothApp::stopRadar() {
    if (_scanner) {
        _scanner->stop();
    }
    _scanning = false;
}

void BluetoothApp::onRadarDevice(BLEAdvertisedDevice& device) {
    String mac = device.getAddress().toString().c_str();
    String name = device.getName().c_str();
    int rssi = device.getRSSI();

    for (int i = 0; i < _deviceCount; i++) {
        if (_devices[i].mac == mac) {
            _devices[i].rssi = rssi;
            if (_devices[i].name == "(sem nome)" && name.length() > 0) {
                _devices[i].name = name;
            }
            return;
        }
    }

    if (_deviceCount < 25) {
        int i = _deviceCount++;
        _devices[i].mac = mac;
        _devices[i].name = name.length() > 0 ? name : "(sem nome)";
        _devices[i].rssi = rssi;
    }
}

void BluetoothApp::drawScanning(Display& d) {
    d.clear();
    GUI::drawMenuTitle(d, _scanType == 0 ? "Scan BLE" : "Scan BT", ICON_BLUETOOTH);

    int dots = (millis() / 400) % 4;
    char buf[22];
    snprintf(buf, sizeof(buf), "Escaneando%s", String("...").substring(0, dots).c_str());
    d.drawCenteredText(24, buf, 1);

    if (_scanType == 0) {
        int found = 0;
        if (_scanner) {
            BLEScanResults r = _scanner->getResults();
            found = r.getCount();
        }
        int elapsed = (millis() - _scanStart) / 1000;
        snprintf(buf, sizeof(buf), "%d encontrados  %ds", found, min(elapsed, 3));
        d.drawCenteredText(36, buf, 1);
    }

    d.drawCenteredText(54, "Segure p/ cancelar", 1);
    d.show();
}

void BluetoothApp::drawDevices(Display& d) {
    d.clear();
    GUI::drawMenuTitle(d, _scanType == 0 ? "BLE" : "BT Classic", ICON_BLUETOOTH);

    if (_deviceCount == 0) {
        d.drawCenteredText(28, "Nenhum encontrado", 1);
    } else {
        int y = 13;
        int maxShow = 5;
        int startIdx = _sel < maxShow ? 0 : _sel - maxShow + 1;
        int endIdx = min(startIdx + maxShow, _deviceCount);

        for (int i = startIdx; i < endIdx; i++) {
            char buf[22];
            char prefix = (i == _sel) ? '>' : ' ';
            String n = _devices[i].name;
            if (n.length() > 18) n = n.substring(0, 17) + "~";
            snprintf(buf, sizeof(buf), "%c%s", prefix, n.c_str());
            d.drawText(1, y, buf, 1);
            y += 10;
        }
    }

    d.drawCenteredText(57, "Segure p/ detalhes", 1);
    d.show();
}

void BluetoothApp::drawDetail(Display& d) {
    d.clear();
    if (_sel >= _deviceCount) { _mode = _fromRadar ? RADAR : DEVICES; return; }

    BTDevice& dev = _devices[_sel];
    char buf[24];
    int y = 4;

    String name = dev.name;
    if (name.length() > 16) name = name.substring(0, 15) + "~";
    d.drawCenteredText(y, name.c_str(), 1); y += 11;

    snprintf(buf, sizeof(buf), "MAC: %s", dev.mac.c_str());
    d.drawText(1, y, buf, 1); y += 9;

    snprintf(buf, sizeof(buf), "RSSI: %d dBm", dev.rssi);
    d.drawText(1, y, buf, 1); y += 9;

    snprintf(buf, sizeof(buf), "Tipo: %s", _scanType == 0 ? "BLE" : "Classic");
    d.drawText(1, y, buf, 1); y += 9;

    if (isPrinterName(dev.name)) {
        d.drawText(1, y, "Parece impressora!", 1); y += 9;
    }

    y = 43;
    d.drawHLine(0, y, 128);
    y += 3;

    const char* opts[2] = { "Conectar", "Voltar" };
    for (int i = 0; i < 2; i++) {
        String s = (i == _detailSel ? "> " : "  ") + String(opts[i]);
        d.drawText(1, y, s.c_str(), 1);
        y += 10;
    }

    d.show();
}

void BluetoothApp::drawConnecting(Display& d) {
    d.clear();
    GUI::drawMenuTitle(d, "Conectando", ICON_BLUETOOTH);

    int dots = (millis() / 400) % 4;
    char buf[22];
    snprintf(buf, sizeof(buf), "Conectando%s", String("...").substring(0, dots).c_str());
    d.drawCenteredText(28, buf, 1);
    d.drawCenteredText(42, _devices[_sel].name.c_str(), 1);
    d.show();
}

void BluetoothApp::drawConnected(Display& d) {
    d.clear();
    d.drawCenteredText(16, "Conectado!", 2);

    char buf[22];
    snprintf(buf, sizeof(buf), "%s", _devices[_sel].name.c_str());
    d.drawCenteredText(32, buf, 1);
    d.drawCenteredText(46, _connectMsg.c_str(), 1);

    d.show();
}

static int deviceAngle(const String& mac) {
    unsigned int h = 0;
    for (unsigned int i = 0; i < mac.length(); i++) {
        h = h * 31 + (unsigned char)mac[i];
    }
    return h % 360;
}

static float rssiToMeters(int rssi) {
    if (rssi >= 0) return 15;
    float d = pow(10, ((-59 - rssi) / 20.0));
    if (d < 0.3) d = 0.3;
    if (d > 15) d = 15;
    return d;
}

void BluetoothApp::drawRadar(Display& d) {
    d.clear();

    int cx = 64, cy = 34, maxR = 27;
    int sel = _radarSel;

    char buf[22];
    if (sel >= 0 && sel < _deviceCount) {
        snprintf(buf, sizeof(buf), "> %s", _devices[sel].name.c_str());
    } else {
        snprintf(buf, sizeof(buf), "Radar  [%d]", _deviceCount);
    }
    d.drawCenteredText(2, buf, 1);

    d.oled().drawCircle(cx, cy, maxR, SSD1306_WHITE);
    d.oled().drawCircle(cx, cy, maxR * 2 / 3, SSD1306_WHITE);
    d.oled().drawCircle(cx, cy, maxR / 3, SSD1306_WHITE);

    d.oled().drawLine(cx - maxR, cy, cx + maxR, cy, SSD1306_WHITE);
    d.oled().drawLine(cx, cy - maxR, cx, cy + maxR, SSD1306_WHITE);

    float rad = _radarAngle * PI / 180.0;
    int sx = cx + maxR * cos(rad);
    int sy = cy + maxR * sin(rad);
    d.oled().drawLine(cx, cy, sx, sy, SSD1306_WHITE);

    for (int i = 0; i < _deviceCount; i++) {
        if (_devices[i].rssi == 0) continue;
        float distM = rssiToMeters(_devices[i].rssi);
        int pr = (int)((distM / 15.0) * maxR);
        if (pr < 2) pr = 2;
        if (pr > maxR) pr = maxR;

        int a = deviceAngle(_devices[i].mac);
        float r2 = a * PI / 180.0;
        int dx = cx + pr * cos(r2);
        int dy = cy + pr * sin(r2);

        if (dx < 0 || dx >= 128 || dy < 0 || dy >= 64) continue;

        if (i == sel) {
            d.oled().fillCircle(dx, dy, 3, SSD1306_WHITE);
            d.oled().drawCircle(dx, dy, 4, SSD1306_WHITE);
        } else {
            d.oled().fillCircle(dx, dy, 2, SSD1306_WHITE);
        }
    }

    d.drawCenteredText(62, "Click=sel Hold=info", 1);
    d.show();
}
