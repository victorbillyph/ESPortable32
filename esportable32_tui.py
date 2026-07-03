#!/usr/bin/env python3
"""ESPortable32 Desktop TUI — Win98-style terminal interface for ESP32."""

import json
import os
import sys
import threading
import time

from textual import work
from textual.app import App, ComposeResult
from textual.binding import Binding
from textual.containers import Container, Horizontal, Vertical, Grid
from textual.screen import Screen, ModalScreen
from textual.widgets import (
    Button, Header, Footer, Input, Label, ListItem, ListView,
    RichLog, Static, TextArea,
)

try:
    import requests
except ImportError:
    requests = None


# ── ESP32 Client ──────────────────────────────────────────────────

class ESPClient:
    def __init__(self):
        self.mode = None; self._serial = None; self._base = None
        self._lock = threading.Lock()

    def connect_tcp(self, host, port=80):
        if not requests: return False, "requests nao instalado"
        self._base = f"http://{host}:{port}"
        try:
            r = requests.get(self._base+"/api/status", timeout=5)
            if r.status_code==200: self.mode="tcp"; return True, r.json()
            return False, f"HTTP {r.status_code}"
        except Exception as e: return False, str(e)

    def connect_serial(self, port, baud=115200):
        try: import serial
        except: return False, "pyserial nao instalado"
        try:
            s=serial.Serial(port,baud,timeout=2); s.dtr=False; time.sleep(0.1)
            s.dtr=True; time.sleep(2); s.reset_input_buffer()
            s.write(b"STATUS\n"); time.sleep(0.5); out=b""
            while s.in_waiting: out+=s.read(s.in_waiting)
            if b"State:" in out or b"ESPortable" in out:
                self._serial=s; self.mode="serial"; return True, out.decode("utf-8",errors="replace")
            s.close(); return False, "nao respondeu"
        except Exception as e: return False, str(e)

    def disconnect(self):
        if self._serial:
            try: self._serial.close()
            except: pass
        self._serial=None; self._base=None; self.mode=None

    def _tcp(self, method, path, **kw):
        if not self._base: raise Exception("Nao conectado")
        fn=getattr(requests,method.lower())
        with self._lock: r=fn(self._base+path,timeout=10,**kw)
        if r.status_code>=400: raise Exception(f"HTTP {r.status_code}")
        if r.text and r.headers.get("content-type","").startswith("application/json"): return r.json()
        return r.text

    def _serial_cmd(self, cmd, timeout=2):
        if not self._serial: raise Exception("Nao conectado")
        with self._lock:
            self._serial.reset_input_buffer(); self._serial.write((cmd+"\n").encode()); time.sleep(0.3)
            out=b""; end=time.time()+timeout
            while time.time()<end:
                if self._serial.in_waiting: out+=self._serial.read(self._serial.in_waiting)
                time.sleep(0.05)
        return out.decode("utf-8",errors="replace")

    def api_get(self, path):
        if self.mode=="tcp": return self._tcp("GET", path)
        return self._serial_cmd(f"API {path}")

    def api_post(self, path, data=None):
        if self.mode=="tcp": return self._tcp("POST", path, json=data)
        return self._serial_cmd(f"API {path} {json.dumps(data) if data else ''}")

    def status(self): return self.api_get("/api/status")
    def gpio_list(self): return self.api_get("/api/gpio")
    def gpio_set(self,p,s): return self.api_post("/api/gpio",{"pin":p,"state":s})
    def fs_list(self,p="/"): return self.api_get(f"/api/fs/list?path={p}")
    def fs_read(self,p): return self.api_get(f"/api/fs/read?path={p}")
    def fs_write(self,p,c): return self.api_post("/api/fs/write",{"path":p,"content":c})
    def fs_delete(self,p):
        if self.mode=="tcp":
            r=requests.delete(self._base+f"/api/fs/delete?path={p}",timeout=10)
            return r.json()
        return self._serial_cmd(f"API /api/fs/delete?path={p}")
    def apps(self): return self.api_get("/api/apps")
    def install_app(self,url,fn=None):
        b={"url":url}
        if fn: b["path"]=fn
        return self.api_post("/api/apps",b)
    def unlock(self,pin):
        if self.mode=="tcp":
            import urllib.parse
            r=requests.post(self._base+"/api/unlock",data=f"pin={urllib.parse.quote(pin)}",
                          headers={"Content-Type":"application/x-www-form-urlencoded"},timeout=10)
            return r.json()
        return self._serial_cmd(f"PIN={pin}\nSAVE")
    def restart(self): return self.api_post("/api/restart")
    def proxy(self,url): return self.api_post("/api/proxy",{"url":url})
    def save_config(self,ssid,passwd,pin="",name=""):
        if self.mode=="tcp":
            import urllib.parse
            b=f"ssid={urllib.parse.quote(ssid)}&pass={urllib.parse.quote(passwd)}"
            if pin: b+=f"&pin={urllib.parse.quote(pin)}"
            r=requests.post(self._base+"/api/setup",data=b,
                          headers={"Content-Type":"application/x-www-form-urlencoded"},timeout=10)
            return r.text
        cmds=f"WIFI={ssid},{passwd}"
        if pin: cmds+=f"\nPIN={pin}"
        if name: cmds+=f"\nNAME={name}"
        cmds+="\nSAVE"
        return self._serial_cmd(cmds)


