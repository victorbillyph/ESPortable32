#ifndef CORE_WEB_MANAGER_H
#define CORE_WEB_MANAGER_H

#include <Arduino.h>
#include <WebServer.h>
#include "Config.h"
#include "ModuleManager.h"
#include "PrinterManager.h"
#include "MediaBuffer.h"

class WebManager {
public:
    WebManager(Config* cfg, ModuleManager* modMgr, PrinterManager* printerMgr);
    void begin();
    void update();
    bool isRunning() { return _running; }

private:
    WebServer _server;
    Config* _config;
    ModuleManager* _modules;
    PrinterManager* _printers;
    bool _running;

    void handleRoot();
    void handleAPI();
    String buildDashboard();
    String statusJSON();
    String configJSON();
    String modulesJSON();
    String printersJSON();
    String alarmsJSON();

    String urlDecode(const String& str);

    void handleUpload();
    void handleUploadFile();
    size_t _uploadSize;
    char _uploadName[32];
    bool _uploading;
};

#endif
