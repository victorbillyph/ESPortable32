#!/usr/bin/env python3
"""ESPortable32 — Instalador/Reparador de firmware ESP32 com TUI Win98-style."""

import glob
import json
import os
import re
import subprocess
import sys
import threading
import time
import urllib.request

from textual.app import App
from textual.containers import Container
from textual.screen import Screen
from textual.widgets import (
    Button, RichLog, Static,
)


VERSION = "1.1.0"
_VERSION_URL = "https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/esportable32_tui.py"

_DEBUG = True
_LOG_FILE = os.path.expanduser("~/.local/share/esportable32/debug.log")

def _log(*args, **kw):
    if not _DEBUG: return
    try:
        os.makedirs(os.path.dirname(_LOG_FILE), exist_ok=True)
        t=time.strftime("%H:%M:%S")
        with open(_LOG_FILE,"a") as f:
            f.write(f"[{t}] {' '.join(str(a) for a in args)}\n")
    except: pass

try:
    import requests
except ImportError:
    requests = None


def _check_update(current):
    """Check GitHub for newer version. Returns (version_str, content) or None."""
    try:
        resp = urllib.request.urlopen(_VERSION_URL, timeout=10)
        src = resp.read().decode("utf-8")
        m = re.search(r'^VERSION\s*=\s*"([^"]+)"', src, re.M)
        if m and m.group(1) != current:
            return (m.group(1), src)
    except Exception: pass
    return None


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

    def _probe_port(self, s):
        s.write(b"STATUS\n"); time.sleep(0.5)
        out=b""
        while s.in_waiting: out+=s.read(s.in_waiting)
        return out

    def connect_serial(self, port, baud=115200):
        try: import serial
        except: return False, "pyserial nao instalado"
        _log(f"connect_serial({port},{baud})")
        try:
            s=serial.Serial(port,baud,timeout=2); s.dtr=False; time.sleep(0.1)
            s.dtr=True; time.sleep(2); s.reset_input_buffer()
            is_esp = lambda out: b"State:" in out or b"ESPortable" in out

            # Quick check: 3 tentativas rapidas pra detectar ESP ja pronto
            for i in range(3):
                out=self._probe_port(s)
                if out:
                    _log(f"connect_serial: quick check {i} got {len(out)} bytes: {out[:80]}")
                if is_esp(out):
                    self._serial=s; self.mode="serial"
                    _log(f"connect_serial({port}) OK (quick)")
                    return True, out.decode("utf-8",errors="replace")

            # Se e porta ACM/USB (ESP32 real), espera mais tempo pela janela de WiFi
            if any(x in port for x in ("ACM","USB","cu.","usb")):
                _log(f"connect_serial: {port} e ESP-like, retry por 11s...")
                for i in range(19):
                    out=self._probe_port(s)
                    if out:
                        _log(f"connect_serial: retry {i} got {len(out)} bytes: {out[:80]}")
                    if is_esp(out):
                        self._serial=s; self.mode="serial"
                        _log(f"connect_serial({port}) OK (apos WiFi)")
                        return True, out.decode("utf-8",errors="replace")
            s.close()
            _log(f"connect_serial({port}) FAIL: no ESP response")
            return False, "nao respondeu"
        except Exception as e:
            _log(f"connect_serial({port}) EXCEPTION: {e}")
            return False, str(e)

    def disconnect(self):
        _log("disconnect()")
        if self._serial:
            try: self._serial.close()
            except Exception: pass
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
        _log(f"_serial_cmd: {cmd[:60]}")
        with self._lock:
            self._serial.reset_input_buffer(); self._serial.write((cmd+"\n").encode()); time.sleep(0.3)
            out=b""; end=time.time()+timeout
            while time.time()<end:
                if self._serial.in_waiting: out+=self._serial.read(self._serial.in_waiting)
                time.sleep(0.05)
        resp=out.decode("utf-8",errors="replace")
        _log(f"_serial_cmd response: {resp[:100]}")
        return resp

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








# ── Boot sequence ─────────────────────────────────────────────────