# ── App Screens ───────────────────────────────────────────────────

class DashboardScreen(Screen):
    def compose(self):
        yield Static("Dashboard", classes="win-title")
        yield Grid(
            Static("",id="t-status",classes="dash-card"),
            Static("",id="t-ip",classes="dash-card"),
            Static("",id="t-ssid",classes="dash-card"),
            Static("",id="t-heap",classes="dash-card"),
            Static("",id="t-rssi",classes="dash-card"),
            Static("",id="t-uptime",classes="dash-card"),
            Static("",id="t-cpu",classes="dash-card"),
            Static("",id="t-mode",classes="dash-card"),
            classes="dash-grid",
        )
        yield Button("Atualizar",id="dash-refresh",classes="win-btn")
        yield Button("Fechar",id="dash-close",classes="win-btn",variant="error")

    def on_mount(self):
        self._refresh()
        self.set_interval(3, self._refresh)

    def _refresh(self):
        try:
            d = self.app.cli.status()
            if not isinstance(d,dict): return
            self.query_one("#t-status").update(f"[bold]Estado[/]\n{d.get('status','--')}")
            self.query_one("#t-ip").update(f"[bold]IP[/]\n{d.get('ip','--')}")
            self.query_one("#t-ssid").update(f"[bold]WiFi[/]\n{d.get('ssid','--')}")
            self.query_one("#t-heap").update(f"[bold]Heap[/]\n{d.get('free_heap','--')} bytes")
            self.query_one("#t-rssi").update(f"[bold]RSSI[/]\n{d.get('wifi_rssi','--')} dBm")
            self.query_one("#t-uptime").update(f"[bold]Uptime[/]\n{d.get('uptime','--')}s")
            self.query_one("#t-cpu").update(f"[bold]CPU[/]\n{d.get('cpu_freq','--')} MHz")
            self.query_one("#t-mode").update(f"[bold]Modo[/]\n{d.get('state_name','--')}")
        except: pass

    def on_button_pressed(self, e):
        if e.button.id=="dash-close": self.app.pop_screen()


