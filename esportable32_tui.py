#!/usr/bin/env python3
"""ESPortable32 TUI — terminal UI for ESP32 via Textual."""

import asyncio
import json
import os
import sys
import threading
import time
from datetime import datetime
from typing import Optional

from textual import work
from textual.app import App, ComposeResult
from textual.binding import Binding
from textual.containers import Container, Horizontal, Vertical, ScrollableContainer, Grid
from textual.reactive import reactive
from textual.screen import Screen
from textual.widgets import (
    Button, Header, Footer, Input, Label, ListItem, ListView,
    RichLog, Static, TabbedContent, TabPane, TextArea,
)


# ── ESP32 Client ──────────────────────────────────────────────────

try:
    import requests
except ImportError:
    requests = None


class ESPClient:
    def __init__(self):
        self.mode = None
        self._serial = None
        self._base = None
        self._lock = threading.Lock()

    def connect_tcp(self, host, port=80):
        if not requests:
            return False, "requests nao instalado"
        self._base = f"http://{host}:{port}"
        try:
            r = requests.get(self._base + "/api/status", timeout=5)
            if r.status_code == 200:
                self.mode = "tcp"
                return True, r.json()
            return False, f"HTTP {r.status_code}"
        except Exception as e:
            return False, str(e)

    def connect_serial(self, port, baud=115200):
        try:
            import serial
        except ImportError:
            return False, "pyserial nao instalado"
        try:
            s = serial.Serial(port, baud, timeout=2)
            s.dtr = False
            time.sleep(0.1)
            s.dtr = True
            time.sleep(2)
            s.reset_input_buffer()
            s.write(b"STATUS\n")
            time.sleep(0.5)
            out = b""
            while s.in_waiting:
                out += s.read(s.in_waiting)
            if b"State:" in out or b"ESPortable" in out:
                self._serial = s
                self.mode = "serial"
                return True, out.decode("utf-8", errors="replace")
            s.close()
            return False, "ESP32 nao respondeu"
        except Exception as e:
            return False, str(e)

    def disconnect(self):
        if self._serial:
            try:
                self._serial.close()
            except:
                pass
        self._serial = None
        self._base = None
        self.mode = None

    def _tcp(self, method, path, **kwargs):
        if not self._base:
            raise Exception("Nao conectado")
        fn = getattr(requests, method.lower())
        url = self._base + path
        with self._lock:
            r = fn(url, timeout=10, **kwargs)
        if r.status_code >= 400:
            raise Exception(f"HTTP {r.status_code}: {r.text[:100]}")
        if r.text and r.headers.get("content-type", "").startswith("application/json"):
            return r.json()
        return r.text

    def _serial_cmd(self, cmd, timeout=2):
        if not self._serial:
            raise Exception("Nao conectado via serial")
        with self._lock:
            self._serial.reset_input_buffer()
            self._serial.write((cmd + "\n").encode())
            time.sleep(0.3)
            out = b""
            end = time.time() + timeout
            while time.time() < end:
                if self._serial.in_waiting:
                    out += self._serial.read(self._serial.in_waiting)
                time.sleep(0.05)
        return out.decode("utf-8", errors="replace")

    def api_get(self, path):
        if self.mode == "tcp":
            return self._tcp("GET", path)
        return self._serial_cmd(f"API {path}")

    def api_post(self, path, data=None):
        if self.mode == "tcp":
            return self._tcp("POST", path, json=data)
        return self._serial_cmd(f"API {path} {json.dumps(data) if data else ''}")

    def status(self):
        return self.api_get("/api/status")

    def gpio_list(self):
        return self.api_get("/api/gpio")

    def gpio_set(self, pin, state):
        return self.api_post("/api/gpio", {"pin": pin, "state": state})

    def gpio_set_mode(self, pin, mode):
        return self.api_post("/api/gpio", {"pin": pin, "mode": mode})

    def fs_list(self, path="/"):
        return self.api_get(f"/api/fs/list?path={path}")

    def fs_read(self, path):
        return self.api_get(f"/api/fs/read?path={path}")

    def fs_write(self, path, content):
        return self.api_post("/api/fs/write", {"path": path, "content": content})

    def fs_delete(self, path):
        if self.mode == "tcp":
            r = requests.delete(self._base + f"/api/fs/delete?path={path}", timeout=10)
            return r.json()
        return self._serial_cmd(f"API /api/fs/delete?path={path}")

    def apps(self):
        return self.api_get("/api/apps")

    def install_app(self, url, filename=None):
        body = {"url": url}
        if filename:
            body["path"] = filename
        return self.api_post("/api/apps", body)

    def unlock(self, pin):
        if self.mode == "tcp":
            import urllib.parse
            r = requests.post(self._base + "/api/unlock",
                              data=f"pin={urllib.parse.quote(pin)}",
                              headers={"Content-Type": "application/x-www-form-urlencoded"},
                              timeout=10)
            return r.json()
        return self._serial_cmd(f"PIN={pin}\nSAVE")

    def restart(self):
        return self.api_post("/api/restart")

    def proxy(self, url):
        return self.api_post("/api/proxy", {"url": url})

    def wifi_scan(self):
        try:
            return self.api_get("/api/wifi/scan")
        except:
            return []

    def save_config(self, ssid, passwd, pin="", name=""):
        if self.mode == "tcp":
            import urllib.parse
            body = f"ssid={urllib.parse.quote(ssid)}&pass={urllib.parse.quote(passwd)}"
            if pin:
                body += f"&pin={urllib.parse.quote(pin)}"
            r = requests.post(self._base + "/api/setup",
                              data=body,
                              headers={"Content-Type": "application/x-www-form-urlencoded"},
                              timeout=10)
            return r.text
        cmds = f"WIFI={ssid},{passwd}"
        if pin:
            cmds += f"\nPIN={pin}"
        if name:
            cmds += f"\nNAME={name}"
        cmds += "\nSAVE"
        return self._serial_cmd(cmds)