class BIOSScreen(Screen):
    def compose(self):
        yield Container(
            RichLog(id="bios-log",highlight=True,markup=True),
            Static("",id="bios-prog"),
            classes="bios-box",
        )

    def on_mount(self):
        self._run()

    def _run(self):
        def task():
            self._log(f"[green]ESPortable32 BIOS v{VERSION}[/]")
            time.sleep(0.4)
            self._log("[green]Copyright (c) 2024[/]")
            time.sleep(0.3)
            self._log("")
            self._log("[green]CPU: Tensilica Xtensa LX6 @ 240MHz[/]")
            time.sleep(0.2)
            self._log("[green]DRAM: 520KB SRAM[/]")
            time.sleep(0.1)
            self._log("[green]Flash: 4MB SPI flash[/]")
            time.sleep(0.2)
            self._log("")
            self._log("[green]POST sequence...[/]")
            time.sleep(0.3)
            self._log("[green]  CPU check... OK[/]")
            time.sleep(0.1)
            self._log("[green]  Memory test... 520KB OK[/]")
            time.sleep(0.15)
            self._log("[green]  Flash ID... 0xEF4016 OK[/]")
            time.sleep(0.1)
            self._log("")
            self._log("[green]Boot device: Serial / USB[/]")
            time.sleep(0.3)
            self._log("")

            ports=[]
            try:
                import serial.tools.list_ports
                for p in serial.tools.list_ports.comports():
                    d=p.device
                    if any(x in d for x in ("ACM","USB","cu.","usb")):
                        ports.append(d)
                def _sort_key(x):
                    if 'ACM' in x: return 0
                    if 'USB' in x: return 1
                    if 'cu.' in x: return 2
                    return 3
                ports.sort(key=_sort_key)
            except Exception: pass
            if not ports:
                if sys.platform=="win32": ports=[f"COM{i}" for i in range(1,10)]
                elif sys.platform=="darwin": ports=["/dev/cu.usbmodem101","/dev/cu.usbserial-110"]
                else: ports=["/dev/ttyACM0","/dev/ttyACM1","/dev/ttyUSB0","/dev/ttyUSB1"]

            _log(f"BIOS scan: ports={ports}")
            self._log("[green]Scanning serial ports...[/]")
            for port in ports:
                self._prog(f"[green]Testing {port}...[/]")
                self._log(f"[green]  {port}... [/]",end=False)
                time.sleep(0.15)
                _log(f"BIOS: connecting to {port}")
                ok,resp=self.app.cli.connect_serial(port,115200)
                _log(f"BIOS: {port} result ok={ok} resp={resp[:80] if resp else ''}")
                if ok:
                    self.app.cli.mode="serial"
                    self._log("[bright_green]DETECTED[/]")
                    time.sleep(0.3)
                    self._log("")
                    self._log(f"[green]MAC: {port}[/]")
                    self._log("[green]Firmware: ESPortable32[/]")
                    self._log("")
                    self._log("[green]Boot device ready.[/]")
                    self._prog("")
                    time.sleep(0.5)
                    self.app.call_from_thread(self._found)
                    return
                self._log("[red]not found[/]")
            self._log("")
            self._log("[red]ERROR: No ESP32 found[/]")
            self._prog("")
            self.app.call_from_thread(self._skip_show)

        threading.Thread(target=task,daemon=True).start()

    def _log(self,msg,end=True):
        self.app.call_from_thread(self._write_log,msg)

    def _write_log(self,msg):
        log=self.query_one("#bios-log",RichLog)
        log.write(msg)

    def _prog(self,msg):
        self.app.call_from_thread(lambda: self.query_one("#bios-prog").update(msg))

    def _found(self):
        _log("BIOS: ESP32 found, switch_screen(found)")
        self.app.switch_screen("found")

    def _skip_show(self):
        _log("BIOS: ESP32 not found, switch_screen(repair)")
        self.app.switch_screen("repair")


