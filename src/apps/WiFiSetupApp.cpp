#include "WiFiSetupApp.h"
#include "../icons/Icons.h"

static const char* AP_SSID = "ESPortable32_Config";

WiFiSetupApp::WiFiSetupApp(Config* config)
    : _config(config), _server(nullptr), _apStarted(false), _done(false),
      _phase(PHASE_BRAND), _phaseStart(0), _waitingBoot(true),
      _animFrame(0), _lastAnim(0), _scanTime(0) {}

void WiFiSetupApp::init() {
    _done = false;
    _phase = PHASE_BRAND;
    _phaseStart = millis();
    _animFrame = 0;
    _waitingBoot = true;
    _scanResult = "";
    _scanTime = 0;
}

void WiFiSetupApp::update() {
    if (_server) _server->handleClient();
    if (_scanResult == "" && _scanTime > 0 && millis() - _scanTime > 2000) {
        _scanResult = buildOptions();
    }
}

void WiFiSetupApp::buttonClick() {
    switch (_phase) {
        case PHASE_PRESS_START:
            advancePhase();
            break;
        case PHASE_WIFI_CONNECT:
            advancePhase();
            break;
        default:
            break;
    }
}

void WiFiSetupApp::advancePhase() {
    switch (_phase) {
        case PHASE_BRAND:
            _phase = PHASE_PRESS_START;
            _phaseStart = millis();
            break;
        case PHASE_PRESS_START:
            _phase = PHASE_WIFI_CONNECT;
            _phaseStart = millis();
            startAP();
            break;
        case PHASE_WIFI_CONNECT:
            _phase = PHASE_SHOW_IP;
            _phaseStart = millis();
            _scanTime = millis();
            break;
        default:
            break;
    }
}

void WiFiSetupApp::startAP() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID);
    _apStarted = true;
    IPAddress ip = WiFi.softAPIP();
    Serial.printf("[Setup] AP: %s IP: %s\n", AP_SSID, ip.toString().c_str());

    _server = new WebServer(80);
    _server->on("/", [this]() { handleRoot(); });
    _server->on("/save", [this]() { handleSave(); });
    _server->begin();
}

void WiFiSetupApp::draw(Display& d) {
    switch (_phase) {
        case PHASE_BRAND: drawBrand(d); break;
        case PHASE_PRESS_START: drawPressStart(d); break;
        case PHASE_WIFI_CONNECT: drawWifiConnect(d); break;
        case PHASE_SHOW_IP: drawShowIP(d); break;
        case PHASE_DONE: break;
    }
}

// ── Brand Animation ──────────────────────────────────────

void WiFiSetupApp::drawBrand(Display& d) {
    d.clear();
    unsigned long elapsed = millis() - _phaseStart;

    // Phase A: chip sliding from left (0-1500ms)
    if (elapsed < 1500) {
        int chipX = (int)((float)elapsed / 1500 * 80) - 30;
        if (chipX < -30) chipX = -30;
        if (chipX > 24) chipX = 24;
        int chipY = 16;

        // draw ESP32 chip
        d.oled().fillRect(chipX, chipY, 40, 32, SSD1306_WHITE);          // body
        d.oled().fillRect(chipX - 4, chipY + 6, 4, 4, SSD1306_WHITE);    // pin left
        d.oled().fillRect(chipX - 4, chipY + 16, 4, 4, SSD1306_WHITE);
        d.oled().fillRect(chipX + 40, chipY + 6, 4, 4, SSD1306_WHITE);   // pin right
        d.oled().fillRect(chipX + 40, chipY + 16, 4, 4, SSD1306_WHITE);
        d.oled().fillRect(chipX + 12, chipY + 4, 16, 24, SSD1306_BLACK); // die
        d.oled().setCursor(chipX + 16, chipY + 12);
        d.oled().setTextSize(1);
        d.oled().setTextColor(SSD1306_WHITE);
        d.oled().print("ESP");

        d.drawCenteredText(56, "ESPortable32", 1);
        d.show();
        return;
    }

    // Phase B: fusion effect (1500-2500ms)
    if (elapsed < 2500) {
        int flash = (elapsed - 1500) / 10;
        if (flash % 20 < 10) d.fillFrame(0, 0, SCREEN_W, SCREEN_H, true);
        else d.fillFrame(0, 0, SCREEN_W, SCREEN_H, false);

        d.oled().setTextSize(2);
        if (flash % 20 < 10) d.oled().setTextColor(SSD1306_BLACK);
        else d.oled().setTextColor(SSD1306_WHITE);
        d.drawCenteredText(24, "ESPortable32", 2);
        d.oled().setTextSize(1);
        d.oled().setTextColor(SSD1306_WHITE);
        d.show();
        return;
    }

    // Phase C: show logo (2500ms+)
    d.drawCenteredText(4, "ESPortable32", 2);
    d.drawHLine(0, 22, SCREEN_W);
    d.drawCenteredText(30, "SmartWatch Edition", 1);
    d.drawCenteredText(42, "Firmware v3.0", 1);
    d.drawCenteredText(54, "Powered by ESP32", 1);
    d.show();

    // auto-advance after 3s
    if (elapsed > 5000) {
        _phase = PHASE_PRESS_START;
        _phaseStart = millis();
    }
}