# ── TUI ───────────────────────────────────────────────────────────

TITLE = "ESPortable32"
SUB = "Terminal UI — ESP32"


class ScanScreen(Screen):
    """Auto-scan de portas seriais."""

    def compose(self):
        yield Header()
        yield Container(
            Static(f"\n  {TITLE}", classes="title"),
            Static(f"  {SUB}", classes="subtitle"),
            Static(""),
            Static("Procurando ESP32 via Serial...", id="scan-msg"),
            Static("", id="scan-detail"),
            Button("Pular (inserir manualmente)", id="scan-skip"),
            classes="connect-box",
        )
        yield Footer()

    def on_mount(self):
        self._scan()

    def _scan(self):
        def scan():
            ports_to_try = []
            try:
                import serial.tools.list_ports
                ports = serial.tools.list_ports.comports()
                for p in ports:
                    ports_to_try.append(p.device)
            except Exception:
                pass
            if not ports_to_try:
                if sys.platform == "win32":
                    ports_to_try = [f"COM{i}" for i in range(1, 10)]
                elif sys.platform == "darwin":
                    ports_to_try = ["/dev/cu.usbmodem101", "/dev/cu.usbserial-110",
                                    "/dev/cu.SLAB_USBtoUART", "/dev/cu.wchusbserial*"]
                else:
                    ports_to_try = ["/dev/ttyACM0", "/dev/ttyACM1",
                                    "/dev/ttyUSB0", "/dev/ttyUSB1",
                                    "/dev/ttyS0", "/dev/ttyS1"]

            for port in ports_to_try:
                self.call_from_thread(self.query_one("#scan-detail").update, f"Testando {port}...")
                time.sleep(0.1)
                ok, _ = self.app.cli.connect_serial(port, 115200)
                if ok:
                    self.app.cli.mode = "serial"
                    self.call_from_thread(self.app.push_screen, "main")
                    return

            if self.app.cli.mode != "serial":
                self.call_from_thread(self._show_connect)

        threading.Thread(target=scan, daemon=True).start()

    def _show_connect(self):
        self.app.pop_screen()
        self.app.push_screen("connect")

    def on_button_pressed(self, event):
        if event.button.id == "scan-skip":
            self._show_connect()


