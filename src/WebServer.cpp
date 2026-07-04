#include "WebServer.h"
#include <WiFi.h>
#include <ArduinoJson.h>

WebServer::WebServer(StateManager& state, ConfigManager& config, WebSockets& ws)
    : _server(80), _state(state), _config(config), _ws(ws), _captive(false) {}

void WebServer::begin() {
    _server.onNotFound([this](AsyncWebServerRequest* request) {
        this->handleRequest(request);
    });

    _server.on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (_state.getState() < STATE_READY) {
            handleCaptive(request);
        } else {
            handleFileRequest(request);
        }
    });

    _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleApiStatus(request);
    });

    _server.on("/api/unlock", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleApiUnlock(request);
    });

    _server.on("/api/unlock", HTTP_OPTIONS, [this](AsyncWebServerRequest* request) {
        request->send(204);
    });

    _server.on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleApiRestart(request);
    });

    _server.on("/api/restart", HTTP_OPTIONS, [this](AsyncWebServerRequest* request) {
        request->send(204);
    });

    _server.on("/api/gpio", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleApiGpioList(request);
    });
    _server.on("/api/gpio", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleApiGpioSet(request);
    });
    _server.on("/api/gpio", HTTP_OPTIONS, [this](AsyncWebServerRequest* request) {
        request->send(204);
    });

    _server.on("/api/fs/list", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleApiFsList(request);
    });
    _server.on("/api/fs/read", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleApiFsRead(request);
    });
    _server.on("/api/fs/write", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleApiFsWrite(request);
    });
    _server.on("/api/fs/write", HTTP_OPTIONS, [this](AsyncWebServerRequest* request) {
        request->send(204);
    });
    _server.on("/api/fs/delete", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
        handleApiFsDelete(request);
    });
    _server.on("/api/fs/delete", HTTP_OPTIONS, [this](AsyncWebServerRequest* request) {
        request->send(204);
    });

    _server.on("/api/apps", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleApiApps(request);
    });
    _server.on("/api/apps", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleApiStoreInstall(request);
    });
    _server.on("/api/apps", HTTP_OPTIONS, [this](AsyncWebServerRequest* request) {
        request->send(204);
    });

    _server.on("/api/proxy", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleApiProxy(request);
    });
    _server.on("/api/proxy", HTTP_OPTIONS, [this](AsyncWebServerRequest* request) {
        request->send(204);
    });

    _server.on("/api/setup", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleApiSetup(request);
    });

    _server.begin();
    Serial.println("[Server] HTTP server started on port 80");
}

void WebServer::handleClient() {}

bool WebServer::isCaptive() { return _captive; }

void WebServer::handleRequest(AsyncWebServerRequest* request) {
    String url = request->url();

    if (_state.getState() < STATE_READY) {
        handleCaptive(request);
        return;
    }

    if (_state.isLocked() && !url.startsWith("/unlock") && !url.startsWith("/api") && !url.startsWith("/css") && !url.startsWith("/js") && !url.startsWith("/apps")) {
        request->redirect("/unlock.html");
        return;
    }

    if (url.startsWith("/api/")) {
        return;
    }

    handleFileRequest(request);
}

void WebServer::handleFileRequest(AsyncWebServerRequest* request) {
    String path = request->url();
    if (path == "/") path = "/index.html";

    String contentType = getContentType(path);

    if (LittleFS.exists(path)) {
        File f = LittleFS.open(path, "r");
        if (f) {
            String content = f.readString();
            f.close();
            AsyncWebServerResponse *response = request->beginResponse(200, contentType, content);
            response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
            request->send(response);
        } else {
            request->send(500, "text/plain", "Error reading: " + path);
        }
    } else {
        request->send(404, "text/plain", "Not found: " + path);
    }
}

