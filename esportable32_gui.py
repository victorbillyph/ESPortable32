#!/usr/bin/env python3
"""ESPortable32 Desktop GUI — comunicação com ESP32 via TCP/IP ou Serial."""

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import threading
import time
import json
import os
import sys
from datetime import datetime

try:
    import requests
except ImportError:
    requests = None

PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))

VERDE = "#4caf50"
VERMELHO = "#e94560"
AZUL = "#0f3460"
FUNDO = "#1a1a2e"
CARD = "#16213e"
TEXTO = "#eee"
TEXTO2 = "#888"

# ── Cliente ESP32 ──────────────────────────────────────────────────

class ESPClient:
    def __init__(self):
        self.mode = None
        self._serial = None
        self._base = None
        self._lock = threading.Lock()

    # -- TCP
    def connect_tcp(self, host, port=80):
        if not requests:
            return False, "requests não instalado (pip install requests)"
        self._base = f"http://{host}:{port}"
        try:
            r = requests.get(self._base + "/api/status", timeout=5)
            if r.status_code == 200:
                self.mode = "tcp"
                return True, r.json()
            return False, f"HTTP {r.status_code}"
        except Exception as e:
            return False, str(e)

    # -- Serial
    def connect_serial(self, port, baud=115200):
        try:
            import serial
        except ImportError:
            return False, "pyserial não instalado (pip install pyserial)"
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
            return False, "ESP32 não respondeu"
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
            raise Exception("Não conectado")
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
            raise Exception("Não conectado via serial")
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

    # -- API wrappers
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


# ── GUI ────────────────────────────────────────────────────────────