class GPIOScreen(Screen):
    PINS = [2,4,5,12,13,14,15,16,17,18,19,21,22,23,25,26,27,32,33]
    def compose(self):
        yield Static("GPIO Control", classes="win-title")
        yield Grid(*[Button(f"GPIO {p}\n--",id=f"gpio-{p}",classes="gpio-btn") for p in self.PINS],
                  classes="gpio-grid")
        yield Static("",id="gpio-msg")
        yield Button("Fechar",id="gpio-close",classes="win-btn",variant="error")

    def on_mount(self):
        self._states={}
        self._refresh()

    def _refresh(self):
        try:
            d=self.app.cli.gpio_list()
            if isinstance(d,list):
                for item in d:
                    p=item.get("pin"); s=item.get("state",0)
                    if p and self.query_one(f"#gpio-{p}",Button):
                        self._states[p]=s
                        b=self.query_one(f"#gpio-{p}")
                        b.label=f"GPIO {p}\n{'ON' if s else 'OFF'}"
                        b.classes=f"gpio-btn {'on' if s else 'off'}"
        except: pass

    def on_button_pressed(self, e):
        for p in self.PINS:
            if e.button.id==f"gpio-{p}":
                new=0 if self._states.get(p) else 1
                try:
                    r=self.app.cli.gpio_set(p,new)
                    if isinstance(r,dict) and r.get("status")=="ok":
                        self._states[p]=new
                        b=self.query_one(f"#gpio-{p}")
                        b.label=f"GPIO {p}\n{'ON' if new else 'OFF'}"
                        b.classes=f"gpio-btn {'on' if new else 'off'}"
                        self.query_one("#gpio-msg").update(f"GPIO {p} -> {'LIGADO' if new else 'DESLIGADO'}")
                except Exception as ex: self.query_one("#gpio-msg").update(f"Erro: {ex}")
                return
        if e.button.id=="gpio-close": self.app.pop_screen()


class EditorScreen(Screen):
    def compose(self):
        yield Static("Editor de Arquivos", classes="win-title")
        yield Horizontal(
            Vertical(
                Label("Arquivos"),
                ListView(id="file-list"),
                Button("Listar",id="file-list-btn",classes="win-btn"),
                Button("Deletar",id="file-del-btn",classes="win-btn",variant="error"),
                classes="file-side",
            ),
            Vertical(
                Horizontal(
                    Input(placeholder="arquivo.txt",id="file-path"),
                    Button("Carregar",id="file-load",classes="win-btn"),
                    Button("Salvar",id="file-save",classes="win-btn"),
                ),
                TextArea(id="file-content"),
                Static("",id="file-msg"),
            ),
        )
        yield Button("Fechar",id="file-close",classes="win-btn",variant="error")

    def on_mount(self): self._list()

    async def on_button_pressed(self, e):
        b=e.button.id
        if b=="file-list-btn": self._list()
        elif b=="file-del-btn": await self._delete()
        elif b=="file-load": self._load()
        elif b=="file-save": self._save()
        elif b=="file-close": self.app.pop_screen()

    def on_list_view_selected(self, e):
        n=str(e.item.label).split(" (")[0]
        self.query_one("#file-path",Input).value=("/"+n if not n.startswith("/") else n)
        self._load()

    def _list(self):
        try:
            d=self.app.cli.fs_list("/"); lv=self.query_one("#file-list",ListView); lv.clear()
            if isinstance(d,list):
                for f in d:
                    if not f.get("dir"):
                        lv.append(ListItem(Label(f"{f.get('name','').lstrip('/')} ({f.get('size',0)} B)")))
                self.query_one("#file-msg").update(f"{len(lv.children)} arquivos")
        except Exception as ex: self.query_one("#file-msg").update(f"Erro: {ex}")

    def _load(self):
        n=self.query_one("#file-path",Input).value.strip()
        if not n: return
        if not n.startswith("/"): n="/"+n
        try:
            d=self.app.cli.fs_read(n)
            if isinstance(d,dict) and "content" in d:
                self.query_one("#file-content",TextArea).text=d["content"]
                self.query_one("#file-msg").update(f"Carregado: {n}")
        except Exception as ex: self.query_one("#file-msg").update(f"Erro: {ex}")

    def _save(self):
        n=self.query_one("#file-path",Input).value.strip()
        if not n: return
        if not n.startswith("/"): n="/"+n
        c=self.query_one("#file-content",TextArea).text
        try:
            r=self.app.cli.fs_write(n,c)
            if isinstance(r,dict) and r.get("status")=="ok":
                self.query_one("#file-msg").update(f"Salvo: {n}"); self._list()
        except Exception as ex: self.query_one("#file-msg").update(f"Erro: {ex}")

    async def _delete(self):
        lv=self.query_one("#file-list",ListView)
        if lv.index is None: return
        n=str(lv.children[lv.index].label).split(" (")[0]
        if not n.startswith("/"): n="/"+n
        try:
            r=self.app.cli.fs_delete(n)
            if isinstance(r,dict) and r.get("status")=="ok":
                self.query_one("#file-msg").update(f"Deletado: {n}"); self._list()
        except Exception as ex: self.query_one("#file-msg").update(f"Erro: {ex}")


