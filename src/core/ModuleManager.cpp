#include "ModuleManager.h"
#include "Display.h"

static int charW = 6;
static int tw(const String& s) { return s.length() * charW; }

void ModuleManager::parseScript(int idx, const String& cmds) {
    _state[idx].scriptLineCount = 0;
    int pos = 0;
    int len = cmds.length();
    while (pos < len && _state[idx].scriptLineCount < 32) {
        int nl = cmds.indexOf('\n', pos);
        if (nl < 0) {
            _state[idx].scriptLines[_state[idx].scriptLineCount++] = cmds.substring(pos);
            break;
        }
        _state[idx].scriptLines[_state[idx].scriptLineCount++] = cmds.substring(pos, nl);
        pos = nl + 1;
    }
}

ModuleManager::ModuleManager() : _count(0) {
    for (int i = 0; i < MAX_MODULES; i++) {
        _state[i] = RunState();
        _scrTxt[i] = String();
        _scrScroll[i] = String();
    }
}

void ModuleManager::begin() {
    _prefs.begin(_ns, false);
    _count = _prefs.getInt("count", 0);
    if (_count > MAX_MODULES) _count = MAX_MODULES;
    for (int i = 0; i < _count; i++) {
        _state[i].active = true;
        String idx = String(i);
        String type = _prefs.getString(("t_" + idx).c_str(), "");
        if (type == "script") {
            String c1 = _prefs.getString(("c1_" + idx).c_str(), "");
            parseScript(i, c1);
        }
    }
}

ModuleDef ModuleManager::get(int idx) {
    ModuleDef mod;
    if (idx < 0 || idx >= _count) return mod;
    String i = String(idx);
    mod.id = "mod_" + i;
    mod.type = _prefs.getString(("t_" + i).c_str(), "");
    mod.name = _prefs.getString(("n_" + i).c_str(), "");
    mod.enabled = _prefs.getInt(("e_" + i).c_str(), 0) != 0;
    mod.cfg1 = _prefs.getString(("c1_" + i).c_str(), "");
    mod.cfg2 = _prefs.getString(("c2_" + i).c_str(), "");
    mod.cfg3 = _prefs.getString(("c3_" + i).c_str(), "");
    return mod;
}

bool ModuleManager::add(const String& type, const String& name, bool enabled, const String& c1, const String& c2, const String& c3) {
    if (_count >= MAX_MODULES) return false;
    int i = _count;
    String idx = String(i);
    _prefs.putString(("t_" + idx).c_str(), type);
    _prefs.putString(("n_" + idx).c_str(), name);
    _prefs.putInt(("e_" + idx).c_str(), enabled ? 1 : 0);
    _prefs.putString(("c1_" + idx).c_str(), c1);
    _prefs.putString(("c2_" + idx).c_str(), c2);
    _prefs.putString(("c3_" + idx).c_str(), c3);
    _count++;
    saveCount();
    _state[i] = RunState();
    _state[i].active = true;
    _scrTxt[i] = String();
    _scrScroll[i] = String();
    if (type == "script") {
        parseScript(i, c1);
    }
    return true;
}

bool ModuleManager::update(const String& id, const String& name, bool enabled, const String& c1, const String& c2, const String& c3) {
    for (int i = 0; i < _count; i++) {
        if (("mod_" + String(i)) == id) {
            String idx = String(i);
            _prefs.putString(("n_" + idx).c_str(), name);
            _prefs.putInt(("e_" + idx).c_str(), enabled ? 1 : 0);
            _prefs.putString(("c1_" + idx).c_str(), c1);
            _prefs.putString(("c2_" + idx).c_str(), c2);
            _prefs.putString(("c3_" + idx).c_str(), c3);
            _state[i] = RunState();
            _state[i].active = true;
            _scrTxt[i] = String();
            _scrScroll[i] = String();
            ModuleDef mod = get(i);
            if (mod.type == "script") {
                parseScript(i, c1);
            }
            return true;
        }
    }
    return false;
}

bool ModuleManager::remove(const String& id) {
    int found = -1;
    for (int i = 0; i < _count; i++) {
        if (("mod_" + String(i)) == id) {
            found = i;
            break;
        }
    }
    if (found < 0) return false;
    for (int i = found; i < _count - 1; i++) {
        String src = String(i + 1);
        String dst = String(i);
        _prefs.putString(("t_" + dst).c_str(), _prefs.getString(("t_" + src).c_str(), ""));
        _prefs.putString(("n_" + dst).c_str(), _prefs.getString(("n_" + src).c_str(), ""));
        _prefs.putInt(("e_" + dst).c_str(), _prefs.getInt(("e_" + src).c_str(), 0));
        _prefs.putString(("c1_" + dst).c_str(), _prefs.getString(("c1_" + src).c_str(), ""));
        _prefs.putString(("c2_" + dst).c_str(), _prefs.getString(("c2_" + src).c_str(), ""));
        _prefs.putString(("c3_" + dst).c_str(), _prefs.getString(("c3_" + src).c_str(), ""));
        _state[i] = _state[i + 1];
        _scrTxt[i] = _scrTxt[i + 1];
        _scrScroll[i] = _scrScroll[i + 1];
    }
    int last = _count - 1;
    String lst = String(last);
    _prefs.remove(("t_" + lst).c_str());
    _prefs.remove(("n_" + lst).c_str());
    _prefs.remove(("e_" + lst).c_str());
    _prefs.remove(("c1_" + lst).c_str());
    _prefs.remove(("c2_" + lst).c_str());
    _prefs.remove(("c3_" + lst).c_str());
    _count--;
    saveCount();
    return true;
}

ModuleDef ModuleManager::getById(const String& id) {
    for (int i = 0; i < _count; i++) {
        if (("mod_" + String(i)) == id) {
            return get(i);
        }
    }
    return ModuleDef();
}