// ── Press BOOT to Start ──────────────────────────────────

void WiFiSetupApp::drawPressStart(Display& d) {
    d.clear();

    // ESP32 chip icon
    d.oled().fillRect(44, 6, 40, 32, SSD1306_WHITE);
    d.oled().fillRect(40, 12, 4, 4, SSD1306_WHITE);
    d.oled().fillRect(40, 22, 4, 4, SSD1306_WHITE);
    d.oled().fillRect(84, 12, 4, 4, SSD1306_WHITE);
    d.oled().fillRect(84, 22, 4, 4, SSD1306_WHITE);
    d.oled().fillRect(56, 10, 16, 24, SSD1306_BLACK);
    d.oled().setCursor(58, 18);
    d.oled().setTextSize(1);
    d.oled().setTextColor(SSD1306_WHITE);
    d.oled().print("ESP");

    // button indicator below
    d.oled().setTextColor(SSD1306_WHITE);
    d.oled().setTextSize(1);
    d.drawCenteredText(44, "Pressione BOOT", 1);
    d.drawCenteredText(54, "para configurar", 1);

    // animated arrow pointing down
    if ((millis() / 500) % 2 == 0) {
        d.drawCenteredText(60, "v", 1);
    }

    d.show();
}

// ── Connect to WiFi ──────────────────────────────────────

void WiFiSetupApp::drawWifiConnect(Display& d) {
    d.clear();

    // WiFi icon
    d.drawIcon(48, 4, ICON_WIFI, 16, 16);

    d.drawCenteredText(24, "Conecte-se ao WiFi:", 1);
    d.oled().setTextSize(1);
    d.oled().setTextColor(SSD1306_WHITE);
    d.oled().setCursor((SCREEN_W - strlen(AP_SSID) * 6) / 2, 34);
    d.oled().print(AP_SSID);

    d.drawCenteredText(48, "Depois pressione BOOT", 1);

    if ((millis() / 500) % 2 == 0) {
        d.drawCenteredText(58, "v", 1);
    }

    d.show();
}

// ── Show IP ──────────────────────────────────────────────

void WiFiSetupApp::drawShowIP(Display& d) {
    d.clear();
    d.drawCenteredText(2, "Configuracao", 1);
    d.drawHLine(0, 11, SCREEN_W);

    IPAddress ip = WiFi.softAPIP();
    char buf[32];

    d.oled().setCursor(2, 16);
    d.oled().setTextSize(1);
    d.oled().setTextColor(SSD1306_WHITE);
    d.oled().print("Conectado em:");
    d.oled().setCursor(2, 26);
    d.oled().print(AP_SSID);

    d.oled().setCursor(2, 38);
    d.oled().print("Abra o navegador:");
    d.oled().setCursor(2, 48);
    snprintf(buf, sizeof(buf), "http://%s", ip.toString().c_str());
    d.oled().setTextSize(1);
    d.oled().print(buf);

    d.show();
}

// ── Web handlers ─────────────────────────────────────────

