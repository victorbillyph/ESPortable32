#include "RelogioApp.h"
#include "../icons/Icons.h"
#include "../core/GUI.h"
#include "../core/State.h"

RelogioApp::RelogioApp(AppManager* mgr) : _mgr(mgr), _mode(MENU), _sel(0),
    _editIdx(0), _scrollPos(0), _count(0), _alarmFired(false), _firedIdx(0),
    _lastFH(-1), _lastFM(-1), _snoozed(false), _snoozeH(0), _snoozeM(0),
    _alarmStart(0), _lastBeep(0), _beepOn(false) {
    memset(_alarms, 0, sizeof(_alarms));
}

void RelogioApp::init() {
    load();
    if (_alarmFired) {
        _mode = ALARMING;
        _alarmStart = millis();
        _lastBeep = 0;
        _beepOn = false;
    } else {
        _items[0] = "Ver Alarmes";
        _items[1] = "Adicionar";
        _items[2] = "Voltar";
        _icons[0] = ICON_CLOCK;
        _icons[1] = ICON_OK;
        _icons[2] = ICON_BACK;
        _menu.setItems(_items, 3);
        _menu.setIcons(_icons);
        _menu.setTitle("Relogio");
        _menu.setTitleIcon(ICON_CLOCK);
        _menu.reset();
        _mode = MENU;
    }
    _sel = 0;
}

void RelogioApp::update() {
    if (_mode == ALARMING && _alarmFired) {
        unsigned long now = millis();
        if (now - _lastBeep > 300) {
            _lastBeep = now;
            _beepOn = !_beepOn;
            if (_beepOn && Speaker::enabled()) Speaker::toneStart(880);
            else Speaker::toneStop();
        }
    }
}

void RelogioApp::draw(Display& d) {
    switch (_mode) {
        case MENU: _menu.draw(d); break;
        case LIST: drawList(d); break;
        case EDIT_HOUR:
        case EDIT_MINUTE:
        case EDIT_DAYS: drawEditing(d); break;
        case ALARMING: drawAlarming(d); break;
    }
}

void RelogioApp::buttonClick() {
    switch (_mode) {
        case MENU:
            _menu.next();
            break;
        case LIST:
            if (_count > 0) {
                _alarms[_sel].enabled = !_alarms[_sel].enabled;
                save();
            }
            break;
        case EDIT_HOUR:
            _alarms[_editIdx].hour = _scrollPos;
            _mode = EDIT_MINUTE;
            _scrollPos = _alarms[_editIdx].minute;
            break;
        case EDIT_MINUTE:
            _alarms[_editIdx].minute = _scrollPos;
            _mode = EDIT_DAYS;
            _scrollPos = _alarms[_editIdx].days;
            break;
        case EDIT_DAYS:
            _scrollPos ^= (1 << _sel);
            break;
        case ALARMING:
            snoozeAlarm();
            break;
    }
}

void RelogioApp::buttonHold() {
    switch (_mode) {
        case MENU:
            switch (_menu.select()) {
                case 0: _mode = LIST; _sel = 0; break;
                case 1:
                    if (_count < MAX_ALARMS) {
                        _alarms[_count].hour = 7;
                        _alarms[_count].minute = 0;
                        _alarms[_count].days = 0x7F;
                        _alarms[_count].enabled = true;
                        _editIdx = _count;
                        _count++;
                        _mode = EDIT_HOUR;
                        _scrollPos = 7;
                    }
                    break;
                case 2: _mgr->popApp(); break;
            }
            break;
        case LIST:
            if (_count > 0) {
                _editIdx = _sel;
                _mode = EDIT_HOUR;
                _scrollPos = _alarms[_editIdx].hour;
            }
            break;
        case EDIT_DAYS:
            save();
            _mode = LIST;
            break;
        case ALARMING:
            dismissAlarm();
            break;
    }
}

void RelogioApp::buttonVeryLong() {
    if (_mode == ALARMING) dismissAlarm();
    else { Speaker::toneStop(); _mode = MENU; _menu.reset(); }
}

void RelogioApp::buttonDoubleClick() {
    if (_mode == ALARMING) dismissAlarm();
    else _mode = MENU;
}

void RelogioApp::exit() {
    Speaker::toneStop();
}

// ── Alarm trigger ─────────────────────────────────────

void RelogioApp::tickAlarm(int h, int m, int wday) {
    if (_alarmFired) return;

    if (_snoozed) {
        if (h == _snoozeH && m == _snoozeM) {
            _alarmFired = true;
            _snoozed = false;
            _mode = ALARMING;
            _alarmStart = millis();
            _lastBeep = 0;
            _beepOn = false;
        }
        return;
    }

    for (int i = 0; i < _count; i++) {
        if (!_alarms[i].enabled) continue;
        if (_alarms[i].hour != h || _alarms[i].minute != m) continue;
        if (!(_alarms[i].days & (1 << wday))) continue;
        if (h == _lastFH && m == _lastFM) continue;

        _lastFH = h;
        _lastFM = m;
        _alarmFired = true;
        _firedIdx = i;
        return;
    }
}

void RelogioApp::dismissAlarm() {
    _alarmFired = false;
    _snoozed = false;
    Speaker::toneStop();
    _mode = MENU;
    _menu.reset();
}