void WebServer::handleCaptive(AsyncWebServerRequest* request) {
    _captive = true;
    String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
    <title>ESPortable32 - Setup</title>
    <style>
    body{font-family:sans-serif;background:#1a1a2e;color:#fff;display:flex;justify-content:center;align-items:center;height:100vh;margin:0}
    .card{background:#16213e;padding:2rem;border-radius:12px;box-shadow:0 4px 20px rgba(0,0,0,.4);max-width:400px;width:90%}
    h1{text-align:center;color:#e94560;margin-bottom:1.5rem}
    .form-group{margin-bottom:1rem}
    label{display:block;margin-bottom:.3rem;color:#aaa;font-size:.9rem}
    input{width:100%;padding:.6rem;border:1px solid #333;border-radius:6px;background:#0f3460;color:#fff;font-size:1rem;box-sizing:border-box}
    button{width:100%;padding:.7rem;background:#e94560;color:#fff;border:none;border-radius:6px;font-size:1rem;cursor:pointer;margin-top:.5rem}
    button:hover{background:#c73650}
    .info{text-align:center;color:#aaa;font-size:.8rem;margin-top:1rem}
    .ssid-list{background:#0f3460;border-radius:6px;padding:.5rem;margin-bottom:.5rem;max-height:150px;overflow-y:auto}
    .ssid-item{padding:.3rem .5rem;cursor:pointer;border-radius:4px}
    .ssid-item:hover{background:#1a5276}
    </style>
    </head><body>
    <div class="card">
    <h1>ESPortable32</h1>
    <div class="form-group">
    <label>Rede WiFi</label>
    <div class="ssid-list" id="ssidList"></div>
    <input type="text" id="ssid" placeholder="SSID">
    </div>
    <div class="form-group">
    <label>Senha</label>
    <input type="password" id="password" placeholder="Senha">
    </div>
    <div class="form-group">
    <label>PIN de bloqueio</label>
    <input type="password" id="pin" placeholder="PIN (opcional)">
    </div>
    <button onclick="save()">Salvar e Conectar</button>
    <div class="info" id="status"></div>
    </div>
    <script>
    fetch('/api/status').then(r=>r.json()).then(d=>{
    if(d.wifi_networks) {
    const list = document.getElementById('ssidList');
    d.wifi_networks.forEach(s=>{
    const div = document.createElement('div');
    div.className='ssid-item';
    div.textContent=s;
    div.onclick=()=>document.getElementById('ssid').value=s;
    list.appendChild(div);
    })
    }
    });
    function save(){
    const ssid=document.getElementById('ssid').value;
    const pass=document.getElementById('password').value;
    const pin=document.getElementById('pin').value;
    const st=document.getElementById('status');
    st.textContent='Salvando...';
    fetch('/api/setup',{method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass)+'&pin='+encodeURIComponent(pin)
    }).then(r=>r.text()).then(t=>{st.textContent=t;if(t.includes('OK'))setTimeout(()=>location.reload(),3000)})
    }
    </script>
    </body></html>
    )rawliteral";
    request->send(200, "text/html", html);
}

void WebServer::handleApiStatus(AsyncWebServerRequest* request) {
    String json;
    json += "{";
    json += "\"status\":\"ok\",";
    json += "\"state\":" + String(_state.getState()) + ",";
    json += "\"state_name\":\"" + String(_state.getStateName()) + "\",";
    json += "\"locked\":" + String(_state.isLocked() ? "true" : "false") + ",";
    json += "\"uptime\":" + String(millis() / 1000) + ",";
    json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"total_heap\":" + String(ESP.getHeapSize()) + ",";
    json += "\"cpu_freq\":" + String(ESP.getCpuFreqMHz()) + ",";
    json += "\"wifi_mode\":" + String(WiFi.getMode()) + ",";
    json += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"ssid\":\"" + _config.getWifiSSID() + "\",";
    json += "\"device_name\":\"" + _config.getDeviceName() + "\",";
    json += "\"version\":\"1.0.0\"";

    // Add WiFi scan results when in setup mode
    if (_state.getState() < STATE_CONNECTED) {
        int n = WiFi.scanComplete();
        if (n >= 0) {
            json += ",\"wifi_networks\":[";
            for (int i = 0; i < n; i++) {
                if (i > 0) json += ",";
                json += "\"" + WiFi.SSID(i) + "\"";
            }
            json += "]";
        }
        // Start async scan if not already scanning
        if (n == -1) {
            WiFi.scanNetworks(true);
        } else if (n == -2) {
            WiFi.scanNetworks(true);
        }
    }

    json += "}";
    request->send(200, "application/json", json);
}

void WebServer::handleApiUnlock(AsyncWebServerRequest* request) {
    String pin = request->arg("pin");
    if (_state.checkPin(pin)) {
        _state.setLocked(false);
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Unlocked\"}");
    } else {
        request->send(401, "application/json", "{\"status\":\"error\",\"message\":\"Invalid PIN\"}");
    }
}

void WebServer::handleApiRestart(AsyncWebServerRequest* request) {
    request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Restarting...\"}");
    delay(500);
    ESP.restart();
}

void WebServer::handleApiGpioList(AsyncWebServerRequest* request) {
    const int pins[] = {2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
    String json = "[";
    for (int i = 0; i < 19; i++) {
        int p = pins[i];
        if (i > 0) json += ",";
        json += "{\"pin\":" + String(p) + ",";
        json += "\"state\":" + String(digitalRead(p)) + "}";
    }
    json += "]";
    request->send(200, "application/json", json);
}

void WebServer::handleApiGpioSet(AsyncWebServerRequest* request) {
    String body = request->arg("plain");
    if (body.length() == 0) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Body vazio\"}");
        return;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"JSON invalido\"}");
        return;
    }
    int pin = doc["pin"] | -1;
    if (pin < 0 || pin > 39) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"PIN invalido\"}");
        return;
    }
    if (!doc["mode"].isNull()) {
        String mode = doc["mode"] | "";
        if (mode == "input") {
            pinMode(pin, INPUT);
        } else if (mode == "output") {
            pinMode(pin, OUTPUT);
        } else if (mode == "input_pullup") {
            pinMode(pin, INPUT_PULLUP);
        } else {
            pinMode(pin, INPUT_PULLDOWN);
        }
    }
    if (!doc["state"].isNull()) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, doc["state"] | 0);
    }
    request->send(200, "application/json", "{\"status\":\"ok\"}");
}