class ConfigScreen(Screen):
    def compose(self):
        yield Static("Configuracoes", classes="win-title")
        yield Label("WiFi")
        yield Label("SSID"); yield Input(placeholder="SSID",id="cfg-ssid")
        yield Label("Senha"); yield Input(placeholder="senha",id="cfg-pass",password=True)
        yield Label("Seguranca")
        yield Label("PIN"); yield Input(placeholder="PIN",id="cfg-pin",password=True)
        yield Label("Nome"); yield Input(placeholder="dispositivo",id="cfg-name")
        yield Horizontal(
            Button("Salvar",id="cfg-save",classes="win-btn",variant="primary"),
            Button("Reiniciar",id="cfg-restart",classes="win-btn",variant="error"),
            Button("Status",id="cfg-status",classes="win-btn"),
            Button("Fechar",id="cfg-close",classes="win-btn",variant="error"),
        )
        yield RichLog(id="cfg-log",highlight=True,markup=True)

    def on_button_pressed(self, e):
        b=e.button.id
        if b=="cfg-save": self._save()
        elif b=="cfg-restart": self._restart()
        elif b=="cfg-status": self._status()
        elif b=="cfg-close": self.app.pop_screen()

    def _log(self,m): self.query_one("#cfg-log",RichLog).write(m)

    def _save(self):
        s=self.query_one("#cfg-ssid",Input).value.strip()
        p=self.query_one("#cfg-pass",Input).value.strip()
        pin=self.query_one("#cfg-pin",Input).value.strip()
        n=self.query_one("#cfg-name",Input).value.strip()
        if not s: self._log("[red]SSID obrigatorio[/]"); return
        try: r=self.app.cli.save_config(s,p,pin,n); self._log(f"[green]Salvo:[/] {r}")
        except Exception as ex: self._log(f"[red]Erro:[/] {ex}")

    def _restart(self):
        try: self.app.cli.restart(); self._log("[yellow]Reiniciando...[/]")
        except Exception as ex: self._log(f"[red]Erro:[/] {ex}")

    def _status(self):
        try:
            s=self.app.cli.status()
            if isinstance(s,dict):
                for k,v in s.items(): self._log(f"[bold]{k}:[/] {v}")
            else: self._log(str(s))
        except Exception as ex: self._log(f"[red]Erro:[/] {ex}")