class ConnectScreen(Screen):
    """Conexao TCP/IP ou Serial manual."""

    def compose(self):
        yield Header()
        yield Container(
            Static(f"\n  {TITLE}", classes="title"),
            Static(f"  {SUB}", classes="subtitle"),
            Static("", id="manual-msg"),
            TabbedContent(
                TabPane("TCP/IP", id="tcp"),
                TabPane("Serial", id="serial"),
            ),
            Static("", id="status"),
            Button("Conectar", variant="primary", id="connect"),
            classes="connect-box",
        )
        yield Footer()

    def on_mount(self):
        self.query_one("#connect").disabled = True
        self._build_tcp()
        self._build_serial()
        self._active_tab = "tcp"

    def _build_tcp(self):
        pane = self.query_one("#tcp")
        pane.mount(
            Label("Endereco IP"),
            Input(placeholder="192.168.2.38", id="tcp-host"),
            Label("Porta"),
            Input(placeholder="80", id="tcp-port"),
        )

    def _build_serial(self):
        pane = self.query_one("#serial")
        pane.mount(
            Label("Porta Serial"),
            Input(placeholder="/dev/ttyACM0", id="ser-port"),
            Label("Baud rate"),
            Input(placeholder="115200", id="ser-baud"),
        )

    def on_tabbed_content_tab_changed(self, event):
        self._active_tab = event.pane.id

    def on_input_changed(self, event):
        if self._active_tab == "tcp":
            host = self.query_one("#tcp-host").value.strip()
            self.query_one("#connect").disabled = not host
        else:
            port = self.query_one("#ser-port").value.strip()
            self.query_one("#connect").disabled = not port

    def on_button_pressed(self, event):
        if event.button.id == "connect":
            self._connect()

    def _connect(self):
        status = self.query_one("#status")
        if self._active_tab == "tcp":
            host = self.query_one("#tcp-host").value.strip()
            port = self.query_one("#tcp-port").value.strip() or "80"
            status.update("Conectando via TCP/IP...")
            ok, result = self.app.cli.connect_tcp(host, int(port))
        else:
            port = self.query_one("#ser-port").value.strip()
            baud = self.query_one("#ser-baud").value.strip() or "115200"
            status.update("Conectando via Serial...")
            ok, result = self.app.cli.connect_serial(port, int(baud))
        if ok:
            self.app.push_screen("main")
        else:
            status.update(f"Erro: {result}")
            self.query_one("#connect").disabled = False


class DashboardPane(ScrollableContainer):
    """Dashboard com cards de informacao."""

    def __init__(self):
        super().__init__()
        self._data = {}

    def compose(self):
        yield Static("Dashboard", classes="pane-title")
        yield Grid(
            Static("", id="d-status", classes="dash-card"),
            Static("", id="d-ip", classes="dash-card"),
            Static("", id="d-ssid", classes="dash-card"),
            Static("", id="d-heap", classes="dash-card"),
            Static("", id="d-rssi", classes="dash-card"),
            Static("", id="d-uptime", classes="dash-card"),
            Static("", id="d-cpu", classes="dash-card"),
            Static("", id="d-mode", classes="dash-card"),
            classes="dash-grid",
        )
        yield Button("Atualizar", id="dash-refresh")

    def update_data(self, data):
        if not isinstance(data, dict):
            return
        self._data = data
        self.query_one("#d-status").update(f"[bold]Estado[/]\n{data.get('status', '--')}")
        self.query_one("#d-ip").update(f"[bold]IP[/]\n{data.get('ip', '--')}")
        self.query_one("#d-ssid").update(f"[bold]WiFi[/]\n{data.get('ssid', '--')}")
        heap = data.get("free_heap", "--")
        self.query_one("#d-heap").update(f"[bold]Heap[/]\n{heap} bytes" if isinstance(heap, int) else f"[bold]Heap[/]\n{heap}")
        self.query_one("#d-rssi").update(f"[bold]RSSI[/]\n{data.get('wifi_rssi', '--')} dBm")
        uptime = data.get("uptime", "--")
        self.query_one("#d-uptime").update(f"[bold]Uptime[/]\n{uptime}s" if isinstance(uptime, (int, float)) else f"[bold]Uptime[/]\n{uptime}")
        self.query_one("#d-cpu").update(f"[bold]CPU[/]\n{data.get('cpu_freq', '--')} MHz")
        self.query_one("#d-mode").update(f"[bold]Modo[/]\n{data.get('state_name', '--')}")


