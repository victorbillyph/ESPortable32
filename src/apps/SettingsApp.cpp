#include "SettingsApp.h"
#include "../icons/Icons.h"
#include "../core/Speaker.h"
#include "../core/Config.h"

extern Config config;

SettingsApp::SettingsApp(AppManager* mgr) : _mgr(mgr), _contrast(128),
    _spkPos(25), _spkNeg(26), _editPin(0), _editing(false) {}

void SettingsApp::init() {
    _spkPos = config.getInt("spk_pos", 25);
    _spkNeg = config.getInt("spk_neg", 26);
    _items[0] = "Contraste";
    _items[1] = "Animacao";
    _items[2] = "Alto-falante";
    _items[3] = "Reset Config";
    _items[4] = "Voltar";
    _icons[0] = ICON_SETTINGS;
    _icons[1] = ICON_CLOCK;
    _icons[2] = ICON_CPU;
    _icons[3] = ICON_ERROR;
    _icons[4] = ICON_BACK;
    _menu.setItems(_items, 5);
    _menu.setIcons(_icons);
    _menu.setTitle("Config");
    _menu.setTitleIcon(ICON_SETTINGS);
    _menu.reset();
    _editing = false;
}

void SettingsApp::update() {}

void SettingsApp::draw(Display& d) {
    if (_editing) drawSpeakerEdit(d);
    else _menu.draw(d);
}

void SettingsApp::buttonClick() {
    if (_editing) {
        _editPin++;
        if (_editPin > 2) _editPin = 0;
        return;
    }
    _menu.next();
}

void SettingsApp::buttonHold() {
    if (_editing) {
        if (_editPin == 2) {
            saveSpeakerPins();
            Speaker::setPins(_spkPos, _spkNeg);
        }
        _editing = false;
        _menu.reset();
        return;
    }
    switch (_menu.select()) {
        case 0:
            _contrast += 16;
            if (_contrast > 255) _contrast = 0;
            setContrast(_mgr->display(), _contrast);
            break;
        case 1:
            break;
        case 2:
            _editing = true;
            _editPin = 0;
            break;
        case 3:
            _mgr->display().clear();
            _mgr->display().drawCenteredText(28, "Reset pronto", 1);
            _mgr->display().show();
            delay(1000);
            break;
        case 4: _mgr->popApp(); break;
    }
}

void SettingsApp::buttonVeryLong() {
    if (_editing) {
        int* val = (_editPin == 0) ? &_spkPos : &_spkNeg;
        (*val)++;
        if (*val > 33) *val = -1;
        else if (*val == -1) *val = 8;
        return;
    }
}

void SettingsApp::buttonDoubleClick() {}

void SettingsApp::exit() {}

void SettingsApp::setContrast(Display& d, uint8_t val) {
    d.setContrast(val);
    d.clear();
    d.drawCenteredText(2, "Contraste", 1);
    d.drawProgressBar(14, 24, 100, 12, val * 100 / 255);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", val * 100 / 255);
    d.drawCenteredText(42, buf, 1);
    d.drawCenteredText(56, "Segure p/ continuar", 1);
    d.show();
    delay(300);
}

void SettingsApp::drawSpeakerEdit(Display& d) {
    d.clear();
    d.drawCenteredText(2, "Alto-falante", 1);
    d.drawHLine(0, 11, 128);
    const char* labels[] = { "Positivo", "Negativo", "Salvar" };
    int vals[] = { _spkPos, _spkNeg, 0 };
    int y = 16;
    for (int i = 0; i < 3; i++) {
        char buf[22];
        if (i < 2) {
            snprintf(buf, 21, "%s %s GPIO %d",
                     i == _editPin ? ">" : " ", labels[i],
                     vals[i] < 0 ? -1 : vals[i]);
        } else {
            snprintf(buf, 21, "%s %s",
                     i == _editPin ? ">" : " ", labels[i]);
        }
        d.drawText(1, y, buf, 1); y += 10;
    }
    d.drawCenteredText(55, "VLong=muda Hold=sai", 1);
    d.show();
}

void SettingsApp::saveSpeakerPins() {
    config.setInt("spk_pos", _spkPos);
    config.setInt("spk_neg", _spkNeg);
}