class RepairScreen(Screen):
    def compose(self):
        yield Container(
            Container(
                Static("  Ferramentas de reparo",classes="rep-title"),
                Container(
                    Button("  Instalar firmware",id="rep-install",classes="win-btn"),
                    Button("  Reparar",id="rep-repair",classes="win-btn"),
                    Button("  Tentar novamente",id="rep-retry",classes="win-btn"),
                    Button("  Buscar updates",id="rep-update",classes="win-btn"),
                    Button("  Sair",id="rep-exit",classes="win-btn",variant="error"),
                    classes="rep-btns",
                ),
                RichLog(id="rep-log",highlight=True,markup=True,classes="rep-log"),
                classes="rep-window-inner",
            ),
            classes="rep-screen",
        )

    def on_mount(self):
        self._busy=False

    def on_button_pressed(self,e):
        if self._busy: return
        b=e.button.id
        if b=="rep-install": self._run_task("install")
        elif b=="rep-repair": self._run_task("repair")
        elif b=="rep-update": self._run_task("update")
        elif b=="rep-retry":
            self.dismiss()
            self.app.switch_screen("bios")
        elif b=="rep-exit": self.app.exit()

    def _log(self,msg):
        self.app.call_from_thread(lambda: self.query_one("#rep-log",RichLog).write(msg))

    def _run_task(self,task):
        self._busy=True
        self.query_one(".rep-btns").disabled=True
        self.query_one("#rep-log",RichLog).clear()
        threading.Thread(target=self._task_thread,args=(task,),daemon=True).start()

    def _enable_btns(self):
        self._busy=False
        self.query_one(".rep-btns").disabled=False

    def _task_thread(self,task):
        try:
            if task=="install": self._do_install()
            elif task=="repair": self._do_repair()
            elif task=="update": self._do_update()
        except Exception as ex:
            self._log(f"[red]Erro: {ex}[/]")
        finally:
            self.app.call_from_thread(self._enable_btns)

    def _find_pio(self):
        for p in ("/tmp/pio-venv/bin/pio",os.path.join(self._project_dir,".venv","bin","pio")):
            if os.path.exists(p): return p
        for cmd in ("pio","platformio"):
            try:
                subprocess.run([cmd,"--version"],capture_output=True,check=True)
                return cmd
            except Exception: pass
        return None

    def _find_esptool(self):
        for p in ("/tmp/pio-venv/bin/esptool.py","/tmp/pio-venv/bin/esptool",
                  os.path.join(self._project_dir,".venv","bin","esptool.py"),
                  os.path.join(self._project_dir,".venv","bin","esptool")):
            if os.path.exists(p): return p
        for cmd in ("esptool.py","esptool"):
            try:
                subprocess.run([cmd,"version"],capture_output=True,check=True)
                return cmd
            except Exception: pass
        return None

    def _find_port(self):
        ports=glob.glob("/dev/ttyACM*")+glob.glob("/dev/ttyUSB*")
        return ports[0] if ports else None

    def _run_cmd(self,args,caption):
        self._log(f"[cyan]{caption}[/]")
        self._log(f"[dim]{' '.join(args)}[/]")
        try:
            r=subprocess.run(args,capture_output=True,text=True,timeout=120)
            if r.stdout: self._log(r.stdout.strip())
            if r.stderr: self._log(f"[yellow]{r.stderr.strip()}[/]")
            if r.returncode!=0:
                self._log(f"[red]Falha (codigo {r.returncode})[/]")
                return False
            self._log("[green]OK[/]")
            return True
        except subprocess.TimeoutExpired:
            self._log("[red]Tempo esgotado[/]")
            return False
        except FileNotFoundError:
            self._log(f"[red]Comando nao encontrado: {args[0]}[/]")
            return False

    def _do_update(self):
        self._log("[cyan]Verificando atualizacoes...[/]")
        result = _check_update(VERSION)
        if not result:
            self._log("[green]Voce ja esta na versao mais recente![/]")
            return
        new_ver, src = result
        self._log(f"[green]Nova versao encontrada: v{new_ver}[/]")
        path = os.path.abspath(__file__)
        new_path = path + ".new"
        try:
            with open(new_path, "w") as f:
                f.write(src)
            os.replace(new_path, path)
            self._log(f"[green]Atualizado para v{new_ver}![/]")
            self._log("[yellow]Reinicie o app para usar a nova versao.[/]")
            self.app._update_info = None
        except Exception as ex:
            self._log(f"[red]Falha ao atualizar: {ex}[/]")

    def _do_install(self):
        self._project_dir=os.path.dirname(os.path.abspath(__file__))
        pio=self._find_pio()
        if not pio:
            self._log("[red]PlatformIO nao encontrado![/]")
            self._log("Instale com: pip install platformio")
            return
        port=self._find_port()
        if not port:
            self._log("[red]Nenhum ESP32 encontrado![/]")
            self._log("Conecte o cabo USB e tente novamente.")
            return
        self._log(f"[green]ESP32 detectado em: {port}[/]")
        self._log("")

        if not self._run_cmd([pio,"run","--project-dir",self._project_dir],"Compilando firmware..."):
            return

        data_dir=os.path.join(self._project_dir,"data")
        data_files=[f for f in os.listdir(data_dir) if f!=".gitkeep"] if os.path.isdir(data_dir) else []
        if data_files:
            self._run_cmd([pio,"run","--project-dir",self._project_dir,"--target","buildfs"],"Compilando LittleFS...")

        self._log("")
        self._log("[yellow]Apagando flash...[/]")
        esptool=self._find_esptool()
        if esptool:
            self._run_cmd([esptool,"--port",port,"--baud","921600","erase_flash"],"Apagando flash...")
        else:
            self._log("[red]esptool nao encontrado, tentando via pio...[/]")

        if not self._run_cmd([pio,"run","--project-dir",self._project_dir,"--target","upload"],"Gravando firmware..."):
            self._log("[red]Falha na gravacao![/]")
            return

        if data_files:
            self._run_cmd([pio,"run","--project-dir",self._project_dir,"--target","uploadfs"],"Enviando LittleFS...")

        self._log("")
        self._log("[green]Instalacao concluida![/]")
        self._log("[green]Desconecte e reconecte o USB para iniciar.[/]")

    def _do_repair(self):
        self._project_dir=os.path.dirname(os.path.abspath(__file__))
        port=self._find_port()
        if not port:
            self._log("[red]Nenhum ESP32 encontrado![/]")
            self._log("Conecte o cabo USB e tente novamente.")
            return
        self._log(f"[green]ESP32 detectado em: {port}[/]")
        self._log("")
        self._log("[cyan]Testando comunicacao...[/]")
        try:
            import serial
            s=serial.Serial(port,115200,timeout=3)
            s.dtr=False; time.sleep(0.1); s.dtr=True; time.sleep(2)
            log=b""
            start=time.time()
            while time.time()-start<8:
                if s.in_waiting: log+=s.read(s.in_waiting)
                time.sleep(0.1)
            s.close()
            txt=log.decode("utf-8",errors="replace")
            if txt.strip():
                self._log("[green]ESP32 responde![/]")
                for line in txt.splitlines():
                    self._log(f"  {line}")
                if "Modo Setup" in txt:
                    self._log("[yellow]Modo SETUP detectado[/]")
                    self._log("Use 'Instalar firmware' para reinstalar.")
                elif "Ready at" in txt:
                    self._log("[green]ESP32 ja configurado![/]")
                    self._log("Conecte via TCP ou reinicie o app.")
                else:
                    self._log("[yellow]Estado desconhecido[/]")
            else:
                self._log("[red]ESP32 nao responde via serial[/]")
        except ImportError:
            self._log("[red]pyserial nao instalado[/]")
        except Exception as ex:
            self._log(f"[red]Erro: {ex}[/]")
        self._log("")
        self._log("[cyan]Verificando com esptool...[/]")
        esptool=self._find_esptool()
        if esptool:
            self._run_cmd([esptool,"--port",port,"chip_id"],"Verificando chip...")
        else:
            self._log("[red]esptool nao encontrado[/]")


