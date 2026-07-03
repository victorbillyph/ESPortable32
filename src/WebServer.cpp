#include "WebServer.h"
#include <WiFi.h>

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

    if (_state.isLocked() && !url.startsWith("/unlock") && !url.startsWith("/api") && !url.startsWith("/css") && !url.startsWith("/js")) {
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
        request->send(LittleFS, path, contentType);
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