class StoreScreen(Screen):
    def compose(self):
        yield Static("Loja de Apps", classes="win-title")
        yield Horizontal(
            Button("Buscar",id="store-fetch",classes="win-btn"),
            Button("Instalados",id="store-installed",classes="win-btn"),
            Button("Instalar",id="store-install",classes="win-btn"),
            Button("Fechar",id="store-close",classes="win-btn",variant="error"),
        )
        yield Horizontal(
            Vertical(Label("Disponiveis"),ListView(id="store-list"),classes="store-col"),
            Vertical(Label("Instalados"),ListView(id="store-list-installed"),classes="store-col"),
        )
        yield Static("",id="store-msg")

    def on_mount(self): self._apps=[]

    def on_button_pressed(self,e):
        b=e.button.id
        if b=="store-fetch": self._fetch()
        elif b=="store-installed": self._installed()
        elif b=="store-install": self._install()
        elif b=="store-close": self.app.pop_screen()

    def _fetch(self):
        self.query_one("#store-msg").update("Buscando...")
        try:
            import urllib.request
            u="https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/apps/manifest.json"
            r=urllib.request.urlopen(u,timeout=10); d=json.loads(r.read())
            self._apps=d.get("apps",[]); lv=self.query_one("#store-list",ListView); lv.clear()
            for a in self._apps: lv.append(ListItem(Label(f"{a.get('icon','')} {a['name']} - {a.get('desc','')}")))
            self.query_one("#store-msg").update(f"{len(self._apps)} apps")
        except Exception as ex: self.query_one("#store-msg").update(f"Erro: {ex}")

    def _installed(self):
        try:
            d=self.app.cli.apps(); lv=self.query_one("#store-list-installed",ListView); lv.clear()
            if isinstance(d,dict):
                for a in d.get("apps",[]): lv.append(ListItem(Label(f"{a['id']} ({a.get('size',0)} B)")))
                self.query_one("#store-msg").update(f"{len(d['apps'])} instalados")
        except Exception as ex: self.query_one("#store-msg").update(f"Erro: {ex}")

    def _install(self):
        lv=self.query_one("#store-list",ListView)
        if lv.index is None or not self._apps: return
        a=self._apps[lv.index]
        u=f"https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/apps/{a.get('file',a['id']+'.html')}"
        self.query_one("#store-msg").update(f"Instalando {a['name']}...")
        try: self.app.cli.install_app(u); self.query_one("#store-msg").update(f"OK {a['name']}!"); self._installed()
        except Exception as ex: self.query_one("#store-msg").update(f"Erro: {ex}")


class TerminalScreen(Screen):
    def compose(self):
        yield Static("Terminal", classes="win-title")
        yield RichLog(id="term-log",highlight=True,markup=True,max_lines=500)
        yield Horizontal(
            Input(placeholder="GET /api/... ou POST /api/... {...}",id="term-input"),
            Button("Enviar",id="term-send",classes="win-btn"),
            Button("Limpar",id="term-clear",classes="win-btn"),
            Button("Fechar",id="term-close",classes="win-btn",variant="error"),
        )

    def on_button_pressed(self,e):
        if e.button.id=="term-send": self._send()
        elif e.button.id=="term-clear": self.query_one("#term-log",RichLog).clear()
        elif e.button.id=="term-close": self.app.pop_screen()

    def on_input_submitted(self,e):
        if e.input.id=="term-input": self._send()

    def _send(self):
        inp=self.query_one("#term-input",Input); cmd=inp.value.strip()
        if not cmd: return
        inp.value=""; log=self.query_one("#term-log",RichLog); log.write(f"[bold]>[/] {cmd}")
        try:
            if self.app.cli.mode=="serial": resp=self.app.cli._serial_cmd(cmd)
            elif self.app.cli.mode=="tcp":
                if cmd.startswith("GET "): resp=self.app.cli.api_get(cmd[4:])
                elif cmd.startswith("POST "):
                    rest=cmd[5:]; sp=rest.index(" "); resp=self.app.cli.api_post(rest[:sp],json.loads(rest[sp+1:]))
                else: resp="GET /path ou POST /path {...}"
            else: resp="Nao conectado"
            log.write(str(resp))
        except Exception as ex: log.write(f"[red]Erro:[/] {ex}")


# ── Scan / Connect / Desktop ──────────────────────────────────────

class ScanScreen(Screen):
    def compose(self):
        yield Container(
            Static("\n  ESPortable32",classes="scan-title"),
            Static("  Procurando ESP32...\n",classes="scan-sub"),
            Static("",id="scan-detail"),
            Button("Pular (inserir manualmente)",id="scan-skip"),
            classes="scan-box",
        )

    def on_mount(self): self._scan()

    def _scan(self):
        def scan():
            ports=[]
            try:
                import serial.tools.list_ports
                for p in serial.tools.list_ports.comports(): ports.append(p.device)
            except: pass
            if not ports:
                if sys.platform=="win32": ports=[f"COM{i}" for i in range(1,10)]
                elif sys.platform=="darwin": ports=["/dev/cu.usbmodem101","/dev/cu.usbserial-110"]
                else: ports=["/dev/ttyACM0","/dev/ttyACM1","/dev/ttyUSB0","/dev/ttyUSB1"]
            for port in ports:
                self.call_from_thread(self.query_one("#scan-detail").update,f"Testando {port}...")
                time.sleep(0.1)
                ok,_=self.app.cli.connect_serial(port,115200)
                if ok:
                    self.app.cli.mode="serial"
                    self.call_from_thread(self.app.push_screen,"desktop")
                    return
            if self.app.cli.mode!="serial":
                self.call_from_thread(self._show_connect)
        threading.Thread(target=scan,daemon=True).start()

    def _show_connect(self):
        self.app.switch_screen("connect")

    def on_button_pressed(self,e):
        if e.button.id=="scan-skip": self._show_connect()


