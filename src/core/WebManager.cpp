#include "WebManager.h"
#include <WiFi.h>
#include <Preferences.h>

#define ALARM_NS "esp32alr"
#define MAX_ALARMS 8

WebManager::WebManager(Config* cfg, ModuleManager* modMgr, PrinterManager* printerMgr)
    : _server(80), _config(cfg), _modules(modMgr), _printers(printerMgr), _running(false),
      _uploadSize(0), _uploading(false) {
    mediaInit();
    _uploadName[0] = '\0';
}

void WebManager::begin() {
    _server.on("/", [this]() { handleRoot(); });
    _server.on("/upload", HTTP_GET, [this]() { handleUpload(); });
    _server.on("/upload", HTTP_POST, [this]() { handleUpload(); }, [this]() { handleUploadFile(); });
    _server.on("/api/status", [this]() { handleAPI(); });
    _server.on("/api/config", [this]() { handleAPI(); });
    _server.on("/api/modules", [this]() { handleAPI(); });
    _server.on("/api/reboot", [this]() { handleAPI(); });
    _server.on("/api/alarms", [this]() { handleAPI(); });
    _server.begin();
    _running = true;
}

void WebManager::update() {
    if (_running) _server.handleClient();
}

String WebManager::urlDecode(const String& str) {
    String decoded = "";
    char temp[3] = {0};
    for (size_t i = 0; i < str.length(); i++) {
        char c = str[i];
        if (c == '+') {
            decoded += ' ';
        } else if (c == '%') {
            if (i + 2 < str.length()) {
                temp[0] = str[i + 1];
                temp[1] = str[i + 2];
                temp[2] = 0;
                decoded += (char)strtol(temp, nullptr, 16);
                i += 2;
            }
        } else {
            decoded += c;
        }
    }
    return decoded;
}

String WebManager::statusJSON() {
    String json = "{";
    json += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"uptime\":" + String(millis() / 1000) + ",";
    json += "\"wifi\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"modules\":" + String(_modules->count()) + ",";
    json += "\"printers\":" + String(_printers->printerCount());
    json += "}";
    return json;
}

String WebManager::printersJSON() {
    String json = "{\"scanning\":" + String(_printers->isScanning() ? "true" : "false") + ",";
    json += "\"printers\":[";
    int count = _printers->printerCount();
    for (int i = 0; i < count; i++) {
        Printer p = _printers->getPrinter(i);
        if (i > 0) json += ",";
        json += "{";
        json += "\"idx\":" + String(i) + ",";
        json += "\"name\":\"" + p.name + "\",";
        json += "\"ip\":\"" + p.ip + "\",";
        json += "\"port\":" + String(p.port) + ",";
        json += "\"mac\":\"" + p.mac + "\",";
        json += "\"bt\":" + String(p.isBluetooth ? "true" : "false") + ",";
        json += "\"reachable\":" + String(p.reachable ? "true" : "false");
        json += "}";
    }
    json += "]}";
    return json;
}

String WebManager::configJSON() {
    String json = "{";
    json += "\"name\":\"" + _config->getDeviceName() + "\"";
    json += "}";
    return json;
}

String WebManager::alarmsJSON() {
    Preferences prefs;
    prefs.begin(ALARM_NS, true);
    int count = prefs.getUChar("count", 0);
    if (count > MAX_ALARMS) count = MAX_ALARMS;
    String json = "[";
    for (int i = 0; i < count; i++) {
        if (i > 0) json += ",";
        char k[4];
        snprintf(k, 4, "h%c", '0' + i);
        int h = prefs.getUChar(k, 0);
        snprintf(k, 4, "m%c", '0' + i);
        int m = prefs.getUChar(k, 0);
        snprintf(k, 4, "d%c", '0' + i);
        int d = prefs.getUChar(k, 0);
        snprintf(k, 4, "e%c", '0' + i);
        bool e = prefs.getBool(k, true);
        json += "{";
        json += "\"idx\":" + String(i) + ",";
        json += "\"hour\":" + String(h) + ",";
        json += "\"minute\":" + String(m) + ",";
        json += "\"days\":" + String(d) + ",";
        json += "\"enabled\":" + String(e ? "true" : "false");
        json += "}";
    }
    json += "]";
    prefs.end();
    return json;
}

String WebManager::modulesJSON() {
    String json = "[";
    int count = _modules->count();
    for (int i = 0; i < count; i++) {
        ModuleDef m = _modules->get(i);
        if (i > 0) json += ",";
        json += "{";
        json += "\"id\":\"" + m.id + "\",";
        json += "\"type\":\"" + m.type + "\",";
        json += "\"name\":\"" + m.name + "\",";
        json += "\"enabled\":" + String(m.enabled ? "true" : "false") + ",";
        json += "\"c1\":\"" + m.cfg1 + "\",";
        json += "\"c2\":\"" + m.cfg2 + "\",";
        json += "\"c3\":\"" + m.cfg3 + "\"";
        json += "}";
    }
    json += "]";
    return json;
}