class GPIOPane(ScrollableContainer):
    """Controle GPIO."""

    PINS = [2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33]

    def __init__(self):
        super().__init__()
        self._states = {}

    def compose(self):
        yield Static("GPIO", classes="pane-title")
        yield Static("Clique para ligar/desligar", id="gpio-sub")
        yield Grid(*[
            Button(f"GPIO {p}\n--", id=f"gpio-{p}", classes="gpio-btn")
            for p in self.PINS
        ], classes="gpio-grid")
        yield Static("", id="gpio-msg")

    def on_mount(self):
        self._refresh()

    def on_button_pressed(self, event):
        for p in self.PINS:
            if event.button.id == f"gpio-{p}":
                self._toggle(p)
                break

    def _refresh(self):
        try:
            data = self.app.cli.gpio_list()
            if isinstance(data, list):
                for item in data:
                    p = item.get("pin")
                    state = item.get("state", 0)
                    if p:
                        self._states[p] = state
                        btn = self.query_one(f"#gpio-{p}")
                        label = "ON" if state else "OFF"
                        btn.classes = f"gpio-btn {'on' if state else 'off'}"
                        btn.label = f"GPIO {p}\n{label}"
        except Exception:
            pass

    def _toggle(self, pin):
        new = 0 if self._states.get(pin) else 1
        try:
            r = self.app.cli.gpio_set(pin, new)
            if isinstance(r, dict) and r.get("status") == "ok":
                self._states[pin] = new
                btn = self.query_one(f"#gpio-{pin}")
                label = "ON" if new else "OFF"
                btn.classes = f"gpio-btn {'on' if new else 'off'}"
                btn.label = f"GPIO {pin}\n{label}"
                self.query_one("#gpio-msg").update(f"GPIO {pin} -> {'LIGADO' if new else 'DESLIGADO'}")
        except Exception as e:
            self.query_one("#gpio-msg").update(f"Erro: {e}")