void WebServer::handleApiFsList(AsyncWebServerRequest* request) {
    String path = request->arg("path");
    if (path.length() == 0) path = "/";
    File root = LittleFS.open(path);
    if (!root || !root.isDirectory()) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Diretorio invalido\"}");
        return;
    }
    String json = "[";
    File f = root.openNextFile();
    bool first = true;
    while (f) {
        if (!first) json += ",";
        first = false;
        json += "{\"name\":\"" + String(f.name()) + "\",";
        json += "\"size\":" + String(f.size()) + ",";
        json += "\"dir\":" + String(f.isDirectory() ? "true" : "false") + "}";
        f = root.openNextFile();
    }
    json += "]";
    request->send(200, "application/json", json);
}

void WebServer::handleApiFsRead(AsyncWebServerRequest* request) {
    String path = request->arg("path");
    if (path.length() == 0) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"path obrigatorio\"}");
        return;
    }
    if (!LittleFS.exists(path)) {
        request->send(404, "application/json", "{\"status\":\"error\",\"message\":\"Arquivo nao encontrado\"}");
        return;
    }
    File f = LittleFS.open(path, "r");
    if (!f) {
        request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Erro ao abrir\"}");
        return;
    }
    String content = f.readString();
    f.close();
    String json = "{\"path\":\"" + path + "\",\"size\":" + String(content.length()) + ",\"content\":\"";
    for (size_t i = 0; i < content.length(); i++) {
        char c = content.charAt(i);
        if (c == '"') json += "\\\"";
        else if (c == '\\') json += "\\\\";
        else if (c == '\n') json += "\\n";
        else if (c == '\r') json += "\\r";
        else if (c == '\t') json += "\\t";
        else json += c;
    }
    json += "\"}";
    request->send(200, "application/json", json);
}

void WebServer::handleApiFsWrite(AsyncWebServerRequest* request) {
    String body = request->arg("plain");
    if (body.length() == 0) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Body vazio\"}");
        return;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"JSON invalido\"}");
        return;
    }
    String path = doc["path"] | "";
    String content = doc["content"] | "";
    if (path.length() == 0) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"path obrigatorio\"}");
        return;
    }
    File f = LittleFS.open(path, "w");
    if (!f) {
        request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Erro ao escrever\"}");
        return;
    }
    f.print(content);
    f.close();
    request->send(200, "application/json", "{\"status\":\"ok\",\"path\":\"" + path + "\",\"size\":" + String(content.length()) + "}");
}