class ConnectScreen(Screen):
    def compose(self):
        yield Container(
            Static("\n  ESPortable32",classes="scan-title"),
            Static("  Conexao manual\n",classes="scan-sub"),
            Static("",id="manual-msg"),
            Label("IP:"), Input(placeholder="192.168.2.38",id="con-ip"),
            Label("Porta:"), Input(placeholder="80",id="con-port"),
            Label("Ou Serial:"), Input(placeholder="/dev/ttyACM0",id="con-serial"),
            Button("Conectar TCP",id="con-tcp",classes="win-btn",variant="primary"),
            Button("Conectar Serial",id="con-serial-btn",classes="win-btn"),
            classes="scan-box",
        )

    def on_button_pressed(self,e):
        if e.button.id=="con-tcp":
            h=self.query_one("#con-ip",Input).value.strip(); p=self.query_one("#con-port",Input).value.strip() or "80"
            if not h: return
            ok,_=self.app.cli.connect_tcp(h,int(p))
            if ok: self.app.switch_screen("desktop")
        elif e.button.id=="con-serial-btn":
            p=self.query_one("#con-serial",Input).value.strip()
            if not p: return
            ok,_=self.app.cli.connect_serial(p,115200)
            if ok: self.app.switch_screen("desktop")


class DesktopScreen(Screen):
    BINDINGS = [
        Binding("escape","start_menu","Menu"),
    ]

    DESKTOP_ICONS = [
        ("\u2302  Painel",   lambda s: s.app.push_screen("dashboard")),
        ("\u26A1  GPIO",     lambda s: s.app.push_screen("gpio")),
        ("\u270E  Editor",   lambda s: s.app.push_screen("editor")),
        ("\u2699  Config",   lambda s: s.app.push_screen("config")),
        ("\u2728  Loja",     lambda s: s.app.push_screen("store")),
        ("\u2328  Terminal", lambda s: s.app.push_screen("terminal")),
    ]

    def compose(self):
        yield Container(
            *[Button(label,id=f"desk-{i}",classes="desk-icon") for i,(label,_) in enumerate(self.DESKTOP_ICONS)],
            classes="desktop",
        )
        yield Container(
            Button("  \u25A0  Iniciar  ",id="start-btn",classes="task-btn"),
            Static("",id="task-apps",classes="task-apps"),
            Static("",id="task-clock",classes="task-clock"),
            classes="taskbar",
        )

    def on_mount(self):
        self._update_clock()
        self.set_interval(60, self._update_clock)
        self._update_taskbar()
        self._watch_connection()

    def _watch_connection(self):
        if not self.app.cli.mode:
            self.app.switch_screen("connect")
        else:
            self.set_timer(2, self._watch_connection)

    def _update_clock(self):
        self.query_one("#task-clock").update(time.strftime("%H:%M"))

    def _update_taskbar(self):
        apps=" | ".join([l for l,_ in self.DESKTOP_ICONS])
        self.query_one("#task-apps").update(apps)

    def on_button_pressed(self,e):
        if e.button.id=="start-btn":
            self.action_start_menu()
        elif e.button.id and e.button.id.startswith("desk-"):
            i=int(e.button.id.split("-")[1])
            if 0<=i<len(self.DESKTOP_ICONS):
                self.DESKTOP_ICONS[i][1](self)

    def action_start_menu(self):
        def item(t,action):
            b=Button(t,classes="start-item"); yield b
        self.app.push_screen("start_menu")

    def action_switch_tab(self,tab):
        pass