class ESPortable32GUI:
    def __init__(self):
        self.cli = ESPClient()
        self._auto_refresh = False
        self._running = True

        self.root = tk.Tk()
        self.root.title("ESPortable32")
        self.root.geometry("900x620")
        self.root.minsize(700, 500)
        self.root.configure(bg=FUNDO)

        try:
            self.root.iconbitmap(default=os.path.join(PROJECT_DIR, "icon.ico"))
        except:
            pass

        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TNotebook", background=FUNDO, borderwidth=0)
        style.configure("TNotebook.Tab", background=CARD, foreground=TEXTO,
                        padding=[12, 4], borderwidth=0)
        style.map("TNotebook.Tab", background=[("selected", VERMELHO)],
                  foreground=[("selected", "#fff")])
        style.configure("TFrame", background=FUNDO)
        style.configure("TLabel", background=FUNDO, foreground=TEXTO)
        style.configure("TLabelframe", background=FUNDO, foreground=TEXTO, borderwidth=1)
        style.configure("TLabelframe.Label", background=FUNDO, foreground=TEXTO)

        self._show_scanning()
        self.root.after(100, self._auto_scan)
        self.root.protocol("WM_DELETE_WINDOW", self._quit)
        self.root.mainloop()

    def _quit(self):
        self._running = False
        self.cli.disconnect()
        self.root.destroy()

    def _show_scanning(self):
        for w in self.root.winfo_children():
            w.destroy()
        self.root.geometry("400x250")

        frame = tk.Frame(self.root, bg=FUNDO)
        frame.pack(expand=True, fill="both", padx=40, pady=40)

        tk.Label(frame, text="ESPortable32", font=("", 22, "bold"),
                 fg=VERMELHO, bg=FUNDO).pack(pady=(0, 10))

        self._scan_label = tk.Label(frame, text="Procurando ESP32...",
                                    font=("", 12), fg=TEXTO2, bg=FUNDO)
        self._scan_label.pack(pady=(0, 20))

        self._scan_progress = ttk.Progressbar(frame, mode="indeterminate", length=250)
        self._scan_progress.pack()
        self._scan_progress.start(10)

        self._scan_btn = tk.Button(frame, text="Pular (inserir manualmente)",
                                   bg=AZUL, fg=TEXTO, relief="flat", cursor="hand2",
                                   padx=15, pady=5, command=self._show_connect)
        self._scan_btn.pack(pady=(20, 0))

        self._scan_status = tk.Label(frame, text="", fg=TEXTO2, bg=FUNDO, font=("", 9))
        self._scan_status.pack(pady=(10, 0))

    def _auto_scan(self):
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

            self.root.after(0, lambda: self._scan_status.config(text=f"Tentando {len(ports_to_try)} portas..."))

            for port in ports_to_try:
                self.root.after(0, lambda p=port: self._scan_status.config(text=f"Testando {p}..."))
                time.sleep(0.1)
                ok, _ = self.cli.connect_serial(port, 115200)
                if ok:
                    self.cli.mode = "serial"
                    self.root.after(0, lambda: self._on_connect(None))
                    return

            if self.cli.mode != "serial":
                self.root.after(0, lambda: self._scan_status.config(text=""))
                self.root.after(0, self._show_connect)

        threading.Thread(target=scan, daemon=True).start()

    def _show_connect(self, msg=None):
        for w in self.root.winfo_children():
            w.destroy()
        self.root.geometry("500x430")

        frame = tk.Frame(self.root, bg=FUNDO)
        frame.pack(expand=True, fill="both", padx=40, pady=40)

        if msg:
            tk.Label(frame, text=msg, fg=TEXTO2, bg=FUNDO,
                     font=("", 9), wraplength=400).pack(pady=(0, 10))

        tk.Label(frame, text="ESPortable32", font=("", 22, "bold"),
                 fg=VERMELHO, bg=FUNDO).pack(pady=(0, 5))
        tk.Label(frame, text="Conectar ao ESP32", font=("", 12),
                 fg=TEXTO2, bg=FUNDO).pack(pady=(0, 20))

        notebook = ttk.Notebook(frame)
        notebook.pack(fill="both", expand=True)

        # TCP tab
        tcp_frame = tk.Frame(notebook, bg=CARD, padx=20, pady=20)
        notebook.add(tcp_frame, text=" TCP/IP ")

        tk.Label(tcp_frame, text="Endereço IP", fg=TEXTO2, bg=CARD,
                 anchor="w").pack(fill="x")
        self.tcp_host = tk.Entry(tcp_frame, bg=AZUL, fg=TEXTO, insertbackground=TEXTO,
                                  font=("", 11), relief="flat", bd=8)
        self.tcp_host.insert(0, "192.168.2.38")
        self.tcp_host.pack(fill="x", pady=(2, 10))

        tk.Label(tcp_frame, text="Porta", fg=TEXTO2, bg=CARD,
                 anchor="w").pack(fill="x")
        self.tcp_port = tk.Entry(tcp_frame, bg=AZUL, fg=TEXTO, insertbackground=TEXTO,
                                  font=("", 11), relief="flat", bd=8)
        self.tcp_port.insert(0, "80")
        self.tcp_port.pack(fill="x", pady=(2, 15))

        self.tcp_btn = tk.Button(tcp_frame, text="Conectar TCP/IP",
                                 bg=VERMELHO, fg="#fff", relief="flat",
                                 font=("", 10, "bold"), cursor="hand2",
                                 padx=20, pady=8, command=self._connect_tcp)
        self.tcp_btn.pack()

        # Serial tab
        ser_frame = tk.Frame(notebook, bg=CARD, padx=20, pady=20)
        notebook.add(ser_frame, text=" Serial ")

        tk.Label(ser_frame, text="Porta Serial", fg=TEXTO2, bg=CARD,
                 anchor="w").pack(fill="x")
        self.ser_port = tk.Entry(ser_frame, bg=AZUL, fg=TEXTO, insertbackground=TEXTO,
                                  font=("", 11), relief="flat", bd=8)
        self.ser_port.insert(0, "/dev/ttyACM0")
        self.ser_port.pack(fill="x", pady=(2, 10))

        tk.Label(ser_frame, text="Baud rate", fg=TEXTO2, bg=CARD,
                 anchor="w").pack(fill="x")
        self.ser_baud = tk.Entry(ser_frame, bg=AZUL, fg=TEXTO, insertbackground=TEXTO,
                                  font=("", 11), relief="flat", bd=8)
        self.ser_baud.insert(0, "115200")
        self.ser_baud.pack(fill="x", pady=(2, 15))

        self.ser_btn = tk.Button(ser_frame, text="Conectar Serial",
                                 bg=VERMELHO, fg="#fff", relief="flat",
                                 font=("", 10, "bold"), cursor="hand2",
                                 padx=20, pady=8, command=self._connect_serial)
        self.ser_btn.pack()

        self.con_status = tk.Label(frame, text="", fg=TEXTO2, bg=FUNDO, font=("", 9))
        self.con_status.pack(pady=(15, 0))

    def _connect_tcp(self):
        host = self.tcp_host.get().strip()
        port = self.tcp_port.get().strip()
        if not host:
            self.con_status.config(text="Informe o IP", fg=VERMELHO)
            return
        self.tcp_btn.config(state="disabled", text="Conectando...")
        self.root.update()
        ok, result = self.cli.connect_tcp(host, int(port) if port else 80)
        if ok:
            self._on_connect(result)
        else:
            self.con_status.config(text=f"Erro: {result}", fg=VERMELHO)
            self.tcp_btn.config(state="normal", text="Conectar TCP/IP")

    def _connect_serial(self):
        port = self.ser_port.get().strip()
        baud = self.ser_baud.get().strip()
        if not port:
            self.con_status.config(text="Informe a porta serial", fg=VERMELHO)
            return
        self.ser_btn.config(state="disabled", text="Conectando...")
        self.root.update()
        ok, result = self.cli.connect_serial(port, int(baud) if baud else 115200)
        if ok:
            self._on_connect(None)
        else:
            self.con_status.config(text=f"Erro: {result}", fg=VERMELHO)
            self.ser_btn.config(state="normal", text="Conectar Serial")

    def _on_connect(self, status_data):
        for w in self.root.winfo_children():
            w.destroy()
        self.root.geometry("940x680")

        # Header
        header = tk.Frame(self.root, bg=CARD, height=40)
        header.pack(fill="x")
        header.pack_propagate(False)
        tk.Label(header, text="ESPortable32", font=("", 13, "bold"),
                 fg=VERMELHO, bg=CARD).pack(side="left", padx=15)
        self.conn_label = tk.Label(header, text=f"Conectado via {self.cli.mode.upper()}",
                                   font=("", 9), fg=TEXTO2, bg=CARD)
        self.conn_label.pack(side="left", padx=5)
        tk.Button(header, text="Desconectar", bg=AZUL, fg=TEXTO,
                  relief="flat", cursor="hand2", padx=10,
                  command=self._disconnect).pack(side="right", padx=10)

        # Notebook
        nb = ttk.Notebook(self.root)
        nb.pack(fill="both", expand=True, padx=8, pady=8)

        self._build_dashboard(nb)
        self._build_gpio(nb)
        self._build_editor(nb)
        self._build_settings(nb)
        if self.cli.mode == "tcp":
            self._build_store(nb)
        self._build_terminal(nb)

        self._auto_refresh = True
        threading.Thread(target=self._refresh_loop, daemon=True).start()

    def _disconnect(self):
        self._auto_refresh = False
        self.cli.disconnect()
        self._show_connect()

    def _refresh_loop(self):
        while self._running and self._auto_refresh:
            time.sleep(3)
            if not self._auto_refresh or not self._running:
                break
            try:
                s = self.cli.status()
                self.root.after(0, lambda: self._update_dashboard(s))
            except:
                pass

    # ── Dashboard ──
    def _build_dashboard(self, nb):
        f = tk.Frame(nb, bg=FUNDO)
        nb.add(f, text=" Dashboard ")

        cards = tk.Frame(f, bg=FUNDO)
        cards.pack(fill="both", expand=True, padx=20, pady=20)

        self._dash_widgets = {}
        info = [
            ("status", "Estado"), ("ip", "IP"), ("ssid", "WiFi"),
            ("free_heap", "Heap Livre"), ("wifi_rssi", "WiFi RSSI"),
            ("uptime", "Uptime"), ("cpu_freq", "CPU"), ("state_name", "Modo"),
        ]
        row, col = 0, 0
        for key, label in info:
            card = tk.Frame(cards, bg=CARD, bd=0, relief="flat", padx=15, pady=12)
            card.grid(row=row, column=col, sticky="nsew", padx=5, pady=5)
            cards.grid_columnconfigure(col, weight=1, uniform="dash")
            tk.Label(card, text=label, fg=TEXTO2, bg=CARD,
                     font=("", 8)).pack(anchor="w")
            w = tk.Label(card, text="--", fg=TEXTO, bg=CARD,
                         font=("", 16, "bold"))
            w.pack(anchor="w", pady=(2, 0))
            self._dash_widgets[key] = (w, card)
            col += 1
            if col > 3:
                col = 0
                row += 1

        tk.Button(f, text="🔄 Atualizar", bg=VERMELHO, fg="#fff",
                  relief="flat", cursor="hand2", padx=15, pady=5,
                  command=self._refresh_dashboard).pack(pady=(0, 15))
        self._dash_status = tk.Label(f, text="", fg=TEXTO2, bg=FUNDO, font=("", 9))
        self._dash_status.pack()

    def _update_dashboard(self, s):
        if isinstance(s, dict):
            m = {k: (w, c) for k, (w, c) in self._dash_widgets.items()}
            for key, (w, card) in m.items():
                val = s.get(key, "--")
                if key == "free_heap":
                    val = f"{val} bytes"
                    color = VERDE if (isinstance(val, int) and val > 20000) or (
                        isinstance(val, str) and val != "--") else VERMELHO
                    w.config(text=val)
                elif key == "wifi_rssi":
                    w.config(text=f"{val} dBm")
                elif key == "uptime":
                    w.config(text=f"{val}s")
                elif key == "cpu_freq":
                    w.config(text=f"{val} MHz")
                elif key == "locked":
                    w.config(text="🔒" if val else "🔓")
                elif key == "status":
                    w.config(text="✅ Online" if val == "ok" else "❌ Offline")
                else:
                    w.config(text=str(val))

    def _refresh_dashboard(self):
        try:
            s = self.cli.status()
            self._update_dashboard(s)
            self._dash_status.config(text="Atualizado")
        except Exception as e:
            self._dash_status.config(text=f"Erro: {e}", fg=VERMELHO)
        self.root.after(2000, lambda: self._dash_status.config(text=""))

    # ── GPIO ──
    def _build_gpio(self, nb):
        f = tk.Frame(nb, bg=FUNDO)
        nb.add(f, text=" GPIO ")

        self._gpio_pins = [2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33]
        self._gpio_buttons = {}
        self._gpio_states = {}

        top = tk.Frame(f, bg=FUNDO)
        top.pack(fill="x", padx=20, pady=(15, 5))
        tk.Label(top, text="Clique para ligar/desligar", fg=TEXTO2, bg=FUNDO,
                 font=("", 9)).pack(side="left")
        tk.Button(top, text="🔄", bg=AZUL, fg=TEXTO, relief="flat",
                  cursor="hand2", padx=10, command=self._gpio_refresh).pack(side="right")

        grid = tk.Frame(f, bg=FUNDO)
        grid.pack(fill="both", expand=True, padx=20, pady=5)
        for i, p in enumerate(self._gpio_pins):
            btn = tk.Button(grid, text=f"GPIO {p}\n--", bg=CARD, fg=TEXTO,
                            font=("", 9, "bold"), relief="flat",
                            cursor="hand2", bd=1, padx=8, pady=12)
            btn.grid(row=i // 5, column=i % 5, sticky="nsew", padx=3, pady=3)
            btn.config(command=lambda pin=p: self._gpio_toggle(pin))
            grid.grid_columnconfigure(i % 5, weight=1)
            self._gpio_buttons[p] = btn

        self._gpio_status = tk.Label(f, text="", fg=TEXTO2, bg=FUNDO, font=("", 9))
        self._gpio_status.pack(pady=(0, 10))
        self._gpio_refresh()

    def _gpio_refresh(self):
        try:
            data = self.cli.gpio_list()
            if isinstance(data, list):
                for item in data:
                    p = item.get("pin")
                    state = item.get("state", 0)
                    if p in self._gpio_buttons:
                        self._gpio_states[p] = state
                        bg = VERDE if state else VERMELHO
                        lbl = "LIGADO" if state else "DESLIGADO"
                        self._gpio_buttons[p].config(text=f"GPIO {p}\n{lbl}", bg=bg)
        except Exception as e:
            self._gpio_status.config(text=f"Erro: {e}", fg=VERMELHO)

    def _gpio_toggle(self, pin):
        new = 0 if self._gpio_states.get(pin) else 1
        try:
            r = self.cli.gpio_set(pin, new)
            if isinstance(r, dict) and r.get("status") == "ok":
                self._gpio_states[pin] = new
                bg = VERDE if new else VERMELHO
                lbl = "LIGADO" if new else "DESLIGADO"
                self._gpio_buttons[pin].config(text=f"GPIO {pin}\n{lbl}", bg=bg)
                self._gpio_status.config(text=f"GPIO {pin} → {'LIGADO' if new else 'DESLIGADO'}")
                self.root.after(2000, lambda: self._gpio_status.config(text=""))
        except Exception as e:
            self._gpio_status.config(text=f"Erro: {e}", fg=VERMELHO)

    # ── Editor de Arquivos ──
    def _build_editor(self, nb):
        f = tk.Frame(nb, bg=FUNDO)
        nb.add(f, text=" Editor ")

        paned = tk.PanedWindow(f, orient="horizontal", bg=FUNDO, sashwidth=4)
        paned.pack(fill="both", expand=True, padx=10, pady=10)

        # File list
        left = tk.Frame(paned, bg=CARD)
        paned.add(left, width=220)

        tk.Label(left, text="Arquivos", fg=TEXTO2, bg=CARD,
                 font=("", 9)).pack(anchor="w", padx=10, pady=(10, 5))

        list_frame = tk.Frame(left, bg=CARD)
        list_frame.pack(fill="both", expand=True, padx=10, pady=(0, 10))

        scroll = tk.Scrollbar(list_frame)
        scroll.pack(side="right", fill="y")

        self._file_listbox = tk.Listbox(list_frame, bg=AZUL, fg=TEXTO,
                                         selectbackground=VERMELHO,
                                         relief="flat", borderwidth=0,
                                         font=("", 9),
                                         yscrollcommand=scroll.set)
        self._file_listbox.pack(fill="both", expand=True)
        scroll.config(command=self._file_listbox.yview)
        self._file_listbox.bind("<<ListboxSelect>>", self._file_select)

        btn_frame = tk.Frame(left, bg=CARD)
        btn_frame.pack(fill="x", padx=10, pady=(0, 10))
        tk.Button(btn_frame, text="🔄 Listar", bg=AZUL, fg=TEXTO,
                  relief="flat", cursor="hand2", padx=8,
                  command=self._file_list).pack(side="left", padx=(0, 5))
        tk.Button(btn_frame, text="🗑 Deletar", bg="#5a2a2a", fg=TEXTO,
                  relief="flat", cursor="hand2", padx=8,
                  command=self._file_delete).pack(side="left")

        # Editor
        right = tk.Frame(paned, bg=FUNDO)
        paned.add(right, width=500)

        name_frame = tk.Frame(right, bg=FUNDO)
        name_frame.pack(fill="x", pady=(0, 5))

        tk.Label(name_frame, text="Arquivo:", fg=TEXTO2, bg=FUNDO,
                 font=("", 9)).pack(side="left")
        self._file_name = tk.Entry(name_frame, bg=AZUL, fg=TEXTO,
                                    insertbackground=TEXTO, relief="flat",
                                    font=("", 10), bd=6)
        self._file_name.pack(side="left", fill="x", expand=True, padx=(5, 5))
        self._file_name.insert(0, "texto.txt")

        tk.Button(name_frame, text="📂 Carregar", bg=AZUL, fg=TEXTO,
                  relief="flat", cursor="hand2", padx=8,
                  command=self._file_load).pack(side="left", padx=(0, 3))
        tk.Button(name_frame, text="💾 Salvar", bg=VERMELHO, fg="#fff",
                  relief="flat", cursor="hand2", padx=8,
                  command=self._file_save).pack(side="left")

        self._file_text = scrolledtext.ScrolledText(right, bg=AZUL, fg=TEXTO,
                                                      insertbackground=TEXTO,
                                                      relief="flat", borderwidth=0,
                                                      font=("Courier", 10),
                                                      padx=10, pady=10)
        self._file_text.pack(fill="both", expand=True)

        self._file_status = tk.Label(right, text="", fg=TEXTO2, bg=FUNDO, font=("", 9))
        self._file_status.pack(pady=(0, 5))

    def _file_list(self):
        try:
            data = self.cli.fs_list("/")
            self._file_listbox.delete(0, "end")
            if isinstance(data, list):
                files = [f for f in data if not f.get("dir")]
                for f in files:
                    name = f.get("name", "").lstrip("/")
                    size = f.get("size", 0)
                    self._file_listbox.insert("end", f"{name} ({size} B)")
                self._file_status.config(text=f"{len(files)} arquivos")
        except Exception as e:
            self._file_status.config(text=f"Erro: {e}", fg=VERMELHO)

    def _file_select(self, event):
        sel = self._file_listbox.curselection()
        if sel:
            name = self._file_listbox.get(sel[0]).split(" (")[0]
            self._file_name.delete(0, "end")
            self._file_name.insert(0, name)
            self._file_load()

    def _file_load(self):
        name = self._file_name.get().strip()
        if not name:
            return
        if not name.startswith("/"):
            name = "/" + name
        try:
            data = self.cli.fs_read(name)
            if isinstance(data, dict) and "content" in data:
                self._file_text.delete("1.0", "end")
                self._file_text.insert("1.0", data["content"])
                self._file_status.config(text=f"Carregado: {name} ({data.get('size', 0)} B)")
        except Exception as e:
            self._file_status.config(text=f"Erro: {e}", fg=VERMELHO)

    def _file_save(self):
        name = self._file_name.get().strip()
        if not name:
            return
        if not name.startswith("/"):
            name = "/" + name
        content = self._file_text.get("1.0", "end-1c")
        try:
            r = self.cli.fs_write(name, content)
            if isinstance(r, dict) and r.get("status") == "ok":
                self._file_status.config(text=f"Salvo: {name} ({r.get('size', 0)} B)")
                self._file_list()
        except Exception as e:
            self._file_status.config(text=f"Erro: {e}", fg=VERMELHO)

    def _file_delete(self):
        sel = self._file_listbox.curselection()
        if not sel:
            return
        name = self._file_listbox.get(sel[0]).split(" (")[0]
        if not name.startswith("/"):
            name = "/" + name
        if messagebox.askyesno("Deletar", f"Deletar {name}?"):
            try:
                r = self.cli.fs_delete(name)
                if isinstance(r, dict) and r.get("status") == "ok":
                    self._file_status.config(text=f"Deletado: {name}")
                    self._file_list()
            except Exception as e:
                self._file_status.config(text=f"Erro: {e}", fg=VERMELHO)

    # ── Configurações ──
    def _build_settings(self, nb):
        f = tk.Frame(nb, bg=FUNDO)
        nb.add(f, text=" Config ")

        # WiFi
        g1 = tk.LabelFrame(f, text="WiFi", bg=CARD, fg=TEXTO2,
                           font=("", 9), padx=15, pady=10)
        g1.pack(fill="x", padx=20, pady=(15, 5))

        tk.Label(g1, text="SSID", fg=TEXTO2, bg=CARD).grid(row=0, column=0, sticky="w")
        self._cfg_ssid = tk.Entry(g1, bg=AZUL, fg=TEXTO, insertbackground=TEXTO,
                                   relief="flat", bd=6, font=("", 10))
        self._cfg_ssid.grid(row=0, column=1, sticky="ew", padx=(10, 0), pady=2)
        g1.grid_columnconfigure(1, weight=1)

        tk.Label(g1, text="Senha", fg=TEXTO2, bg=CARD).grid(row=1, column=0, sticky="w")
        self._cfg_pass = tk.Entry(g1, bg=AZUL, fg=TEXTO, insertbackground=TEXTO,
                                   relief="flat", bd=6, font=("", 10), show="*")
        self._cfg_pass.grid(row=1, column=1, sticky="ew", padx=(10, 0), pady=2)

        # PIN
        g2 = tk.LabelFrame(f, text="Segurança", bg=CARD, fg=TEXTO2,
                           font=("", 9), padx=15, pady=10)
        g2.pack(fill="x", padx=20, pady=5)

        tk.Label(g2, text="PIN", fg=TEXTO2, bg=CARD).grid(row=0, column=0, sticky="w")
        self._cfg_pin = tk.Entry(g2, bg=AZUL, fg=TEXTO, insertbackground=TEXTO,
                                  relief="flat", bd=6, font=("", 10), show="*")
        self._cfg_pin.grid(row=0, column=1, sticky="ew", padx=(10, 0), pady=2)
        g2.grid_columnconfigure(1, weight=1)

        tk.Label(g2, text="Nome do dispositivo", fg=TEXTO2, bg=CARD).grid(row=1, column=0, sticky="w")
        self._cfg_name = tk.Entry(g2, bg=AZUL, fg=TEXTO, insertbackground=TEXTO,
                                   relief="flat", bd=6, font=("", 10))
        self._cfg_name.grid(row=1, column=1, sticky="ew", padx=(10, 0), pady=2)

        # Botões
        btn_f = tk.Frame(f, bg=FUNDO)
        btn_f.pack(padx=20, pady=10)

        tk.Button(btn_f, text="💾 Salvar Config", bg=VERDE, fg="#fff",
                  relief="flat", cursor="hand2", padx=15, pady=6,
                  font=("", 10, "bold"), command=self._cfg_save).pack(side="left", padx=5)
        tk.Button(btn_f, text="🔄 Reiniciar ESP32", bg=VERMELHO, fg="#fff",
                  relief="flat", cursor="hand2", padx=15, pady=6,
                  font=("", 10, "bold"), command=self._cfg_restart).pack(side="left", padx=5)
        tk.Button(btn_f, text="📋 Status", bg=AZUL, fg=TEXTO,
                  relief="flat", cursor="hand2", padx=15, pady=6,
                  command=self._cfg_status).pack(side="left", padx=5)

        # Terminal output for config
        self._cfg_out = scrolledtext.ScrolledText(f, bg=AZUL, fg=TEXTO,
                                                    relief="flat", borderwidth=0,
                                                    font=("Courier", 9), height=8,
                                                    padx=8, pady=8)
        self._cfg_out.pack(fill="x", padx=20, pady=(0, 15))

        self._cfg_out.insert("end", "Clique em 'Status' para ver informações do ESP32\n")

    def _cfg_save(self):
        ssid = self._cfg_ssid.get().strip()
        passwd = self._cfg_pass.get().strip()
        pin = self._cfg_pin.get().strip()
        name = self._cfg_name.get().strip()
        if not ssid:
            messagebox.showwarning("Aviso", "Informe o SSID da rede WiFi")
            return
        try:
            r = self.cli.save_config(ssid, passwd, pin, name)
            self._cfg_out.insert("end", f"\n> Config salva: {r}")
            self._cfg_out.see("end")
        except Exception as e:
            self._cfg_out.insert("end", f"\n> Erro: {e}")
            self._cfg_out.see("end")

    def _cfg_restart(self):
        if messagebox.askyesno("Reiniciar", "Reiniciar o ESP32?"):
            try:
                self.cli.restart()
                self._cfg_out.insert("end", "\n> Reiniciando...")
            except Exception as e:
                self._cfg_out.insert("end", f"\n> Erro: {e}")
            self._cfg_out.see("end")

    def _cfg_status(self):
        try:
            s = self.cli.status()
            self._cfg_out.delete("1.0", "end")
            if isinstance(s, dict):
                for k, v in s.items():
                    self._cfg_out.insert("end", f"{k}: {v}\n")
            else:
                self._cfg_out.insert("end", str(s))
        except Exception as e:
            self._cfg_out.insert("end", f"Erro: {e}")

    # ── Loja (TCP only) ──
    def _build_store(self, nb):
        f = tk.Frame(nb, bg=FUNDO)
        nb.add(f, text=" Loja ")

        top = tk.Frame(f, bg=FUNDO)
        top.pack(fill="x", padx=20, pady=(15, 5))
        tk.Label(top, text="Apps disponíveis no repositório GitHub",
                 fg=TEXTO2, bg=FUNDO, font=("", 9)).pack(side="left")
        tk.Button(top, text="🔄 Buscar", bg=VERMELHO, fg="#fff",
                  relief="flat", cursor="hand2", padx=12,
                  command=self._store_fetch).pack(side="right")
        tk.Button(top, text="📦 Instalados", bg=AZUL, fg=TEXTO,
                  relief="flat", cursor="hand2", padx=12,
                  command=self._store_installed).pack(side="right", padx=(0, 5))

        paned = tk.PanedWindow(f, orient="horizontal", bg=FUNDO, sashwidth=4)
        paned.pack(fill="both", expand=True, padx=20, pady=5)

        # Available apps
        left = tk.Frame(paned, bg=CARD)
        paned.add(left, width=300)
        tk.Label(left, text="Disponíveis", fg=TEXTO2, bg=CARD,
                 font=("", 9)).pack(anchor="w", padx=10, pady=(10, 5))

        self._store_listbox = tk.Listbox(left, bg=AZUL, fg=TEXTO,
                                          selectbackground=VERMELHO,
                                          relief="flat", borderwidth=0,
                                          font=("", 9))
        self._store_listbox.pack(fill="both", expand=True, padx=10, pady=(0, 10))

        # Installed
        right = tk.Frame(paned, bg=CARD)
        paned.add(right, width=200)
        tk.Label(right, text="Instalados", fg=TEXTO2, bg=CARD,
                 font=("", 9)).pack(anchor="w", padx=10, pady=(10, 5))
        self._store_installed_box = tk.Listbox(right, bg=AZUL, fg=TEXTO,
                                                selectbackground=VERMELHO,
                                                relief="flat", borderwidth=0,
                                                font=("", 9))
        self._store_installed_box.pack(fill="both", expand=True, padx=10, pady=(0, 10))

        btn_f = tk.Frame(f, bg=FUNDO)
        btn_f.pack(pady=(0, 10))
        tk.Button(btn_f, text="📥 Instalar selecionado", bg=VERDE, fg="#fff",
                  relief="flat", cursor="hand2", padx=12, pady=4,
                  command=self._store_install).pack(side="left", padx=5)

        self._store_status = tk.Label(f, text="", fg=TEXTO2, bg=FUNDO, font=("", 9))
        self._store_status.pack()

        self._store_apps = []

    def _store_fetch(self):
        self._store_status.config(text="Buscando...")
        self.root.update()
        try:
            import urllib.request
            url = "https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/apps/manifest.json"
            r = urllib.request.urlopen(url, timeout=10)
            data = json.loads(r.read())
            self._store_apps = data.get("apps", [])
            self._store_listbox.delete(0, "end")
            for a in self._store_apps:
                self._store_listbox.insert("end", f"{a.get('icon','📦')} {a['name']} — {a.get('desc','')}")
            self._store_status.config(text=f"{len(self._store_apps)} apps encontrados")
        except Exception as e:
            self._store_status.config(text=f"Erro: {e}", fg=VERMELHO)

    def _store_installed(self):
        try:
            data = self.cli.apps()
            self._store_installed_box.delete(0, "end")
            if isinstance(data, dict):
                apps = data.get("apps", [])
                for a in apps:
                    self._store_installed_box.insert("end", f"{a['id']} ({a.get('size',0)} B)")
                self._store_status.config(text=f"{len(apps)} apps instalados")
        except Exception as e:
            self._store_status.config(text=f"Erro: {e}", fg=VERMELHO)

    def _store_install(self):
        sel = self._store_listbox.curselection()
        if not sel or not self._store_apps:
            return
        app = self._store_apps[sel[0]]
        app_id = app["id"]
        app_name = app["name"]
        app_file = app.get("file", f"{app_id}.html")
        url = f"https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/apps/{app_file}"
        self._store_status.config(text=f"Instalando {app_name}...")
        self.root.update()
        try:
            r = self.cli.install_app(url)
            self._store_status.config(text=f"✔ {app_name} instalado!")
            self._store_installed()
        except Exception as e:
            self._store_status.config(text=f"Erro: {e}", fg=VERMELHO)

    # ── Terminal ──
    def _build_terminal(self, nb):
        f = tk.Frame(nb, bg=FUNDO)
        nb.add(f, text=" Terminal ")

        self._term_text = scrolledtext.ScrolledText(f, bg=AZUL, fg=TEXTO,
                                                      relief="flat", borderwidth=0,
                                                      font=("Courier", 10),
                                                      padx=10, pady=10)
        self._term_text.pack(fill="both", expand=True, padx=10, pady=(10, 5))

        bottom = tk.Frame(f, bg=FUNDO)
        bottom.pack(fill="x", padx=10, pady=(0, 10))

        self._term_input = tk.Entry(bottom, bg=CARD, fg=TEXTO,
                                     insertbackground=TEXTO, relief="flat",
                                     font=("Courier", 10), bd=8)
        self._term_input.pack(side="left", fill="x", expand=True)
        self._term_input.bind("<Return>", self._term_send)

        tk.Button(bottom, text="Enviar", bg=VERMELHO, fg="#fff",
                  relief="flat", cursor="hand2", padx=10,
                  command=self._term_send).pack(side="left", padx=(5, 0))
        tk.Button(bottom, text="Limpar", bg=AZUL, fg=TEXTO,
                  relief="flat", cursor="hand2", padx=10,
                  command=lambda: self._term_text.delete("1.0", "end")).pack(side="left", padx=5)

    def _term_send(self, event=None):
        cmd = self._term_input.get().strip()
        if not cmd:
            return
        self._term_input.delete(0, "end")
        self._term_text.insert("end", f"> {cmd}\n")
        self._term_text.see("end")
        try:
            if self.cli.mode == "serial":
                resp = self.cli._serial_cmd(cmd)
            elif self.cli.mode == "tcp":
                if cmd.startswith("GET "):
                    resp = self.cli.api_get(cmd[4:])
                elif cmd.startswith("POST "):
                    rest = cmd[5:]
                    space = rest.index(" ")
                    path = rest[:space]
                    body = rest[space+1:]
                    import json as j
                    resp = self.cli.api_post(path, j.loads(body))
                else:
                    resp = "Comandos: GET /path, POST /path {json}"
            else:
                resp = "Não conectado"
            self._term_text.insert("end", f"{resp}\n")
        except Exception as e:
            self._term_text.insert("end", f"Erro: {e}\n")
        self._term_text.see("end")


# ── Main ──────────────────────────────────────────────────────────

def main():
    ESPortable32GUI()

if __name__ == "__main__":
    main()