void WebServer::handleApiFsDelete(AsyncWebServerRequest* request) {
    String path = request->arg("path");
    if (path.length() == 0) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"path obrigatorio\"}");
        return;
    }
    if (!LittleFS.exists(path)) {
        request->send(404, "application/json", "{\"status\":\"error\",\"message\":\"Arquivo nao encontrado\"}");
        return;
    }
    if (LittleFS.remove(path)) {
        request->send(200, "application/json", "{\"status\":\"ok\",\"path\":\"" + path + "\"}");
    } else {
        request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Erro ao deletar\"}");
    }
}

void WebServer::handleApiApps(AsyncWebServerRequest* request) {
    String json = "{\"apps\":[";

    // Load manifest
    String manifest;
    if (LittleFS.exists("/apps/.manifest.json")) {
        File mf = LittleFS.open("/apps/.manifest.json", "r");
        if (mf) manifest = mf.readString();
        if (mf) mf.close();
    }

    File root = LittleFS.open("/apps");
    if (root && root.isDirectory()) {
        File f = root.openNextFile();
        bool first = true;
        while (f) {
            String name = String(f.name());
            if (name.endsWith(".html") && !name.startsWith(".")) {
                if (!first) json += ",";
                first = false;
                String id = name.substring(0, name.length() - 5);
                json += "{\"id\":\"" + id + "\",\"path\":\"" + name + "\",\"size\":" + String(f.size());

                // Check manifest for metadata
                String key = "\"" + id + "\":";
                int idx = manifest.indexOf(key);
                if (idx >= 0) {
                    int start = manifest.indexOf('{', idx + key.length());
                    int end = manifest.indexOf('}', start);
                    if (start > 0 && end > start) {
                        json += "," + manifest.substring(start, end + 1);
                    }
                } else {
                    json += ",\"type\":\"user\",\"name\":\"" + id + "\"";
                }

                json += "}";
            }
            f = root.openNextFile();
        }
    }
    json += "]}";
    request->send(200, "application/json", json);
}

void WebServer::handleApiStoreInstall(AsyncWebServerRequest* request) {
    String body = request->arg("plain");
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err || doc["url"].isNull()) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"JSON invalido ou url ausente\"}");
        return;
    }
    String url = doc["url"].as<String>();
    String filename = doc["path"].as<String>();
    if (filename.length() == 0) {
        int lastSlash = url.lastIndexOf('/');
        if (lastSlash >= 0) filename = url.substring(lastSlash + 1);
        else filename = url;
    }
    if (!filename.startsWith("/")) filename = "/apps/" + filename;
    else if (!filename.startsWith("/apps/")) filename = "/apps/" + filename.substring(1);

    String host = "raw.githubusercontent.com";
    String rawPath = "/victorbillyph/ESPortable32/main/apps/" + url;
    if (url.startsWith("http")) {
        int slash = url.indexOf("/", 8);
        if (slash > 0) {
            host = url.substring(0, slash);
            int pathStart = url.indexOf("/", slash + 1);
            if (pathStart > 0) rawPath = url.substring(pathStart);
            else rawPath = "/";
            host = host.substring(url.indexOf("/") + 2);
        }
    }

    WiFiClient client;
    if (!client.connect(host.c_str(), 80)) {
        request->send(502, "application/json", "{\"status\":\"error\",\"message\":\"Falha ao conectar\"}");
        return;
    }
    client.println("GET " + rawPath + " HTTP/1.1");
    client.println("Host: " + host);
    client.println("User-Agent: ESPortable32");
    client.println("Connection: close");
    client.println();

    unsigned long timeout = millis() + 8000;
    while (!client.available() && millis() < timeout) {
        delay(10);
    }

    String response;
    bool headerEnd = false;
    while (client.available() && millis() < timeout) {
        String line = client.readStringUntil('\n');
        if (!headerEnd && line == "\r") {
            headerEnd = true;
            continue;
        }
        if (headerEnd) {
            response += line + "\n";
        }
    }
    client.stop();

    if (response.length() == 0) {
        request->send(502, "application/json", "{\"status\":\"error\",\"message\":\"Resposta vazia\"}");
        return;
    }

    if (!LittleFS.exists("/apps")) LittleFS.mkdir("/apps");
    File f = LittleFS.open(filename, "w");
    if (!f) {
        request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Erro ao salvar\"}");
        return;
    }
    f.print(response);
    f.close();

    request->send(200, "application/json", "{\"status\":\"ok\",\"path\":\"" + filename + "\",\"size\":" + String(response.length()) + "}");
}