void RelogioApp::snoozeAlarm() {
    _alarmFired = false;
    Speaker::toneStop();
    int totalMin = _lastFH * 60 + _lastFM + 5;
    _snoozeH = (totalMin / 60) % 24;
    _snoozeM = totalMin % 60;
    _snoozed = true;
    _mode = MENU;
    _menu.reset();
}

// ── Persistence ───────────────────────────────────────

void RelogioApp::load() {
    _prefs.begin("esp32alr", true);
    _count = _prefs.getUChar("count", 0);
    if (_count > MAX_ALARMS) _count = MAX_ALARMS;
    for (int i = 0; i < _count; i++) {
        char k[4];
        snprintf(k, 4, "h%c", '0' + i);
        _alarms[i].hour = _prefs.getUChar(k, 0);
        snprintf(k, 4, "m%c", '0' + i);
        _alarms[i].minute = _prefs.getUChar(k, 0);
        snprintf(k, 4, "d%c", '0' + i);
        _alarms[i].days = _prefs.getUChar(k, 0);
        snprintf(k, 4, "e%c", '0' + i);
        _alarms[i].enabled = _prefs.getBool(k, true);
    }
    _prefs.end();
}

void RelogioApp::save() {
    _prefs.begin("esp32alr", false);
    _prefs.putUChar("count", _count);
    for (int i = 0; i < _count; i++) {
        char k[4];
        snprintf(k, 4, "h%c", '0' + i);
        _prefs.putUChar(k, _alarms[i].hour);
        snprintf(k, 4, "m%c", '0' + i);
        _prefs.putUChar(k, _alarms[i].minute);
        snprintf(k, 4, "d%c", '0' + i);
        _prefs.putUChar(k, _alarms[i].days);
        snprintf(k, 4, "e%c", '0' + i);
        _prefs.putBool(k, _alarms[i].enabled);
    }
    _prefs.end();
}

// ── Drawing ───────────────────────────────────────────

static const char* DAY_NAMES[] = { "Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sab" };

void RelogioApp::drawList(Display& d) {
    d.clear();
    GUI::drawMenuTitle(d, "Alarmes", ICON_CLOCK);
    int y = 14;
    if (_count == 0) {
        d.drawCenteredText(30, "Nenhum alarme", 1);
        d.drawCenteredText(44, "Adicionar >", 1);
        d.show();
        return;
    }
    for (int i = 0; i < _count; i++) {
        char buf[22];
        AlarmData& a = _alarms[i];
        snprintf(buf, 21, "%s%02d:%02d %s",
                 i == _sel ? ">" : " ", a.hour, a.minute,
                 a.enabled ? "" : "(des)");
        d.drawText(1, y, buf, 1); y += 9;
        char days[14] = {};
        int p = 0;
        for (int j = 0; j < 7; j++) {
            if (a.days & (1 << j)) { days[p++] = DAY_NAMES[j][0]; days[p++] = ' '; }
        }
        if (p > 0) days[p - 1] = 0;
        if (days[0]) { d.drawText(15, y, days, 1); y += 9; }
    }
    d.drawCenteredText(57, "Click=toggle Hold=editar", 1);
    d.show();
}

void RelogioApp::drawEditing(Display& d) {
    d.clear();
    AlarmData& a = _alarms[_editIdx];

    if (_mode == EDIT_HOUR) {
        d.drawCenteredText(2, "Hora", 1);
        char buf[4];
        snprintf(buf, 3, "%02d", _scrollPos);
        d.oled().setTextSize(3);
        d.oled().setTextColor(SSD1306_WHITE);
        int16_t x1, y1; uint16_t w, h;
        d.oled().getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
        d.oled().setCursor((128 - w) / 2, 18);
        d.oled().print(buf);
        d.drawCenteredText(52, "Click=confirma", 1);
    } else if (_mode == EDIT_MINUTE) {
        d.drawCenteredText(2, "Minuto", 1);
        char buf[4];
        snprintf(buf, 3, "%02d", _scrollPos);
        d.oled().setTextSize(3);
        d.oled().setTextColor(SSD1306_WHITE);
        int16_t x1, y1; uint16_t w, h;
        d.oled().getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
        d.oled().setCursor((128 - w) / 2, 18);
        d.oled().print(buf);
        d.drawCenteredText(52, "Click=confirma", 1);
    } else if (_mode == EDIT_DAYS) {
        d.drawCenteredText(2, "Dias da Semana", 1);
        d.drawHLine(0, 11, 128);
        int x = 2;
        for (int i = 0; i < 7; i++) {
            bool on = _scrollPos & (1 << i);
            if (i == _sel) d.drawFrame(x - 1, 15, 18, 11);
            d.drawText(x, 17, on ? DAY_NAMES[i] : "--", 1);
            x += 18;
        }
        d.drawCenteredText(40, "Click=toggle", 1);
        d.drawCenteredText(52, "Hold=salvar", 1);
    }

    d.show();
}

void RelogioApp::drawAlarming(Display& d) {
    d.clear();
    d.drawCenteredText(10, "ALARME!", 2);
    d.drawHLine(0, 28, 128);
    AlarmData& a = _alarms[_firedIdx];
    char buf[22];
    snprintf(buf, 21, "%02d:%02d", a.hour, a.minute);
    d.drawCenteredText(32, buf, 1);
    d.drawCenteredText(44, "Click=soneca", 1);
    d.drawCenteredText(54, "Hold=dismiss", 1);
    d.show();
}