class StartMenuScreen(Screen):
    BINDINGS = [Binding("escape","close","Fechar")]

    def compose(self):
        yield Container(
            Button("  \u2302  Painel",id="sm-dashboard",classes="start-item"),
            Button("  \u26A1  GPIO",id="sm-gpio",classes="start-item"),
            Button("  \u270E  Editor",id="sm-editor",classes="start-item"),
            Button("  \u2699  Config",id="sm-config",classes="start-item"),
            Button("  \u2728  Loja",id="sm-store",classes="start-item"),
            Button("  \u2328  Terminal",id="sm-terminal",classes="start-item"),
            Static("",classes="start-divider"),
            Button("  \u2B95  Desconectar",id="sm-disconnect",classes="start-item"),
            Button("  \u274C  Sair",id="sm-quit",classes="start-item"),
            classes="start-menu",
        )

    def on_button_pressed(self,e):
        b={"sm-dashboard":"dashboard","sm-gpio":"gpio","sm-editor":"editor",
           "sm-config":"config","sm-store":"store","sm-terminal":"terminal"}
        if e.button.id in b:
            self.app.pop_screen()
            self.app.push_screen(b[e.button.id])
        elif e.button.id=="sm-disconnect":
            self.app.cli.disconnect()
            self.app.pop_screen()
            self.app.switch_screen("scan")
        elif e.button.id=="sm-quit":
            self.app.exit()

    def action_close(self):
        self.app.pop_screen()


# ── App ───────────────────────────────────────────────────────────

class ESPortableTUI(App):
    CSS = """
    .scan-box { align:center top; padding:2 4; max-width:60; }
    .scan-title { text-style:bold; color:$primary; text-align:center; }
    .scan-sub { color:$text-muted; text-align:center; }

    .desktop { background:#008080; align:center top; padding:1; height:1fr; }
    .desk-icon { min-width:16; margin:1 2; background:$surface; border:solid $primary; }
    .desk-icon:hover { background:$primary; color:white; }

    .taskbar { background:$surface; height:3; dock:bottom; padding:0 1; }
    .task-btn { min-width:14; }
    .task-apps { color:$text-muted; padding:0 1; }
    .task-clock { color:$text; padding:0 1; dock:right; }

    .start-menu { background:$surface; border:solid $primary; padding:1; width:30; }
    .start-item { width:100%; margin:0; }
    .start-item:hover { background:$primary; color:white; }
    .start-divider { height:1; background:$text-muted; margin:1 0; }

    .win-title { text-style:bold; background:$primary; color:white; padding:0 1; }
    .win-btn { margin:0 1; }

    .dash-grid { grid-size:4; grid-gutter:1; height:auto; padding:1; }
    .dash-card { border:solid $primary; padding:1; height:5; text-align:center; }

    .gpio-grid { grid-size:5; grid-gutter:1; height:auto; padding:1; }
    .gpio-btn.on { background:green; color:white; }
    .gpio-btn.off { background:darkred; color:white; }

    .file-side { width:30; margin:0 1; }

    .store-col { width:1fr; margin:0 1; }

    Horizontal { height:auto; margin:0 0 1 0; }
    Vertical { height:auto; }
    ListView { height:10; }
    RichLog { border:solid $surface; height:1fr; }
    Input { margin:0 0 1 0; }
    Label { padding:0; }
    """

    SCREENS = {
        "scan": ScanScreen,
        "connect": ConnectScreen,
        "desktop": DesktopScreen,
        "start_menu": StartMenuScreen,
        "dashboard": DashboardScreen,
        "gpio": GPIOScreen,
        "editor": EditorScreen,
        "config": ConfigScreen,
        "store": StoreScreen,
        "terminal": TerminalScreen,
    }

    def __init__(self):
        super().__init__()
        self.cli = ESPClient()

    def on_ready(self):
        self.push_screen("scan")


def main():
    ESPortableTUI().run()

if __name__ == "__main__":
    main()