void ModuleManager::saveCount() {
    _prefs.putInt("count", _count);
}

int ModuleManager::getCfgInt(const String& val, int def) {
    if (val.length() == 0) return def;
    return val.toInt();
}

void ModuleManager::update(unsigned long now) {
    for (int i = 0; i < _count; i++) {
        if (!_state[i].active) continue;
        ModuleDef mod = get(i);
        if (!mod.enabled) continue;
        if (mod.type == "gpio_blink") {
            execGpio(i, mod, now);
        } else if (mod.type == "scrolling_text") {
            execScrolling(i, mod, now);
        } else if (mod.type == "script") {
            execScript(i, mod, now);
        }
    }
}

void ModuleManager::execGpio(int idx, ModuleDef& mod, unsigned long now) {
    int pin = getCfgInt(mod.cfg1, -1);
    int interval = getCfgInt(mod.cfg2, 500);
    if (pin < 0) return;
    if (now - _state[idx].gpioTimer >= (unsigned long)interval) {
        _state[idx].gpioTimer = now;
        _state[idx].gpioOn = !_state[idx].gpioOn;
        digitalWrite(pin, _state[idx].gpioOn ? HIGH : LOW);
    }
}

void ModuleManager::execScrolling(int idx, ModuleDef& mod, unsigned long now) {
    int speed = getCfgInt(mod.cfg3, 100);
    if (now - _state[idx].scrollTimer >= (unsigned long)speed) {
        _state[idx].scrollTimer = now;
        _state[idx].scrollPos++;
    }
}

void ModuleManager::execScript(int idx, ModuleDef& mod, unsigned long now) {
    RunState& st = _state[idx];
    if (st.scriptWaiting) {
        if (now - st.scriptTimer >= (unsigned long)st.scriptDelay) {
            st.scriptWaiting = false;
            st.scriptPC++;
        }
        return;
    }
    if (st.scriptPC >= st.scriptLineCount) {
        st.scriptPC = 0;
        st.loopSP = 0;
        _scrTxt[idx] = String();
        _scrScroll[idx] = String();
        return;
    }
    String cmd = st.scriptLines[st.scriptPC];
    cmd.trim();
    if (cmd.length() == 0) {
        st.scriptPC++;
        return;
    }
    if (cmd.startsWith("GPIO ")) {
        String rest = cmd.substring(5);
        rest.trim();
        int sp = rest.indexOf(' ');
        if (sp > 0) {
            int pin = rest.substring(0, sp).toInt();
            String val = rest.substring(sp + 1);
            val.trim();
            digitalWrite(pin, val.equalsIgnoreCase("ON") ? HIGH : LOW);
        }
        st.scriptPC++;
    } else if (cmd.startsWith("DELAY ")) {
        int ms = cmd.substring(6).toInt();
        if (ms > 0) {
            st.scriptWaiting = true;
            st.scriptDelay = ms;
            st.scriptTimer = now;
        } else {
            st.scriptPC++;
        }
    } else if (cmd.startsWith("TEXT ")) {
        _scrTxt[idx] = cmd.substring(5);
        _scrTxt[idx].trim();
        st.scriptPC++;
    } else if (cmd.startsWith("SCROLL ")) {
        _scrScroll[idx] = cmd.substring(7);
        _scrScroll[idx].trim();
        st.scrollPos = 0;
        st.scriptPC++;
    } else if (cmd.startsWith("LOOP ")) {
        int n = cmd.substring(5).toInt();
        if (n < 0) n = 0;
        st.loopStack[st.loopSP] = st.scriptPC;
        _loopCnt[idx][st.loopSP] = n;
        st.loopSP++;
        st.scriptPC++;
    } else if (cmd.equalsIgnoreCase("END")) {
        if (st.loopSP > 0) {
            int sp = st.loopSP - 1;
            _loopCnt[idx][sp]--;
            if (_loopCnt[idx][sp] > 0) {
                st.scriptPC = st.loopStack[sp] + 1;
            } else {
                st.loopSP--;
                st.scriptPC++;
            }
        } else {
            st.scriptPC++;
        }
    } else {
        st.scriptPC++;
    }
}

void ModuleManager::drawOverlay(Display& d) {
    for (int i = 0; i < _count; i++) {
        if (!_state[i].active) continue;
        ModuleDef mod = get(i);
        if (!mod.enabled) continue;
        if (mod.type == "scrolling_text") {
            String t = mod.cfg1;
            if (t.length() == 0) continue;
            int y = getCfgInt(mod.cfg2, 0);
            if (y < 0 || y >= 64) continue;
            int w = tw(t);
            int off = _state[i].scrollPos % (w + 128);
            int x = 128 - off;
            d.drawText(x, y, t.c_str(), 1);
            if (x + w < 128) {
                d.drawText(x + w + 6, y, t.c_str(), 1);
            }
        } else if (mod.type == "oled_text") {
            String t = mod.cfg1;
            if (t.length() == 0) continue;
            int x = getCfgInt(mod.cfg2, 0);
            int y = getCfgInt(mod.cfg3, 0);
            d.drawText(x, y, t.c_str(), 1);
        } else if (mod.type == "script") {
            if (_scrTxt[i].length() > 0) {
                d.drawCenteredText(0, _scrTxt[i].c_str(), 1);
            }
            if (_scrScroll[i].length() > 0) {
                String t = _scrScroll[i];
                int w = tw(t);
                int off = _state[i].scrollPos % (w + 128);
                int x = 128 - off;
                d.drawText(x, 54, t.c_str(), 1);
                if (x + w < 128) {
                    d.drawText(x + w + 6, 54, t.c_str(), 1);
                }
            }
        }
    }
}