String WiFiSetupApp::htmlPage() {
    String page = R"rawliteral(
<!DOCTYPE html><html lang="pt-BR"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESPortable32 - Config</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
  background:linear-gradient(135deg,#0a0a1a 0%,#1a1a3e 100%);
  min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px;color:#eee}
.container{max-width:480px;width:100%}
.card{background:rgba(255,255,255,0.06);backdrop-filter:blur(20px);
  border-radius:24px;padding:32px;border:1px solid rgba(255,255,255,0.1)}
.logo{text-align:center;margin-bottom:24px}
.logo h1{font-size:28px;background:linear-gradient(135deg,#0ff,#06f);
  -webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text}
.logo p{color:#888;font-size:14px;margin-top:4px}
.desc{background:rgba(0,255,255,0.05);border-radius:12px;padding:16px;margin-bottom:24px;font-size:14px;line-height:1.5;color:#aaa}
.desc strong{color:#0ff}
label{display:block;font-size:13px;font-weight:600;margin:16px 0 6px;color:#bbb}
select,input{width:100%;padding:12px 14px;border-radius:12px;border:1px solid rgba(255,255,255,0.15);
  background:rgba(255,255,255,0.05);color:#fff;font-size:15px;outline:none;transition:border .2s}
select:focus,input:focus{border-color:#0ff}
select option{background:#1a1a3e}
.btn{width:100%;padding:14px;margin-top:24px;border-radius:12px;border:none;
  background:linear-gradient(135deg,#0ff,#06f);color:#000;font-size:16px;font-weight:700;
  cursor:pointer;transition:transform .2s,opacity .2s}
.btn:hover{transform:translateY(-1px);opacity:.9}
.btn:active{transform:translateY(0)}
.footer{text-align:center;margin-top:16px;font-size:12px;color:#555}
.footer a{color:#0ff;text-decoration:none}
.loading{display:none;text-align:center;margin-top:12px;color:#0ff}
</style></head><body>
<div class="container">
<div class="card">
<div class="logo">
<h1>ESPortable32</h1>
<p>SmartWatch Edition</p>
</div>
<div class="desc">
<strong>Bem-vindo!</strong> O ESPortable32 e um firmware open-source
que transforma seu ESP32 em um smartwatch funcional.
<br><br>
Configure abaixo o nome do dispositivo e a rede WiFi:
</div>
<form id="configForm" action="/save" method="POST">
<label for="name">Nome do Dispositivo</label>
<input type="text" name="name" id="name" placeholder="Meu ESP32 Watch" value="ESPortable32" maxlength="24">

<label for="ssid">Rede WiFi</label>
<select name="ssid" id="ssid">
<option value="">Escaneando redes...</option>
)rawliteral";

    page += _scanResult;

    page += R"rawliteral(
</select>

<label for="pass">Senha WiFi</label>
<input type="password" name="pass" id="pass" placeholder="Digite a senha (se houver)">

<button type="submit" class="btn" onclick="this.disabled=true;this.style.opacity='0.5';document.querySelector('.loading').style.display='block';document.getElementById('configForm').submit()">
Configurar e Iniciar
</button>
<div class="loading">Configurando... o dispositivo sera reiniciado em alguns segundos.</div>
</form>
</div>
<div class="footer">
<a href="https://github.com/anomalyco/ESPortable32" target="_blank">GitHub</a>
</div>
</div></body></html>
)rawliteral";
    return page;
}

String WiFiSetupApp::buildOptions() {
    String opts = "";
    int n = WiFi.scanNetworks();
    if (n == 0) {
        opts += "<option value=\"\">Nenhuma rede encontrada</option>";
    }
    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        ssid.replace("&", "&amp;");
        ssid.replace("<", "&lt;");
        ssid.replace(">", "&gt;");
        ssid.replace("\"", "&quot;");
        int rssi = WiFi.RSSI(i);
        String sig = rssi > -50 ? "&#x1f7e2;" : rssi > -70 ? "&#x1f7e1;" : "\u26aa";
        opts += "<option value=\"" + ssid + "\">" + sig + " " + ssid + " (" + String(rssi) + "dBm)</option>";
    }
    return opts;
}

void WiFiSetupApp::handleRoot() {
    _server->send(200, "text/html; charset=utf-8", htmlPage());
}

void WiFiSetupApp::handleSave() {
    String name = _server->hasArg("name") ? _server->arg("name") : "ESPortable32";

    if (!_server->hasArg("ssid") || _server->arg("ssid").length() == 0) {
        _server->send(200, "text/html; charset=utf-8",
            "<html><body style='background:#111;color:#fff;font-family:sans-serif;"
            "display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0'>"
            "<div style='text-align:center'><h2>Erro</h2><p>Selecione uma rede WiFi.</p>"
            "<a href='/' style='color:#0ff'>Voltar</a></div></body></html>");
        return;
    }

    String ssid = _server->arg("ssid");
    String pass = _server->hasArg("pass") ? _server->arg("pass") : "";

    _server->send(200, "text/html; charset=utf-8",
        "<!DOCTYPE html><html><body style='background:#111;color:#fff;font-family:sans-serif;"
        "display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0'>"
        "<div style='text-align:center'>"
        "<h1 style='color:#0ff'>Configurado!</h1>"
        "<p>O dispositivo sera reiniciado e conectara automaticamente.</p>"
        "</div></body></html>");
    _server->handleClient();
    delay(100);

    _config->saveAll(name.c_str(), ssid.c_str(), pass.c_str());

    _done = true;
    delay(500);
    ESP.restart();
}

void WiFiSetupApp::exit() {
    if (_server) {
        _server->stop();
        delete _server;
        _server = nullptr;
    }
    WiFi.softAPdisconnect(true);
    _apStarted = false;
}