# ── Found Screen ─────────────────────────────────────────────────

class FoundScreen(Screen):
    def compose(self):
        yield Container(
            Container(
                Static("\n  ESPortable32 encontrado!",classes="fd-title"),
                Static("",id="fd-info"),
                Static("",id="fd-ip"),
                Container(
                    Button("  \u2197  Abrir no navegador",id="fd-browser",classes="win-btn"),
                    Button("  \u2691  Reparar",id="fd-repair",classes="win-btn"),
                    Button("  \u274C  Sair",id="fd-exit",classes="win-btn",variant="error"),
                    classes="fd-btns",
                ),
                classes="fd-box",
            ),
            classes="fd-screen",
        )

    def on_mount(self):
        self._ip = None
        cli = self.app.cli
        if cli.mode == "tcp":
            try:
                st = cli.status()
                self._ip = st.get("ip","") if isinstance(st,dict) else ""
                self.query_one("#fd-info").update("[green]Conectado via TCP[/]")
                self.query_one("#fd-ip").update(f"IP: [bold green]{self._ip}[/]")
            except Exception:
                self.query_one("#fd-info").update("[green]Conectado via TCP[/]")
        elif cli.mode == "serial":
            self.query_one("#fd-info").update("[green]Conectado via Serial[/]")
            try:
                resp = cli._serial_cmd("STATUS")
                if "IP:" in resp:
                    import re
                    m = re.search(r'IP:\s*([\d.]+)', resp)
                    if m:
                        self._ip = m.group(1)
                        self.query_one("#fd-ip").update(f"IP: [bold green]{self._ip}[/]")
                if "SETUP" in resp:
                    self.query_one("#fd-info").update("[yellow]Modo Setup — configure o WiFi abrindo o navegador[/]")
            except Exception: pass

    def on_button_pressed(self,e):
        b=e.button.id
        if b=="fd-browser":
            url = None
            if self._ip:
                url = f"http://{self._ip}"
            elif self.app.cli.mode=="tcp" and hasattr(self.app.cli,'_base'):
                url = self.app.cli._base
            if url:
                import webbrowser
                webbrowser.open(url)
        elif b=="fd-repair":
            self.app.push_screen("repair")
        elif b=="fd-exit":
            self.app.exit()