class EditorPane(ScrollableContainer):
    """Editor de arquivos no LittleFS."""

    def compose(self):
        yield Static("Editor de Arquivos", classes="pane-title")
        yield Horizontal(
            Vertical(
                Label("Arquivos"),
                ListView(id="file-list"),
                Button("Listar", id="file-list-btn"),
                Button("Deletar", id="file-del-btn", variant="error"),
                classes="file-sidebar",
            ),
            Vertical(
                Label("Conteudo"),
                Horizontal(
                    Input(placeholder="caminho/arquivo.txt", id="file-path"),
                    Button("Carregar", id="file-load"),
                    Button("Salvar", id="file-save", variant="primary"),
                ),
                TextArea(id="file-content"),
                Static("", id="file-msg"),
                classes="file-editor",
            ),
        )

    def on_mount(self):
        self._list_files()

    async def on_button_pressed(self, event):
        btn = event.button.id
        if btn == "file-list-btn":
            self._list_files()
        elif btn == "file-del-btn":
            await self._delete_file()
        elif btn == "file-load":
            self._load_file()
        elif btn == "file-save":
            self._save_file()

    def on_list_view_selected(self, event):
        item = event.item
        name = str(item.label).split(" (")[0]
        path = "/" + name if not name.startswith("/") else name
        self.query_one("#file-path", Input).value = path
        self._load_file()

    def _list_files(self):
        try:
            data = self.app.cli.fs_list("/")
            lv = self.query_one("#file-list", ListView)
            lv.clear()
            if isinstance(data, list):
                for f in data:
                    if not f.get("dir"):
                        name = f.get("name", "").lstrip("/")
                        size = f.get("size", 0)
                        lv.append(ListItem(Label(f"{name} ({size} B)")))
                self.query_one("#file-msg").update(f"{len(lv.children)} arquivos")
        except Exception as e:
            self.query_one("#file-msg").update(f"Erro: {e}")

    def _load_file(self):
        path = self.query_one("#file-path", Input).value.strip()
        if not path.startswith("/"):
            path = "/" + path
        try:
            data = self.app.cli.fs_read(path)
            if isinstance(data, dict) and "content" in data:
                self.query_one("#file-content", TextArea).text = data["content"]
                self.query_one("#file-msg").update(f"Carregado: {path}")
        except Exception as e:
            self.query_one("#file-msg").update(f"Erro: {e}")

    def _save_file(self):
        path = self.query_one("#file-path", Input).value.strip()
        if not path.startswith("/"):
            path = "/" + path
        content = self.query_one("#file-content", TextArea).text
        try:
            r = self.app.cli.fs_write(path, content)
            if isinstance(r, dict) and r.get("status") == "ok":
                self.query_one("#file-msg").update(f"Salvo: {path}")
                self._list_files()
        except Exception as e:
            self.query_one("#file-msg").update(f"Erro: {e}")

    async def _delete_file(self):
        lv = self.query_one("#file-list", ListView)
        if lv.index is None:
            return
        item = lv.children[lv.index]
        name = str(item.label).split(" (")[0]
        path = "/" + name if not name.startswith("/") else name
        try:
            r = self.app.cli.fs_delete(path)
            if isinstance(r, dict) and r.get("status") == "ok":
                self.query_one("#file-msg").update(f"Deletado: {path}")
                self._list_files()
        except Exception as e:
            self.query_one("#file-msg").update(f"Erro: {e}")


class ConfigPane(ScrollableContainer):
    """Configuracoes WiFi / PIN."""

    def compose(self):
        yield Static("Configuracoes", classes="pane-title")
        yield Static("WiFi", classes="section-title")
        yield Label("SSID")
        yield Input(placeholder="nome da rede", id="cfg-ssid")
        yield Label("Senha")
        yield Input(placeholder="senha", id="cfg-pass", password=True)
        yield Static("Seguranca", classes="section-title")
        yield Label("PIN")
        yield Input(placeholder="PIN de bloqueio", id="cfg-pin", password=True)
        yield Label("Nome do dispositivo")
        yield Input(placeholder="ESPortable32", id="cfg-name")
        yield Horizontal(
            Button("Salvar Config", variant="primary", id="cfg-save"),
            Button("Reiniciar ESP32", variant="error", id="cfg-restart"),
            Button("Status", id="cfg-status"),
        )
        yield RichLog(id="cfg-log", highlight=True, markup=True)

    def on_button_pressed(self, event):
        btn = event.button.id
        if btn == "cfg-save":
            self._save()
        elif btn == "cfg-restart":
            self._restart()
        elif btn == "cfg-status":
            self._status()

    def _log(self, msg):
        self.query_one("#cfg-log", RichLog).write(msg)

    def _save(self):
        ssid = self.query_one("#cfg-ssid", Input).value.strip()
        passwd = self.query_one("#cfg-pass", Input).value.strip()
        pin = self.query_one("#cfg-pin", Input).value.strip()
        name = self.query_one("#cfg-name", Input).value.strip()
        if not ssid:
            self._log("[red]Informe o SSID[/]")
            return
        try:
            r = self.app.cli.save_config(ssid, passwd, pin, name)
            self._log(f"[green]Config salva:[/] {r}")
        except Exception as e:
            self._log(f"[red]Erro:[/] {e}")

    def _restart(self):
        try:
            self.app.cli.restart()
            self._log("[yellow]Reiniciando ESP32...[/]")
        except Exception as e:
            self._log(f"[red]Erro:[/] {e}")

    def _status(self):
        try:
            s = self.app.cli.status()
            if isinstance(s, dict):
                for k, v in s.items():
                    self._log(f"[bold]{k}:[/] {v}")
            else:
                self._log(str(s))
        except Exception as e:
            self._log(f"[red]Erro:[/] {e}")