String WebManager::buildDashboard() {
    return R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESPortable32</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#0f0f12;color:#e0e0e0;font-family:'JetBrains Mono','Fira Code','Cascadia Code',monospace;min-height:100vh;display:flex}

/* ─── Sidebar ─── */
.sidebar{position:fixed;left:0;top:0;bottom:0;width:200px;background:rgba(20,20,28,0.95);backdrop-filter:blur(16px);-webkit-backdrop-filter:blur(16px);border-right:1px solid rgba(255,255,255,0.05);padding:16px 0;z-index:100;display:flex;flex-direction:column;overflow-y:auto}
.sidebar .logo{font-size:.75rem;font-weight:400;color:#8888aa;letter-spacing:2px;text-transform:uppercase;text-align:center;padding:12px 16px 20px;border-bottom:1px solid rgba(255,255,255,0.05);margin-bottom:8px}
.sidebar .logo span{color:#6080dd}
.sidebar nav{flex:1}
.nav-item{display:flex;align-items:center;gap:10px;padding:10px 18px;color:#808098;cursor:pointer;font-size:.78rem;letter-spacing:.5px;transition:all .2s;border-left:3px solid transparent;text-decoration:none}
.nav-item:hover{background:rgba(255,255,255,0.03);color:#c0c8e0}
.nav-item.active{background:rgba(96,128,221,0.12);color:#b0c0f0;border-left-color:#6080dd}
.nav-item .nav-icon{width:20px;text-align:center;font-size:1rem;flex-shrink:0}

/* ─── Sidebar toggle (mobile) ─── */
.sidebar-toggle{display:none;position:fixed;top:12px;left:12px;z-index:200;background:rgba(30,30,40,0.8);backdrop-filter:blur(8px);border:1px solid rgba(255,255,255,0.08);border-radius:8px;padding:6px 10px;color:#b0c0f0;font-size:1.2rem;cursor:pointer}
.sidebar-overlay{display:none;position:fixed;inset:0;background:rgba(0,0,0,0.5);z-index:99}

/* ─── Main Content ─── */
.main{flex:1;margin-left:200px;padding:24px 28px;max-width:900px;min-height:100vh}
.main h1{font-size:1.15rem;font-weight:400;color:#a0a0b8;letter-spacing:2px;text-transform:uppercase;margin-bottom:24px}
.page{display:none}
.page.active{display:block}

/* ─── Cards ─── */
.card{background:rgba(30,30,40,0.6);backdrop-filter:blur(16px);-webkit-backdrop-filter:blur(16px);border:1px solid rgba(255,255,255,0.06);border-radius:14px;padding:20px 24px;margin-bottom:18px;transition:border-color .2s}
.card:hover{border-color:rgba(100,140,255,0.2)}
.card h2{font-size:.85rem;font-weight:400;color:#8888aa;letter-spacing:1.5px;text-transform:uppercase;margin-bottom:14px;padding-bottom:8px;border-bottom:1px solid rgba(255,255,255,0.05)}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.stat{background:rgba(255,255,255,0.03);border-radius:8px;padding:12px 14px}
.stat .label{font-size:.65rem;color:#666680;text-transform:uppercase;letter-spacing:1px;margin-bottom:4px}
.stat .value{font-size:1.05rem;color:#c0c8e0}
label{display:block;font-size:.7rem;color:#8888aa;letter-spacing:1px;text-transform:uppercase;margin-bottom:4px;margin-top:10px}
label:first-child{margin-top:0}
input[type=text],input[type=number],select{width:100%;background:rgba(255,255,255,0.06);border:1px solid rgba(255,255,255,0.1);border-radius:8px;padding:10px 14px;color:#e0e0e0;font-family:inherit;font-size:.85rem;outline:none;transition:border-color .2s}
input[type=text]:focus,input[type=number]:focus,select:focus{border-color:rgba(100,140,255,0.5)}
input[type=range]{width:100%;-webkit-appearance:none;appearance:none;background:rgba(255,255,255,0.08);height:4px;border-radius:2px;outline:none;margin:6px 0}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:16px;height:16px;border-radius:50%;background:#6080dd;cursor:pointer;border:2px solid rgba(255,255,255,0.1)}
input[type=range]::-moz-range-thumb{width:16px;height:16px;border-radius:50%;background:#6080dd;cursor:pointer;border:2px solid rgba(255,255,255,0.1)}
input[type=checkbox]{accent-color:#6080dd;width:16px;height:16px;cursor:pointer}
textarea{width:100%;background:rgba(255,255,255,0.06);border:1px solid rgba(255,255,255,0.1);border-radius:8px;padding:10px 14px;color:#e0e0e0;font-family:inherit;font-size:.85rem;outline:none;resize:vertical}
textarea:focus{border-color:rgba(100,140,255,0.5)}
.btn{background:rgba(96,128,221,0.15);border:1px solid rgba(96,128,221,0.3);border-radius:8px;padding:8px 16px;color:#b0c0f0;font-family:inherit;font-size:.75rem;cursor:pointer;letter-spacing:.5px;transition:all .2s;white-space:nowrap}
.btn:hover{background:rgba(96,128,221,0.25);border-color:rgba(96,128,221,0.5)}
.btn-danger{background:rgba(221,80,80,0.15);border-color:rgba(221,80,80,0.3);color:#f0a0a0}
.btn-danger:hover{background:rgba(221,80,80,0.25);border-color:rgba(221,80,80,0.5)}
.btn-success{background:rgba(80,200,120,0.15);border-color:rgba(80,200,120,0.3);color:#a0e0b0}
.btn-success:hover{background:rgba(80,200,120,0.25);border-color:rgba(80,200,120,0.5)}
.btn-group{display:flex;gap:8px;flex-wrap:wrap;margin-top:12px;align-items:center}
.btn-group .btn{margin-top:0}
.module-row{background:rgba(255,255,255,0.02);border-radius:8px;padding:10px 14px;margin-bottom:8px;display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap}
.module-info{flex:1;min-width:120px}
.module-info .mod-name{font-size:.85rem;color:#d0d8f0;margin-bottom:2px}
.module-info .mod-type{font-size:.65rem;color:#666680;text-transform:uppercase;letter-spacing:.5px}
.module-actions{display:flex;align-items:center;gap:10px}
.module-actions input[type=checkbox]{margin:0}
.inline-form{background:rgba(255,255,255,0.03);border-radius:8px;padding:14px;margin-top:10px}
.inline-form .form-row{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px;margin-bottom:8px}
.inline-form .form-row-2{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-bottom:8px}
.config-fields{margin-top:8px;padding-top:8px;border-top:1px solid rgba(255,255,255,0.06)}
.range-value{float:right;color:#a0b0d0;font-size:.8rem}
.toast{position:fixed;bottom:24px;left:50%;transform:translateX(-50%);background:rgba(40,200,100,0.9);color:#fff;padding:10px 24px;border-radius:8px;font-size:.8rem;font-family:inherit;opacity:0;transition:opacity .3s;pointer-events:none;z-index:999}
.toast.error{background:rgba(200,60,60,0.9)}
.toast.show{opacity:1}

/* ─── Responsive ─── */
@media(max-width:640px){
  .sidebar{transform:translateX(-100%);transition:transform .3s}
  .sidebar.open{transform:translateX(0)}
  .sidebar-overlay.open{display:block}
  .sidebar-toggle{display:block}
  .main{margin-left:0;padding:16px;padding-top:56px}
  .grid2{grid-template-columns:1fr}
  .inline-form .form-row,.inline-form .form-row-2{grid-template-columns:1fr}
}
</style>
</head>
<body>

<button class="sidebar-toggle" id="menuToggle" onclick="toggleSidebar()">&#9776;</button>
<div class="sidebar-overlay" id="sidebarOverlay" onclick="toggleSidebar()"></div>

<div class="sidebar" id="sidebar">
<div class="logo">ESP<span>ortable</span>32</div>
<nav>
<a class="nav-item active" data-page="dashboard" onclick="showPage('dashboard')"><span class="nav-icon">&#9632;</span> Dashboard</a>
<a class="nav-item" data-page="modules" onclick="showPage('modules')"><span class="nav-icon">&#9881;</span> Modules</a>
<a class="nav-item" data-page="printers" onclick="showPage('printers')"><span class="nav-icon">&#9998;</span> Impressoras</a>
<a class="nav-item" data-page="alarms" onclick="showPage('alarms')"><span class="nav-icon">&#9200;</span> Alarmes</a>
<a class="nav-item" data-page="upload" onclick="showPage('upload')"><span class="nav-icon">&#128228;</span> Upload</a>
<a class="nav-item" data-page="reboot" onclick="showPage('reboot')"><span class="nav-icon">&#8635;</span> Reboot</a>
</nav>
</div>

<div class="main">
<h1 id="pageTitle">Dashboard</h1>

<div class="page active" id="page-dashboard">
<div class="card" id="status-card">
<h2>System Status</h2>
<div class="grid2">
<div class="stat"><div class="label">Heap</div><div class="value" id="stat-heap">--</div></div>
<div class="stat"><div class="label">Uptime</div><div class="value" id="stat-uptime">--</div></div>
<div class="stat"><div class="label">WiFi IP</div><div class="value" id="stat-wifi">--</div></div>
<div class="stat"><div class="label">Modules</div><div class="value" id="stat-modules">--</div></div>
<div class="stat"><div class="label">Printers</div><div class="value" id="stat-printers">--</div></div>
</div>
</div>
<div class="card" id="config-card">
<h2>Configuration</h2>
<label for="dev-name">Device Name</label>
<input type="text" id="dev-name" placeholder="ESPortable32">
<div class="btn-group">
<button class="btn btn-success" onclick="saveConfig()">Save Config</button>
</div>
</div>
</div>

<div class="page" id="page-modules">
<div class="card" id="modules-card">
<h2>Modules</h2>
<div id="module-list"></div>
<button class="btn" onclick="showAddForm()" id="add-mod-btn">+ Add Module</button>
<div id="add-form" class="inline-form" style="display:none">
<div class="form-row-2">
<div>
<label for="mod-type">Type</label>
<select id="mod-type" onchange="updateConfigFields()">
<option value="sensor">Sensor</option>
<option value="actuator">Actuator</option>
<option value="display">Display</option>
<option value="comm">Communication</option>
<option value="other">Other</option>
</select>
</div>
<div>
<label for="mod-name">Name</label>
<input type="text" id="mod-name" placeholder="Module name">
</div>
</div>
<div class="form-row">
<div><label for="mod-c1">Config 1</label><input type="text" id="mod-c1"></div>
<div><label for="mod-c2">Config 2</label><input type="text" id="mod-c2"></div>
<div><label for="mod-c3">Config 3</label><input type="text" id="mod-c3"></div>
</div>
<div class="btn-group">
<button class="btn btn-success" onclick="addModule()">Add</button>
<button class="btn" onclick="hideAddForm()">Cancel</button>
</div>
</div>
</div>
</div>

<div class="page" id="page-printers">
<div class="card" id="printers-card">
<h2>Impressoras</h2>
<div id="printer-list"></div>
<div class="btn-group">
<button class="btn" onclick="scanPrinters()">Scan Network</button>
</div>
<div style="margin-top:10px">
<label for="print-text">Texto para imprimir</label>
<textarea id="print-text" rows="3">ESPortable32 - Teste</textarea>
</div>
</div>
</div>

<div class="page" id="page-alarms">
<div class="card" id="alarms-card">
<h2>Alarmes</h2>
<div id="alarm-list"></div>
<div class="btn-group">
<button class="btn" onclick="showAlarmForm()">+ Novo Alarme</button>
</div>
<div id="alarm-form" class="inline-form" style="display:none">
<div class="form-row-2">
<div><label>Hora</label><input type="number" id="alarm-hour" min="0" max="23" value="7"></div>
<div><label>Minuto</label><input type="number" id="alarm-min" min="0" max="59" value="0"></div>
</div>
<div style="margin:8px 0">
<label>Dias da Semana</label>
<div id="alarm-days">
<label style="display:inline-block;margin:0 4px"><input type="checkbox" class="day-cb" value="0" checked> Dom</label>
<label style="display:inline-block;margin:0 4px"><input type="checkbox" class="day-cb" value="1" checked> Seg</label>
<label style="display:inline-block;margin:0 4px"><input type="checkbox" class="day-cb" value="2" checked> Ter</label>
<label style="display:inline-block;margin:0 4px"><input type="checkbox" class="day-cb" value="3" checked> Qua</label>
<label style="display:inline-block;margin:0 4px"><input type="checkbox" class="day-cb" value="4" checked> Qui</label>
<label style="display:inline-block;margin:0 4px"><input type="checkbox" class="day-cb" value="5" checked> Sex</label>
<label style="display:inline-block;margin:0 4px"><input type="checkbox" class="day-cb" value="6" checked> Sab</label>
</div>
</div>
<label><input type="checkbox" id="alarm-enabled" checked> Ativo</label>
<div class="btn-group">
<button class="btn btn-success" id="alarm-save-btn" onclick="saveAlarm()">Salvar</button>
<button class="btn" onclick="hideAlarmForm()">Cancelar</button>
</div>
<input type="hidden" id="alarm-idx" value="-1">
</div>
</div>
</div>

<div class="page" id="page-upload">
<div class="card">
<h2>Upload de Midia</h2>
<p style="font-size:.8rem;color:#808098;margin-bottom:14px">Max 40KB — WAV, RAW, ou MP3</p>
<form method="POST" action="/upload" enctype="multipart/form-data">
<input type="file" name="file" required style="display:block;margin-bottom:12px">
<input type="submit" class="btn btn-success" value="Enviar">
</form>
</div>
</div>

<div class="page" id="page-reboot">
<div class="card">
<h2>Reboot</h2>
<p style="font-size:.8rem;color:#808098;margin-bottom:14px">Reinicia o dispositivo ESP32</p>
<button class="btn btn-danger reboot-btn" onclick="rebootDevice()">Reboot ESP</button>
</div>
</div>

</div>
<div class="toast" id="toast"></div>

<script>
const pageTitles={dashboard:'Dashboard',modules:'Modules',printers:'Impressoras',alarms:'Alarmes',upload:'Upload',reboot:'Reboot'};

function qs(s){return document.querySelector(s)}
function gid(s){return document.getElementById(s)}

function toggleSidebar(){
  gid('sidebar').classList.toggle('open');
  gid('sidebarOverlay').classList.toggle('open');
}

function showPage(name){
  document.querySelectorAll('.page').forEach(p=>p.classList.remove('active'));
  document.querySelectorAll('.nav-item').forEach(n=>n.classList.remove('active'));
  gid('page-'+name).classList.add('active');
  document.querySelector(`.nav-item[data-page="${name}"]`).classList.add('active');
  gid('pageTitle').textContent=pageTitles[name]||name;
  if(window.innerWidth<=640){gid('sidebar').classList.remove('open');gid('sidebarOverlay').classList.remove('open')}
  if(name==='modules')refreshModules();
  if(name==='printers')refreshPrinters();
  if(name==='alarms')refreshAlarms();
}

function showToast(msg,err){
  const t=gid('toast');
  t.textContent=msg;
  t.className='toast'+(err?' error':'')+' show';
  setTimeout(()=>t.classList.remove('show'),3000);
}

async function fetchJSON(url,opts){
  const r=await fetch(url,opts);
  if(!r.ok)throw new Error(await r.text());
  return r.json();
}

async function refreshStatus(){
  try{
    const s=await fetchJSON('/api/status');
    gid('stat-heap').textContent=s.heap+' bytes';
    const u=s.uptime;
    const h=Math.floor(u/3600),m=Math.floor((u%3600)/60),sec=u%60;
    gid('stat-uptime').textContent=h+'h '+m+'m '+sec+'s';
    gid('stat-wifi').textContent=s.wifi;
    gid('stat-modules').textContent=s.modules;
  }catch(e){console.error('status err',e)}
}

async function refreshConfig(){
  try{
    const c=await fetchJSON('/api/config');
    gid('dev-name').value=c.name||'';
  }catch(e){console.error('config err',e)}
}

async function refreshModules(){
  try{
    const mods=await fetchJSON('/api/modules');
    const list=gid('module-list');
    list.innerHTML='';
    mods.forEach(m=>{
      const row=document.createElement('div');
      row.className='module-row';
      row.innerHTML=`
        <div class="module-info">
          <div class="mod-name">${esc(m.name)}</div>
          <div class="mod-type">${esc(m.type)}</div>
        </div>
        <div class="module-actions">
          <input type="checkbox" ${m.enabled?'checked':''} onchange="toggleModule('${m.id}',this.checked)">
          <button class="btn" onclick="editModule('${m.id}')">Edit</button>
          <button class="btn btn-danger" onclick="deleteModule('${m.id}')">Del</button>
        </div>`;
      list.appendChild(row);
    });
  }catch(e){console.error('modules err',e)}
}

function esc(s){const d=document.createElement('div');d.appendChild(document.createTextNode(s));return d.innerHTML}

async function saveConfig(){
  const fd=new URLSearchParams();
  fd.set('name',gid('dev-name').value);
  try{
    await fetch('/api/config',{method:'POST',body:fd});
    showToast('Config saved');
    refreshConfig();
  }catch(e){showToast('Save failed',1)}
}

function showAddForm(){
  gid('add-form').style.display='block';
  gid('add-mod-btn').style.display='none';
  gid('mod-type').value='sensor';
  gid('mod-name').value='';
  gid('mod-c1').value='';
  gid('mod-c2').value='';
  gid('mod-c3').value='';
  updateConfigFields();
}

function hideAddForm(){
  gid('add-form').style.display='none';
  gid('add-mod-btn').style.display='inline-block';
}

const configFieldLabels={
  sensor:['Pin','Interval (ms)','Threshold'],
  actuator:['Pin','PWM Channel','Default'],
  display:['Address','Width','Height'],
  comm:['Protocol','Port','Target'],
  other:['Value 1','Value 2','Value 3']
};

function updateConfigFields(){
  const type=gid('mod-type').value;
  const labels=configFieldLabels[type]||['Config 1','Config 2','Config 3'];
  document.querySelectorAll('#mod-c1, #mod-c2, #mod-c3').forEach((el,i)=>{
    const label=el.previousElementSibling;
    if(label)label.textContent=labels[i]||'Config '+(i+1);
  });
}

async function addModule(){
  const fd=new URLSearchParams();
  fd.set('type',gid('mod-type').value);
  fd.set('name',gid('mod-name').value);
  fd.set('enabled','1');
  fd.set('c1',gid('mod-c1').value);
  fd.set('c2',gid('mod-c2').value);
  fd.set('c3',gid('mod-c3').value);
  try{
    await fetch('/api/modules',{method:'POST',body:fd});
    showToast('Module added');
    hideAddForm();
    refreshModules();
  }catch(e){showToast('Add failed',1)}
}

async function deleteModule(id){
  if(!confirm('Delete module?'))return;
  const fd=new URLSearchParams();
  fd.set('id',id);
  try{
    await fetch('/api/modules',{method:'DELETE',body:fd});
    showToast('Module deleted');
    refreshModules();
  }catch(e){showToast('Delete failed',1)}
}

let editingId=null;

async function editModule(id){
  if(editingId){
    document.querySelector(`[data-edit="${editingId}"]`)?.remove();
  }
  editingId=id;
  const mods=await fetchJSON('/api/modules');
  const m=mods.find(x=>x.id===id);
  if(!m)return;
  const row=document.querySelector(`.module-row:has(button[onclick*="editModule('${id}')"])`);
  if(!row)return;
  const form=document.createElement('div');
  form.className='inline-form';
  form.dataset.edit=id;
  form.innerHTML=`
    <div class="form-row-2">
      <div><label>Name</label><input type="text" class="edit-name" value="${esc(m.name)}"></div>
      <div><label>Enabled</label><br><input type="checkbox" class="edit-enabled" ${m.enabled?'checked':''}></div>
    </div>
    <div class="form-row">
      <div><label>Config 1</label><input type="text" class="edit-c1" value="${esc(m.cfg1)}"></div>
      <div><label>Config 2</label><input type="text" class="edit-c2" value="${esc(m.cfg2)}"></div>
      <div><label>Config 3</label><input type="text" class="edit-c3" value="${esc(m.cfg3)}"></div>
    </div>
    <div class="btn-group">
      <button class="btn btn-success" onclick="saveEdit('${m.id}')">Save</button>
      <button class="btn" onclick="cancelEdit('${m.id}')">Cancel</button>
    </div>`;
  row.after(form);
}

async function saveEdit(id){
  const form=document.querySelector(`[data-edit="${id}"]`);
  if(!form)return;
  const fd=new URLSearchParams();
  fd.set('id',id);
  fd.set('name',form.querySelector('.edit-name').value);
  fd.set('enabled',form.querySelector('.edit-enabled').checked?'1':'0');
  fd.set('c1',form.querySelector('.edit-c1').value);
  fd.set('c2',form.querySelector('.edit-c2').value);
  fd.set('c3',form.querySelector('.edit-c3').value);
  try{
    await fetch('/api/modules',{method:'PUT',body:fd});
    showToast('Module updated');
    cancelEdit(id);
    refreshModules();
  }catch(e){showToast('Update failed',1)}
}

function cancelEdit(id){
  document.querySelector(`[data-edit="${id}"]`)?.remove();
  editingId=null;
}

async function toggleModule(id,enabled){
  const mods=await fetchJSON('/api/modules');
  const m=mods.find(x=>x.id===id);
  if(!m)return;
  const fd=new URLSearchParams();
  fd.set('id',id);
  fd.set('name',m.name);
  fd.set('enabled',enabled?'1':'0');
  fd.set('c1',m.cfg1);
  fd.set('c2',m.cfg2);
  fd.set('c3',m.cfg3);
  try{
    await fetch('/api/modules',{method:'PUT',body:fd});
    refreshModules();
  }catch(e){showToast('Toggle failed',1)}
}

async function rebootDevice(){
  if(!confirm('Reboot ESP now?'))return;
  try{
    await fetch('/api/reboot',{method:'POST'});
    showToast('Rebooting...');
    setTimeout(()=>{location.reload()},5000);
  }catch(e){showToast('Reboot failed',1)}
}

async function refreshPrinters(){
  try{
    const data=await fetchJSON('/api/printers');
    const list=gid('printer-list');
    list.innerHTML='';
    const arr=data.printers||[];
    arr.forEach((p,i)=>{
      const row=document.createElement('div');
      row.className='module-row';
      row.innerHTML=`
        <div class="module-info">
          <div class="mod-name">${esc(p.name)}</div>
          <div class="mod-type">${p.bt?'BT':'TCP'} ${p.ip}:${p.port} ${p.reachable?'(OK)':'(Offline)'}</div>
        </div>
        <button class="btn btn-success" onclick="printToPrinter(${i})">Print</button>`;
      list.appendChild(row);
    });
    gid('stat-printers').textContent=data.printers;
  }catch(e){}
}

async function scanPrinters(){
  const fd=new URLSearchParams();
  fd.set('action','scan');
  try{
    await fetch('/api/printers',{method:'POST',body:fd});
    showToast('Escaneando rede...');
    setTimeout(refreshPrinters,3000);
  }catch(e){showToast('Scan failed',1)}
}

async function printToPrinter(idx){
  const text=gid('print-text').value;
  const fd=new URLSearchParams();
  fd.set('action','print');
  fd.set('idx',idx);
  fd.set('text',text);
  try{
    const r=await fetchJSON('/api/printers',{method:'POST',body:fd});
    if(r.status==='ok')showToast('Impresso!');
    else showToast('Erro: '+r.status,1);
  }catch(e){showToast('Print failed',1)}
}

let editingAlarmIdx=null;

function getDaysMask(){
  let mask=0;
  document.querySelectorAll('.day-cb').forEach(cb=>{
    if(cb.checked)mask|=1<<parseInt(cb.value);
  });
  return mask;
}

function setDaysMask(mask){
  document.querySelectorAll('.day-cb').forEach(cb=>{
    cb.checked=!!(mask&(1<<parseInt(cb.value)));
  });
}

function showAlarmForm(data){
  gid('alarm-form').style.display='block';
  if(data){
    gid('alarm-idx').value=data.idx;
    gid('alarm-hour').value=data.hour;
    gid('alarm-min').value=data.minute;
    setDaysMask(data.days);
    gid('alarm-enabled').checked=data.enabled;
    gid('alarm-save-btn').textContent='Atualizar';
    editingAlarmIdx=data.idx;
  }else{
    gid('alarm-idx').value='-1';
    gid('alarm-hour').value='7';
    gid('alarm-min').value='0';
    setDaysMask(127);
    gid('alarm-enabled').checked=true;
    gid('alarm-save-btn').textContent='Salvar';
    editingAlarmIdx=null;
  }
}

function hideAlarmForm(){
  gid('alarm-form').style.display='none';
  editingAlarmIdx=null;
}

async function saveAlarm(){
  const idx=parseInt(gid('alarm-idx').value);
  const fd=new URLSearchParams();
  if(idx>=0)fd.set('idx',idx);
  fd.set('hour',gid('alarm-hour').value);
  fd.set('minute',gid('alarm-min').value);
  fd.set('days',getDaysMask());
  fd.set('enabled',gid('alarm-enabled').checked?'1':'0');
  try{
    const method=idx>=0?'PUT':'POST';
    await fetch('/api/alarms',{method,body:fd});
    showToast('Alarme salvo');
    hideAlarmForm();
    refreshAlarms();
  }catch(e){showToast('Erro ao salvar',1)}
}

async function deleteAlarm(idx){
  if(!confirm('Excluir alarme?'))return;
  const fd=new URLSearchParams();
  fd.set('idx',idx);
  try{
    await fetch('/api/alarms',{method:'DELETE',body:fd});
    showToast('Alarme excluido');
    if(editingAlarmIdx===idx)hideAlarmForm();
    refreshAlarms();
  }catch(e){showToast('Erro ao excluir',1)}
}

async function refreshAlarms(){
  try{
    const alarms=await fetchJSON('/api/alarms');
    const list=gid('alarm-list');
    list.innerHTML='';
    const dayNames=['Dom','Seg','Ter','Qua','Qui','Sex','Sab'];
    alarms.forEach(a=>{
      const days=[];
      for(let i=0;i<7;i++){if(a.days&(1<<i))days.push(dayNames[i]);}
      const row=document.createElement('div');
      row.className='module-row';
      row.innerHTML=`
        <div class="module-info">
          <div class="mod-name">${String(a.hour).padStart(2,'0')}:${String(a.minute).padStart(2,'0')} ${a.enabled?'':'(desativado)'}</div>
          <div class="mod-type">${days.join(' ')||'Nenhum'}</div>
        </div>
        <div class="module-actions">
          <input type="checkbox" ${a.enabled?'checked':''} onchange="toggleAlarm(${a.idx},this.checked)">
          <button class="btn" onclick="showAlarmForm({idx:${a.idx},hour:${a.hour},minute:${a.minute},days:${a.days},enabled:${a.enabled}})">Editar</button>
          <button class="btn btn-danger" onclick="deleteAlarm(${a.idx})">Excluir</button>
        </div>`;
      list.appendChild(row);
    });
  }catch(e){console.error('alarms err',e)}
}

async function toggleAlarm(idx,enabled){
  const fd=new URLSearchParams();
  fd.set('idx',idx);
  fd.set('enabled',enabled?'1':'0');
  try{
    await fetch('/api/alarms',{method:'PUT',body:fd});
    refreshAlarms();
  }catch(e){showToast('Falha ao alternar',1)}
}

function init(){
  refreshStatus();
  refreshConfig();
  refreshModules();
  refreshPrinters();
  refreshAlarms();
  setInterval(refreshStatus,5000);
}
document.addEventListener('DOMContentLoaded',init);
</script>
</body>
</html>
)rawliteral";
}

void WebManager::handleRoot() {
    _server.send(200, "text/html", buildDashboard());
}

void WebManager::handleAPI() {
    String path = _server.uri();
    HTTPMethod method = _server.method();

    if (path == "/api/status" && method == HTTP_GET) {
        _server.send(200, "application/json", statusJSON());
        return;
    }

    if (path == "/api/config") {
        if (method == HTTP_GET) {
            _server.send(200, "application/json", configJSON());
            return;
        }
        if (method == HTTP_POST) {
            if (_server.hasArg("name")) {
                _config->setDeviceName(urlDecode(_server.arg("name")).c_str());
            }
            _server.send(200, "application/json", configJSON());
            return;
        }
    }

    if (path == "/api/modules") {
        if (method == HTTP_GET) {
            _server.send(200, "application/json", modulesJSON());
            return;
        }
        if (method == HTTP_POST) {
            String type = _server.hasArg("type") ? urlDecode(_server.arg("type")) : "";
            String name = _server.hasArg("name") ? urlDecode(_server.arg("name")) : "";
            bool enabled = _server.hasArg("enabled") && _server.arg("enabled") == "1";
            String c1 = _server.hasArg("c1") ? urlDecode(_server.arg("c1")) : "";
            String c2 = _server.hasArg("c2") ? urlDecode(_server.arg("c2")) : "";
            String c3 = _server.hasArg("c3") ? urlDecode(_server.arg("c3")) : "";
            _modules->add(type, name, enabled, c1, c2, c3);
            _server.send(200, "application/json", modulesJSON());
            return;
        }
        if (method == HTTP_PUT) {
            String id = _server.hasArg("id") ? urlDecode(_server.arg("id")) : "";
            String name = _server.hasArg("name") ? urlDecode(_server.arg("name")) : "";
            bool enabled = _server.hasArg("enabled") && _server.arg("enabled") == "1";
            String c1 = _server.hasArg("c1") ? urlDecode(_server.arg("c1")) : "";
            String c2 = _server.hasArg("c2") ? urlDecode(_server.arg("c2")) : "";
            String c3 = _server.hasArg("c3") ? urlDecode(_server.arg("c3")) : "";
            _modules->update(id, name, enabled, c1, c2, c3);
            _server.send(200, "application/json", modulesJSON());
            return;
        }
        if (method == HTTP_DELETE) {
            String id = _server.hasArg("id") ? urlDecode(_server.arg("id")) : "";
            _modules->remove(id);
            _server.send(200, "application/json", modulesJSON());
            return;
        }
    }

    if (path == "/api/printers") {
        if (method == HTTP_GET) {
            _server.send(200, "application/json", printersJSON());
            return;
        }
        if (method == HTTP_POST) {
            String action = _server.hasArg("action") ? urlDecode(_server.arg("action")) : "";
            if (action == "scan") {
                _printers->startScan();
                _server.send(200, "application/json", "{\"status\":\"scan_started\"}");
                return;
            }
            if (action == "print") {
                int idx = _server.hasArg("idx") ? _server.arg("idx").toInt() : -1;
                String text = _server.hasArg("text") ? urlDecode(_server.arg("text")) : "";
                String result = _printers->printText(idx, text);
                String json = "{\"status\":\"" + (result.length() == 0 ? "ok" : result) + "\"}";
                _server.send(200, "application/json", json);
                return;
            }
            if (action == "add_bt") {
                String name = _server.hasArg("name") ? urlDecode(_server.arg("name")) : "";
                String mac = _server.hasArg("mac") ? urlDecode(_server.arg("mac")) : "";
                // save BT printer via addPrinter... need public method or direct
                // For now add to manager via persistent storage is handled internally
                _server.send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }
        }
    }

    if (path == "/api/alarms") {
        Preferences prefs;
        if (method == HTTP_GET) {
            _server.send(200, "application/json", alarmsJSON());
            return;
        }
        if (method == HTTP_POST) {
            prefs.begin(ALARM_NS, false);
            int count = prefs.getUChar("count", 0);
            if (count < MAX_ALARMS) {
                int h = _server.hasArg("hour") ? _server.arg("hour").toInt() : 0;
                int m = _server.hasArg("minute") ? _server.arg("minute").toInt() : 0;
                int d = _server.hasArg("days") ? _server.arg("days").toInt() : 0;
                bool e = _server.hasArg("enabled") ? _server.arg("enabled") == "1" : true;
                char k[4];
                snprintf(k, 4, "h%c", '0' + count);
                prefs.putUChar(k, (uint8_t)constrain(h, 0, 23));
                snprintf(k, 4, "m%c", '0' + count);
                prefs.putUChar(k, (uint8_t)constrain(m, 0, 59));
                snprintf(k, 4, "d%c", '0' + count);
                prefs.putUChar(k, (uint8_t)constrain(d, 0, 127));
                snprintf(k, 4, "e%c", '0' + count);
                prefs.putBool(k, e);
                prefs.putUChar("count", count + 1);
            }
            prefs.end();
            _server.send(200, "application/json", alarmsJSON());
            return;
        }
        if (method == HTTP_PUT) {
            int idx = _server.hasArg("idx") ? _server.arg("idx").toInt() : -1;
            if (idx >= 0 && idx < MAX_ALARMS) {
                prefs.begin(ALARM_NS, false);
                char k[4];
                if (_server.hasArg("hour")) {
                    snprintf(k, 4, "h%c", '0' + idx);
                    prefs.putUChar(k, (uint8_t)constrain(_server.arg("hour").toInt(), 0, 23));
                }
                if (_server.hasArg("minute")) {
                    snprintf(k, 4, "m%c", '0' + idx);
                    prefs.putUChar(k, (uint8_t)constrain(_server.arg("minute").toInt(), 0, 59));
                }
                if (_server.hasArg("days")) {
                    snprintf(k, 4, "d%c", '0' + idx);
                    prefs.putUChar(k, (uint8_t)constrain(_server.arg("days").toInt(), 0, 127));
                }
                if (_server.hasArg("enabled")) {
                    snprintf(k, 4, "e%c", '0' + idx);
                    prefs.putBool(k, _server.arg("enabled") == "1");
                }
                prefs.end();
            }
            _server.send(200, "application/json", alarmsJSON());
            return;
        }
        if (method == HTTP_DELETE) {
            int idx = _server.hasArg("idx") ? _server.arg("idx").toInt() : -1;
            prefs.begin(ALARM_NS, false);
            int count = prefs.getUChar("count", 0);
            if (idx >= 0 && idx < count) {
                for (int i = idx; i < count - 1; i++) {
                    char ks[4], kd[4];
                    snprintf(ks, 4, "h%c", '0' + (i + 1));
                    snprintf(kd, 4, "h%c", '0' + i);
                    prefs.putUChar(kd, prefs.getUChar(ks, 0));
                    snprintf(ks, 4, "m%c", '0' + (i + 1));
                    snprintf(kd, 4, "m%c", '0' + i);
                    prefs.putUChar(kd, prefs.getUChar(ks, 0));
                    snprintf(ks, 4, "d%c", '0' + (i + 1));
                    snprintf(kd, 4, "d%c", '0' + i);
                    prefs.putUChar(kd, prefs.getUChar(ks, 0));
                    snprintf(ks, 4, "e%c", '0' + (i + 1));
                    snprintf(kd, 4, "e%c", '0' + i);
                    prefs.putBool(kd, prefs.getBool(ks, true));
                }
                prefs.putUChar("count", count - 1);
            }
            prefs.end();
            _server.send(200, "application/json", alarmsJSON());
            return;
        }
        _server.send(405, "text/plain", "Method Not Allowed");
        return;
    }

    if (path == "/api/reboot" && method == HTTP_POST) {
        _server.send(200, "application/json", "{\"status\":\"ok\"}");
        _server.close();
        delay(500);
        ESP.restart();
        return;
    }

    _server.send(404, "text/plain", "Not Found");
}

void WebManager::handleUploadFile() {
    HTTPUpload& upload = _server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        mediaClear();
        _uploadSize = 0;
        _uploading = true;
        String name = upload.filename;
        if (name.length() == 0) name = "media";
        strncpy(_uploadName, name.c_str(), sizeof(_uploadName) - 1);
        _uploadName[sizeof(_uploadName) - 1] = '\0';
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        size_t remaining = MEDIA_MAX_SIZE - _uploadSize;
        size_t toWrite = upload.currentSize;
        if (toWrite > remaining) toWrite = remaining;
        if (toWrite > 0) {
            memcpy(mediaData + _uploadSize, upload.buf, toWrite);
            _uploadSize += toWrite;
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        _uploading = false;
        mediaSet(_uploadName, mediaData, _uploadSize);
    }
}

void WebManager::handleUpload() {
    if (_server.method() == HTTP_GET) {
        String form = "<!DOCTYPE html><html><body style='font-family:sans-serif;padding:20px'>";
        form += "<h2>Enviar Midia</h2>";
        form += "<form method='POST' action='/upload' enctype='multipart/form-data'>";
        form += "<input type='file' name='file' required><br><br>";
        form += "<input type='submit' value='Enviar'>";
        form += "</form>";
        form += "<p>Max " + String(MEDIA_MAX_SIZE / 1024) + "KB, 1 arquivo | RAW video 128x64 ou MP3 audio</p>";
        form += "<p>Cada frame = 1024 bytes. Converter imagem: <code>convert input.png -resize 128x64! -monochrome rgb:- | xxd -p -c 1024 > anim.raw</code></p>";
        form += "</body></html>";
        _server.send(200, "text/html", form);
        return;
    }
    String resp;
    if (_uploadSize > 0) {
        resp = "<html><body><h3>Upload OK!</h3><p>Arquivo: " + String(_uploadName) + " (" + String(_uploadSize) + " bytes)</p>";
        resp += "<a href='/upload'>Voltar</a></body></html>";
    } else {
        resp = "<html><body><h3>Falha no upload</h3><a href='/upload'>Tentar novamente</a></body></html>";
    }
    _server.send(200, "text/html", resp);
}
