#ifndef CORE_MODULE_MANAGER_H
#define CORE_MODULE_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

class Display;

#define MAX_MODULES 10

struct ModuleDef {
    String id;
    String type;      // "scrolling_text", "gpio_blink", "oled_text", "script"
    String name;
    bool enabled;
    String cfg1, cfg2, cfg3;  // type-specific config fields
};

class ModuleManager {
public:
    ModuleManager();
    void begin();
    void update(unsigned long now);
    void drawOverlay(Display& d);  // draw scroll/text modules on screen

    int count() { return _count; }
    ModuleDef get(int idx);
    bool add(const String& type, const String& name, bool enabled, const String& c1, const String& c2, const String& c3);
    bool update(const String& id, const String& name, bool enabled, const String& c1, const String& c2, const String& c3);
    bool remove(const String& id);
    ModuleDef getById(const String& id);

private:
    Preferences _prefs;
    const char* _ns = "esp32mod";
    int _count;

    struct RunState {
        bool active = false;
        // gpio blink
        bool gpioOn = false;
        unsigned long gpioTimer = 0;
        // scrolling text
        int scrollPos = 0;
        unsigned long scrollTimer = 0;
        // script
        int scriptPC = 0;
        bool scriptWaiting = false;
        unsigned long scriptTimer = 0;
        int scriptDelay = 0;
        int loopStack[8];
        int loopSP = 0;
        String scriptLines[32];
        int scriptLineCount = 0;
    };
    RunState _state[MAX_MODULES];

    void parseScript(int idx, const String& cmds);
    void saveCount();
    void execGpio(int idx, ModuleDef& mod, unsigned long now);
    void execScrolling(int idx, ModuleDef& mod, unsigned long now);
    void execScript(int idx, ModuleDef& mod, unsigned long now);
    int getCfgInt(const String& val, int def);

    String _scrTxt[MAX_MODULES];
    String _scrScroll[MAX_MODULES];
    int _loopCnt[MAX_MODULES][8];
};

#endif