void WebServer::handleApiProxy(AsyncWebServerRequest* request) {
    String body = request->arg("plain");
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err || doc["url"].isNull()) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"JSON invalido ou url ausente\"}");
        return;
    }
    String url = doc["url"].as<String>();
    if (!url.startsWith("http://") && !url.startsWith("https://")) {
        url = "http://" + url;
    }
    int slash = url.indexOf("/", 8);
    String host, path;
    if (slash > 0) {
        host = url.substring(0, slash);
        path = url.substring(slash);
    } else {
        host = url;
        path = "/";
    }
    int colon = host.indexOf(':', 8);
    int port = 80;
    if (colon > 0) {
        port = host.substring(colon + 1).toInt();
        host = host.substring(0, colon);
    }
    host = host.substring(host.indexOf("/") + 2);

    WiFiClient client;
    if (!client.connect(host.c_str(), port)) {
        request->send(502, "application/json", "{\"status\":\"error\",\"message\":\"Falha ao conectar\"}");
        return;
    }
    client.println("GET " + path + " HTTP/1.1");
    client.println("Host: " + host);
    client.println("User-Agent: ESPortable32");
    client.println("Connection: close");
    client.println();

    unsigned long timeout = millis() + 8000;
    while (!client.available() && millis() < timeout) {
        delay(10);
    }

    String response;
    bool headerEnd = false;
    while (client.available() && millis() < timeout) {
        String line = client.readStringUntil('\n');
        if (!headerEnd && line == "\r") {
            headerEnd = true;
            continue;
        }
        if (headerEnd) {
            response += line + "\n";
        }
    }
    client.stop();

    String json = "{\"url\":\"" + url + "\",\"content\":\"";
    for (size_t i = 0; i < response.length(); i++) {
        char c = response.charAt(i);
        if (c == '"') json += "\\\"";
        else if (c == '\\') json += "\\\\";
        else if (c == '\n') json += "\\n";
        else if (c == '\r') json += "\\r";
        else if (c == '\t') json += "\\t";
        else if (c < 32) json += " ";
        else json += c;
    }
    json += "\"}";
    request->send(200, "application/json", json);
}

void WebServer::handleApiSetup(AsyncWebServerRequest* request) {
    String ssid = request->arg("ssid");
    String pass = request->arg("pass");
    String pin = request->arg("pin");
    if (ssid.length() > 0) {
        _config.setWifi(ssid, pass);
    }
    if (pin.length() > 0) {
        _config.setPin(pin);
        _state.setPin(pin);
    }
    _config.save();
    request->send(200, "text/plain", "OK");
}

String WebServer::getContentType(const String& path) {
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".css")) return "text/css";
    if (path.endsWith(".js")) return "application/javascript";
    if (path.endsWith(".json")) return "application/json";
    if (path.endsWith(".png")) return "image/png";
    if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
    if (path.endsWith(".svg")) return "image/svg+xml";
    if (path.endsWith(".ico")) return "image/x-icon";
    if (path.endsWith(".woff")) return "font/woff";
    if (path.endsWith(".woff2")) return "font/woff2";
    return "text/plain";
}

void WebServer::handleSerialCommand(const String& cmd, AsyncWebServerRequest* request) {
    // Process serial-style commands via HTTP
    if (cmd.startsWith("WIFI=")) {
        String rest = cmd.substring(5);
        int comma = rest.indexOf(',');
        if (comma > 0) {
            _config.setWifi(rest.substring(0, comma), rest.substring(comma + 1));
            request->send(200, "text/plain", "OK:WiFi configurado");
        } else {
            request->send(400, "text/plain", "ERRO:Formato WIFI=ssid,pass");
        }
    } else if (cmd.startsWith("PIN=")) {
        String pin = cmd.substring(4);
        _config.setPin(pin);
        _state.setPin(pin);
        request->send(200, "text/plain", "OK:PIN configurado");
    } else if (cmd == "SAVE") {
        _config.save();
        request->send(200, "text/plain", "OK:Salvo");
    } else {
        request->send(400, "text/plain", "ERRO:Comando desconhecido");
    }
}