class StorePane(ScrollableContainer):
    """Loja de apps."""

    def compose(self):
        yield Static("Loja de Apps", classes="pane-title")
        yield Horizontal(
            Button("Buscar apps", variant="primary", id="store-fetch"),
            Button("Instalados", id="store-installed"),
        )
        yield Horizontal(
            Vertical(
                Label("Disponiveis"),
                ListView(id="store-list"),
                classes="store-col",
            ),
            Vertical(
                Label("Instalados"),
                ListView(id="store-list-installed"),
                classes="store-col",
            ),
        )
        yield Button("Instalar selecionado", id="store-install")
        yield Static("", id="store-msg")

    def on_mount(self):
        self._apps = []

    def on_button_pressed(self, event):
        btn = event.button.id
        if btn == "store-fetch":
            self._fetch()
        elif btn == "store-installed":
            self._installed()
        elif btn == "store-install":
            self._install()

    def _fetch(self):
        msg = self.query_one("#store-msg")
        msg.update("Buscando...")
        try:
            import urllib.request
            url = "https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/apps/manifest.json"
            r = urllib.request.urlopen(url, timeout=10)
            data = json.loads(r.read())
            self._apps = data.get("apps", [])
            lv = self.query_one("#store-list", ListView)
            lv.clear()
            for a in self._apps:
                icon = a.get("icon", "")
                lv.append(ListItem(Label(f"{icon} {a['name']} - {a.get('desc','')}")))
            msg.update(f"{len(self._apps)} apps encontrados")
        except Exception as e:
            msg.update(f"Erro: {e}")

    def _installed(self):
        try:
            data = self.app.cli.apps()
            lv = self.query_one("#store-list-installed", ListView)
            lv.clear()
            if isinstance(data, dict):
                for a in data.get("apps", []):
                    lv.append(ListItem(Label(f"{a['id']} ({a.get('size',0)} B)")))
                self.query_one("#store-msg").update(f"{len(data['apps'])} apps instalados")
        except Exception as e:
            self.query_one("#store-msg").update(f"Erro: {e}")

    def _install(self):
        lv = self.query_one("#store-list", ListView)
        if lv.index is None or not self._apps:
            return
        app = self._apps[lv.index]
        app_name = app["name"]
        app_file = app.get("file", f"{app['id']}.html")
        url = f"https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/apps/{app_file}"
        msg = self.query_one("#store-msg")
        msg.update(f"Instalando {app_name}...")
        try:
            r = self.app.cli.install_app(url)
            msg.update(f" OK {app_name} instalado!")
            self._installed()
        except Exception as e:
            msg.update(f"Erro: {e}")


class TerminalPane(ScrollableContainer):
    """Terminal para comandos diretos."""

    def compose(self):
        yield Static("Terminal", classes="pane-title")
        yield RichLog(id="term-log", highlight=True, markup=True, max_lines=500)
        yield Horizontal(
            Input(placeholder="GET /api/path ou POST /api/path {...}", id="term-input"),
            Button("Enviar", id="term-send"),
            Button("Limpar", id="term-clear"),
        )

    def on_button_pressed(self, event):
        if event.button.id == "term-send":
            self._send()
        elif event.button.id == "term-clear":
            self.query_one("#term-log", RichLog).clear()

    def on_input_submitted(self, event):
        if event.input.id == "term-input":
            self._send()

    def _send(self):
        inp = self.query_one("#term-input", Input)
        cmd = inp.value.strip()
        if not cmd:
            return
        inp.value = ""
        log = self.query_one("#term-log", RichLog)
        log.write(f"[bold]>[/] {cmd}")
        try:
            if self.app.cli.mode == "serial":
                resp = self.app.cli._serial_cmd(cmd)
            elif self.app.cli.mode == "tcp":
                if cmd.startswith("GET "):
                    resp = self.app.cli.api_get(cmd[4:])
                elif cmd.startswith("POST "):
                    rest = cmd[5:]
                    space = rest.index(" ")
                    path = rest[:space]
                    body = rest[space + 1:]
                    resp = self.app.cli.api_post(path, json.loads(body))
                else:
                    resp = "Comandos: GET /path, POST /path {json}"
            else:
                resp = "Nao conectado"
            log.write(str(resp))
        except Exception as e:
            log.write(f"[red]Erro:[/] {e}")


