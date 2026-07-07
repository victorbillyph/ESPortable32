#include "PrintersApp.h"
#include "../icons/Icons.h"
#include "../core/GUI.h"

PrintersApp::PrintersApp(AppManager* mgr, PrinterManager* printerMgr)
    : _mgr(mgr), _pm(printerMgr), _mode(MENU), _sel(0) {}

void PrintersApp::init() {
    _items[0] = "Scan Network";
    _items[1] = "Impressoras";
    _items[2] = "Voltar";
    _icons[0] = ICON_WIFI;
    _icons[1] = ICON_PRINTER;
    _icons[2] = ICON_BACK;
    _menu.setItems(_items, 3);
    _menu.setIcons(_icons);
    _menu.setTitle("Printer");
    _menu.setTitleIcon(ICON_PRINTER);
    _menu.reset();
    _mode = MENU;
}

void PrintersApp::update() {}

void PrintersApp::draw(Display& d) {
    switch (_mode) {
        case MENU: _menu.draw(d); break;
        case LIST: drawList(d); break;
        case DETAIL: drawDetail(d); break;
    }
}

void PrintersApp::buttonClick() {
    switch (_mode) {
        case MENU:
            _menu.next();
            break;
        case LIST: {
            int cnt = _pm->printerCount();
            _sel = (_sel + 1) % (cnt > 0 ? cnt : 1);
            break;
        }
        case DETAIL:
            if (_sel < _pm->printerCount()) {
                _pm->printText(_sel, "ESPortable32 - Pagina de Teste\n\n"
                                     "Impressao de teste.\n\n");
            }
            break;
    }
}

void PrintersApp::buttonHold() {
    switch (_mode) {
        case MENU:
            switch (_menu.select()) {
                case 0:
                    _pm->startScan();
                    break;
                case 1:
                    _sel = 0;
                    _mode = LIST;
                    break;
                case 2:
                    _mgr->popApp();
                    break;
            }
            break;
        case LIST:
            if (_pm->printerCount() > 0) _mode = DETAIL;
            break;
        case DETAIL:
            _mode = LIST;
            break;
    }
}

void PrintersApp::buttonVeryLong() { _mode = MENU; }

void PrintersApp::buttonDoubleClick() { _mode = MENU; }

void PrintersApp::exit() {}

void PrintersApp::drawList(Display& d) {
    d.clear();
    GUI::drawMenuTitle(d, "Impressoras", ICON_PRINTER);
    int cnt = _pm->printerCount();
    char buf[22];
    if (cnt == 0) {
        d.drawCenteredText(28, "Nenhuma", 1);
        d.drawCenteredText(52, "Segure p/ voltar", 1);
    } else {
        int y = 13;
        for (int i = 0; i < cnt; i++) {
            Printer p = _pm->getPrinter(i);
            snprintf(buf, sizeof(buf), "%s %s", i == _sel ? ">" : " ", p.name.c_str());
            d.drawText(1, y, buf, 1);
            y += 10;
        }
    }
    d.show();
}

void PrintersApp::drawDetail(Display& d) {
    d.clear();
    if (_sel >= _pm->printerCount()) { _mode = LIST; return; }
    Printer p = _pm->getPrinter(_sel);
    char buf[24];
    int y = 2;
    snprintf(buf, sizeof(buf), "%s", p.name.c_str());
    d.drawText(1, y, buf, 1); y += 10;
    if (p.isBluetooth) {
        snprintf(buf, sizeof(buf), "MAC: %s", p.mac.c_str());
    } else {
        snprintf(buf, sizeof(buf), "IP: %s", p.ip.c_str());
    }
    d.drawText(1, y, buf, 1); y += 10;
    snprintf(buf, sizeof(buf), "Porta: %d", p.port);
    d.drawText(1, y, buf, 1); y += 10;
    snprintf(buf, sizeof(buf), "Status: %s", p.reachable ? "OK" : "Offline");
    d.drawText(1, y, buf, 1);
    d.drawCenteredText(54, "Click=testar  Hold=voltar", 1);
    d.show();
}