# ── App ───────────────────────────────────────────────────────────

class ESPortableTUI(App):
    CSS = """
    .bios-box { background:#000000; align:center top; padding:1; }
    #bios-log { background:#000000; color:#00ff00; height:1fr; border:none; }
    #bios-prog { background:#000000; color:#00aa00; height:1; dock:bottom; padding:0 2; }
    .bios-skip { dock:bottom; align:center bottom; }

    .win-btn { margin:0 1; }

    .rep-screen { align:center middle; }
    .rep-window-inner { width:64; height:auto; border:solid #000080; background:#c0c0c0; padding:1; }
    .rep-title { text-style:bold; background:#000080; color:white; padding:0 1; margin:0 0 1 0; }
    .rep-btns { height:auto; margin:0 0 1 0; }
    .rep-btns Button { margin:0 0 1 0; }
    .rep-log { border:solid #808080; height:10; margin:1 0; }

    .fd-screen { align:center middle; }
    .fd-box { width:52; height:auto; border:solid #000080; background:#c0c0c0; padding:1; }
    .fd-title { text-style:bold; color:#008000; text-align:center; }
    #fd-info { text-align:center; }
    #fd-ip { text-align:center; margin:0 0 1 0; }
    .fd-btns { align:center middle; margin:1 0; }
    .fd-btns Button { min-width:24; margin:0 0 1 0; }

    RichLog { border:solid #c0c0c0; height:1fr; }
    """

    SCREENS = {
        "bios": BIOSScreen,
        "found": FoundScreen,
        "repair": RepairScreen,
    }

    def __init__(self):
        super().__init__()
        self.cli = ESPClient()
        self._update_info = None
        self._reconnecting = False

    def on_ready(self):
        _log("App.on_ready: push_screen(bios)")
        self.push_screen("bios")
        self._check_update()

    def _check_update(self):
        def task():
            result = _check_update(VERSION)
            if result:
                self._update_info = result
                self.call_from_thread(self._notify_update, result[0])
        threading.Thread(target=task, daemon=True).start()

    def _notify_update(self, new_ver):
        self.notify(f"Update v{new_ver} disponivel! Va em Reparo para atualizar.", severity="information", timeout=10)

    def _apply_update(self):
        if not self._update_info:
            return
        _, src = self._update_info
        path = os.path.abspath(__file__)
        try:
            new_path = path + ".new"
            with open(new_path, "w") as f:
                f.write(src)
            os.replace(new_path, path)
            self.notify("Update aplicado! Reinicie o app.", severity="information")
            self._update_info = None
            return True
        except Exception as ex:
            self.notify(f"Falha ao atualizar: {ex}", severity="error")
            return False


def main():
    ESPortableTUI().run()

if __name__ == "__main__":
    main()