class MainScreen(Screen):
    """Tela principal com abas."""

    BINDINGS = [
        Binding("d", "switch_tab('dashboard')", "Dashboard"),
        Binding("g", "switch_tab('gpio')", "GPIO"),
        Binding("e", "switch_tab('editor')", "Editor"),
        Binding("c", "switch_tab('config')", "Config"),
        Binding("s", "switch_tab('store')", "Store"),
        Binding("t", "switch_tab('terminal')", "Terminal"),
        Binding("q", "app.pop_screen", "Desconectar"),
    ]

    def compose(self):
        yield Header(show_clock=True)
        yield TabbedContent(
            TabPane("Dashboard", DashboardPane(), id="dashboard"),
            TabPane("GPIO", GPIOPane(), id="gpio"),
            TabPane("Editor", EditorPane(), id="editor"),
            TabPane("Config", ConfigPane(), id="config"),
            TabPane("Store", StorePane(), id="store"),
            TabPane("Terminal", TerminalPane(), id="terminal"),
        )
        yield Footer()

    def on_mount(self):
        self.set_interval(3, self._refresh_dash)
        self._refresh_dash()

    def _refresh_dash(self):
        try:
            data = self.app.cli.status()
            dash = self.query_one(DashboardPane)
            dash.update_data(data)
        except Exception:
            pass

    def action_switch_tab(self, tab):
        tc = self.query_one(TabbedContent)
        tc.active = tab


class ESPortableTUI(App):
    """Aplicacao TUI ESPortable32."""

    CSS = """
    .connect-box {
        align: center top;
        padding: 1 2;
        max-width: 60;
    }
    .connect-box Static.title {
        text-style: bold;
        color: $primary;
        text-align: center;
    }
    .connect-box Static.subtitle {
        color: $text-muted;
        text-align: center;
    }
    .connect-box TabbedContent {
        height: auto;
        margin: 1 0;
    }
    .connect-box Input {
        margin: 0 0 1 0;
    }
    .connect-box Button {
        margin: 1 0;
    }
    .connect-box #status {
        text-align: center;
        color: $error;
    }
    .dash-grid {
        grid-size: 4;
        grid-gutter: 1;
        height: auto;
    }
    .dash-card {
        border: solid $primary;
        padding: 1;
        height: 5;
        text-align: center;
    }
    .gpio-grid {
        grid-size: 5;
        grid-gutter: 1;
        height: auto;
        margin: 1 0;
    }
    .gpio-btn {
        min-width: 10;
    }
    .gpio-btn.on {
        background: green;
        color: white;
    }
    .gpio-btn.off {
        background: darkred;
        color: white;
    }
    .pane-title {
        text-style: bold;
        padding: 0 0 1 0;
    }
    .section-title {
        text-style: bold;
        color: $text-muted;
        padding: 1 0 0 0;
    }
    .file-sidebar {
        width: 30;
        margin: 0 1 0 0;
    }
    .file-editor {
        height: 100%;
    }
    .file-editor TextArea {
        height: 1fr;
    }
    .store-col {
        width: 1fr;
        margin: 0 1;
    }
    #gpio-sub {
        color: $text-muted;
    }
    Button {
        margin: 0 1 0 0;
    }
    Horizontal {
        height: auto;
        margin: 0 0 1 0;
    }
    ListView {
        height: 12;
    }
    RichLog {
        border: solid $surface;
        height: 1fr;
    }
    """

    SCREENS = {
        "scan": ScanScreen,
        "connect": ConnectScreen,
        "main": MainScreen,
    }

    def __init__(self):
        super().__init__()
        self.cli = ESPClient()

    def on_ready(self):
        self.push_screen("scan")


def main():
    app = ESPortableTUI()
    app.run()


if __name__ == "__main__":
    main()
