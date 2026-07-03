#!/usr/bin/env python3
"""ESPortable32 Desktop — Windows 98 style ESP32 interface."""

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import threading
import time
import json
import os
import sys

try:
    import requests
except ImportError:
    requests = None

# ── Win98 palette ──
DESKTOP  = "#008080"
TITLE_BG = "#000080"
TITLE_FG = "#ffffff"
TITLE_INACTIVE = "#808080"
WIN_BG   = "#c0c0c0"
BTN_FACE = "#c0c0c0"
BTN_HIGH = "#ffffff"
BTN_SHAD = "#808080"
DARK_SHAD = "#404040"
STATUSBAR = "#c0c0c0"

ICON_APPS = [
    ("Painel",  "\u2302"),   # Dashboard
    ("GPIO",    "\u26A1"),   # GPIO
    ("Editor",  "\u270E"),   # Editor
    ("Config",  "\u2699"),   # Config
    ("Loja",    "\u2728"),   # Store
    ("Terminal","\u2328"),   # Terminal
]

PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))

# ── Cliente ESP32 ────────────────────────────────────────────────

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
            s.dtr = False; time.sleep(0.1)
            s.dtr = True;  time.sleep(2)
            s.reset_input_buffer()
            s.write(b"STATUS\n"); time.sleep(0.5)
            out = b""
            while s.in_waiting: out += s.read(s.in_waiting)
            if b"State:" in out or b"ESPortable" in out:
                self._serial = s; self.mode = "serial"
                return True, out.decode("utf-8", errors="replace")
            s.close(); return False, "ESP32 nao respondeu"
        except Exception as e:
            return False, str(e)

    def disconnect(self):
        if self._serial:
            try: self._serial.close()
            except: pass
        self._serial = None; self._base = None; self.mode = None

    def _tcp(self, method, path, **kwargs):
        if not self._base: raise Exception("Nao conectado")
        fn = getattr(requests, method.lower())
        with self._lock:
            r = fn(self._base + path, timeout=10, **kwargs)
        if r.status_code >= 400:
            raise Exception(f"HTTP {r.status_code}: {r.text[:100]}")
        if r.text and r.headers.get("content-type","").startswith("application/json"):
            return r.json()
        return r.text

    def _serial_cmd(self, cmd, timeout=2):
        if not self._serial: raise Exception("Nao conectado")
        with self._lock:
            self._serial.reset_input_buffer()
            self._serial.write((cmd+"\n").encode()); time.sleep(0.3)
            out = b""; end = time.time()+timeout
            while time.time() < end:
                if self._serial.in_waiting: out += self._serial.read(self._serial.in_waiting)
                time.sleep(0.05)
        return out.decode("utf-8", errors="replace")

    def api_get(self, path):
        if self.mode == "tcp": return self._tcp("GET", path)
        return self._serial_cmd(f"API {path}")

    def api_post(self, path, data=None):
        if self.mode == "tcp": return self._tcp("POST", path, json=data)
        return self._serial_cmd(f"API {path} {json.dumps(data) if data else ''}")

    def status(self):     return self.api_get("/api/status")
    def gpio_list(self):  return self.api_get("/api/gpio")
    def gpio_set(self, p, s):    return self.api_post("/api/gpio", {"pin":p,"state":s})
    def gpio_set_mode(self,p,m): return self.api_post("/api/gpio", {"pin":p,"mode":m})
    def fs_list(self, p="/"):    return self.api_get(f"/api/fs/list?path={p}")
    def fs_read(self, p):  return self.api_get(f"/api/fs/read?path={p}")
    def fs_write(self, p, c): return self.api_post("/api/fs/write", {"path":p,"content":c})
    def fs_delete(self, p):
        if self.mode == "tcp":
            r = requests.delete(self._base+f"/api/fs/delete?path={p}", timeout=10)
            return r.json()
        return self._serial_cmd(f"API /api/fs/delete?path={p}")
    def apps(self):        return self.api_get("/api/apps")
    def install_app(self, url, fn=None):
        b={"url":url}
        if fn: b["path"]=fn
        return self.api_post("/api/apps", b)
    def unlock(self, pin):
        if self.mode=="tcp":
            import urllib.parse
            r=requests.post(self._base+"/api/unlock", data=f"pin={urllib.parse.quote(pin)}",
                          headers={"Content-Type":"application/x-www-form-urlencoded"}, timeout=10)
            return r.json()
        return self._serial_cmd(f"PIN={pin}\nSAVE")
    def restart(self):    return self.api_post("/api/restart")
    def proxy(self, url): return self.api_post("/api/proxy", {"url":url})
    def wifi_scan(self):
        try: return self.api_get("/api/wifi/scan")
        except: return []
    def save_config(self, ssid, passwd, pin="", name=""):
        if self.mode=="tcp":
            import urllib.parse
            b=f"ssid={urllib.parse.quote(ssid)}&pass={urllib.parse.quote(passwd)}"
            if pin: b+=f"&pin={urllib.parse.quote(pin)}"
            r=requests.post(self._base+"/api/setup", data=b,
                          headers={"Content-Type":"application/x-www-form-urlencoded"}, timeout=10)
            return r.text
        cmds=f"WIFI={ssid},{passwd}"
        if pin: cmds+=f"\nPIN={pin}"
        if name: cmds+=f"\nNAME={name}"
        cmds+="\nSAVE"
        return self._serial_cmd(cmds)


# ── Win98 Widgets ────────────────────────────────────────────────

class Win98TitleBar(tk.Frame):
    """Title bar with icon, title, min, close."""
    def __init__(self, parent, title, icon, on_close, on_minimize, **kw):
        super().__init__(parent, bg=TITLE_BG, **kw)
        self.parent = parent
        self._drag = {"x":0, "y":0}
        self._on_close = on_close
        self._on_minimize = on_minimize

        self.pack(fill="x")
        self._bar = tk.Frame(self, bg=TITLE_BG, height=22)
        self._bar.pack(fill="x")
        self._bar.pack_propagate(False)

        lbl = tk.Label(self._bar, text=f"  {icon or '\u25A0'}  {title}",
                      bg=TITLE_BG, fg=TITLE_FG, font=("System",8,"bold"),
                      anchor="w")
        lbl.pack(side="left", fill="x", expand=True)

        btn_frame = tk.Frame(self._bar, bg=TITLE_BG)
        btn_frame.pack(side="right", padx=2)

        for txt, cmd in [("\u2013", on_minimize), ("\u2716", on_close)]:
            b = tk.Label(btn_frame, text=txt, bg=TITLE_BG, fg=TITLE_FG,
                        font=("System",8,"bold"), padx=4, cursor="hand2")
            b.pack(side="left", padx=1)
            b.bind("<Button-1>", lambda e, c=cmd: c() if c else None)

        self._bar.bind("<Button-1>", self._drag_start)
        self._bar.bind("<B1-Motion>", self._drag_move)
        lbl.bind("<Button-1>", self._drag_start)
        lbl.bind("<B1-Motion>", self._drag_move)

    def _drag_start(self, e):
        self._drag["x"] = e.x_root - self.parent.winfo_x()
        self._drag["y"] = e.y_root - self.parent.winfo_y()

    def _drag_move(self, e):
        x = e.x_root - self._drag["x"]
        y = e.y_root - self._drag["y"]
        self.parent.place(x=x, y=y)


class Win98Window:
    """A Win98-style window that lives on the desktop."""
    win_count = 0

    def __init__(self, desktop, title, icon, content_fn, w=480, h=360):
        self.desktop = desktop
        self.title = title
        self.icon = icon
        self.content_fn = content_fn
        self.minimized = False
        self.frame = None
        self._w = w; self._h = h
        self._open = False
        self.content = None

    def open(self, x=None, y=None):
        if self._open:
            self._raise()
            return
        self._open = True
        self._auto_place(x, y)
        # content
        self.content = self.content_fn(self.frame)
        self.content.pack(fill="both", expand=True, padx=2, pady=(0,2))
        self.desktop._on_window_open(self)

    def _auto_place(self, x, y):
        Win98Window.win_count += 1
        off = ((Win98Window.win_count % 5) * 30) + 20
        if x is None: x = off
        if y is None: y = off
        f = tk.Frame(self.desktop.canvas, bg=WIN_BG, bd=0,
                    highlightbackground=DARK_SHAD, highlightthickness=1)
        f.place(x=x, y=y, width=self._w, height=self._h)
        self.frame = f
        self._update_zorder()
        # Title bar
        Win98TitleBar(f, self.title, self.icon,
                     on_close=self.close, on_minimize=self._minimize)
        # Resize grip
        grip = tk.Frame(f, bg=WIN_BG, cursor="bottom_right_corner", width=16, height=16)
        grip.pack(side="bottom", anchor="se")
        grip.bind("<Button-1>", self._resize_start)
        grip.bind("<B1-Motion>", self._resize_move)

    def _update_zorder(self):
        self.frame.tkraise()

    def _raise(self):
        if self.frame:
            self.frame.tkraise()
            self.desktop._on_window_raise(self)

    def _minimize(self):
        if self.frame:
            self.frame.place_forget()
            self.minimized = True
            self.desktop._on_window_minimize(self)

    def restore(self):
        if self.minimized and self.frame:
            self._auto_place(30, 30)
            self.minimized = False
            self.desktop._on_window_restore(self)
            self._raise()

    def close(self):
        if self.frame:
            self.frame.destroy()
            self.frame = None
        self._open = False
        self.minimized = False
        self.desktop._on_window_close(self)

    def _resize_start(self, e):
        self._rs = {"x": e.x_root, "y": e.y_root,
                    "w": self._w, "h": self._h}

    def _resize_move(self, e):
        dw = e.x_root - self._rs["x"]
        dh = e.y_root - self._rs["y"]
        self._w = max(200, self._rs["w"] + dw)
        self._h = max(150, self._rs["h"] + dh)
        if self.frame:
            self.frame.configure(width=self._w, height=self._h)


class DesktopIcon(tk.Frame):
    """Icon on the desktop."""
    def __init__(self, parent, label, icon_char, command):
        super().__init__(parent, bg=DESKTOP, cursor="hand2")
        self.command = command
        self.configure(bd=0, padx=4, pady=4)
        self.lbl = tk.Label(self, text=icon_char, font=("System",24),
                           bg=DESKTOP, fg=TITLE_FG)
        self.lbl.pack()
        self.txt = tk.Label(self, text=label, font=("System",8),
                           bg=DESKTOP, fg=TITLE_FG,
                           wraplength=80, justify="center")
        self.txt.pack()
        self.bind("<Button-1>", self._click)
        self.lbl.bind("<Button-1>", self._click)
        self.txt.bind("<Button-1>", self._click)
        self.bind("<Double-Button-1>", self._dbl)
        self.lbl.bind("<Double-Button-1>", self._dbl)
        self.txt.bind("<Double-Button-1>", self._dbl)

    def _click(self, e):
        self.configure(bg="#000080")
        self.lbl.configure(bg="#000080")
        self.txt.configure(bg="#000080")
        self.after(100, self._unsel)

    def _unsel(self):
        self.configure(bg=DESKTOP)
        self.lbl.configure(bg=DESKTOP)
        self.txt.configure(bg=DESKTOP)

    def _dbl(self, e):
        self.command()


class Taskbar(tk.Frame):
    """Bottom taskbar with Start, buttons, tray."""
    def __init__(self, parent, on_start):
        super().__init__(parent, bg=BTN_FACE,
                        highlightbackground=BTN_SHAD,
                        highlightthickness=1, height=30)
        self.parent = parent
        self._on_start = on_start
        self._buttons = {}
        self.pack(side="bottom", fill="x")
        self.pack_propagate(False)

        # Start button
        self.start_btn = tk.Label(self, text="  \u25A0  Iniciar  ",
                                 bg=BTN_FACE, fg="black",
                                 font=("System",8,"bold"),
                                 relief="raised", cursor="hand2",
                                 padx=4, pady=2)
        self.start_btn.pack(side="left", padx=2, pady=2)
        self.start_btn.bind("<Button-1>", lambda e: self._on_start())

        # Container for task buttons
        self.btn_container = tk.Frame(self, bg=BTN_FACE)
        self.btn_container.pack(side="left", fill="x", expand=True, padx=4)

        # System tray
        self.tray = tk.Frame(self, bg=BTN_FACE,
                            highlightbackground=BTN_SHAD,
                            highlightthickness=1, padx=6)
        self.tray.pack(side="right", padx=2, pady=2)

        self.clock = tk.Label(self.tray, text="", bg=BTN_FACE,
                             font=("System",7))
        self.clock.pack()
        self._update_clock()

    def _update_clock(self):
        self.clock.config(text=time.strftime("%H:%M"))
        self.after(10000, self._update_clock)

    def add_button(self, wid):
        """Add a taskbar button for an open window."""
        name = wid.title
        if name in self._buttons:
            return
        btn = tk.Label(self.btn_container, text=f"  {wid.icon} {name}  ",
                      bg=BTN_FACE, relief="raised", cursor="hand2",
                      font=("System",7), padx=4)
        btn.pack(side="left", padx=1, pady=2)
        btn.bind("<Button-1>", lambda e: self._toggle(wid))
        self._buttons[name] = btn

    def _toggle(self, wid):
        if wid.minimized:
            wid.restore()
        else:
            wid._minimize()

    def remove_button(self, wid):
        name = wid.title
        if name in self._buttons:
            self._buttons[name].destroy()
            del self._buttons[name]

    def highlight(self, wid):
        name = wid.title
        if name in self._buttons:
            for n, b in self._buttons.items():
                b.configure(relief="raised" if n != name else "sunken")

    def restore_all(self):
        for n, b in self._buttons.items():
            b.configure(relief="raised")


# ── App Content Frames ───────────────────────────────────────────

class DashboardContent(tk.Frame):
    def __init__(self, parent, cli):
        super().__init__(parent, bg=WIN_BG)
        self.cli = cli
        self._widgets = {}
        info = [("status","Estado"),("ip","IP"),("ssid","WiFi"),
                ("free_heap","Heap"),("wifi_rssi","RSSI"),
                ("uptime","Uptime"),("cpu_freq","CPU"),("state_name","Modo")]
        grid = tk.Frame(self, bg=WIN_BG)
        grid.pack(fill="both", expand=True, padx=8, pady=8)
        for i,(k,lbl) in enumerate(info):
            f = tk.LabelFrame(grid, text=lbl, bg=WIN_BG, fg="black",
                            font=("System",7), relief="groove",
                            padx=6, pady=4)
            f.grid(row=i//4, column=i%4, sticky="nsew", padx=3, pady=3)
            grid.grid_columnconfigure(i%4, weight=1)
            w = tk.Label(f, text="--", bg=WIN_BG, fg="black",
                        font=("System",14,"bold"))
            w.pack()
            self._widgets[k] = w
        tk.Button(self, text="Atualizar", bg=BTN_FACE, relief="raised",
                 command=self._refresh).pack(pady=4)

    def _refresh(self):
        try:
            d = self.cli.status()
            if not isinstance(d,dict): return
            for k,w in self._widgets.items():
                v = d.get(k,"--")
                if k=="free_heap": v=f"{v} bytes"
                elif k=="wifi_rssi": v=f"{v} dBm"
                elif k=="uptime": v=f"{v}s"
                elif k=="cpu_freq": v=f"{v} MHz"
                w.config(text=str(v))
        except: pass

    def auto_refresh(self):
        self._refresh()
        if self.winfo_exists():
            self.after(3000, self.auto_refresh)


class GPIOContent(tk.Frame):
    PINS = [2,4,5,12,13,14,15,16,17,18,19,21,22,23,25,26,27,32,33]
    def __init__(self, parent, cli):
        super().__init__(parent, bg=WIN_BG)
        self.cli = cli
        self._states = {}
        self._btns = {}
        tk.Label(self, text="GPIO", font=("System",9,"bold"),
                bg=WIN_BG).pack()
        g = tk.Frame(self, bg=WIN_BG)
        g.pack(fill="both", expand=True, padx=4, pady=4)
        for i,p in enumerate(self.PINS):
            btn = tk.Button(g, text=f"GPIO {p}\n--", width=8, height=2,
                          bg=BTN_FACE, relief="raised", cursor="hand2")
            btn.grid(row=i//5, column=i%5, sticky="nsew", padx=2, pady=2)
            g.grid_columnconfigure(i%5, weight=1)
            btn.config(command=lambda pin=p: self._toggle(pin))
            self._btns[p] = btn
        self._msg = tk.Label(self, text="", bg=WIN_BG, fg="black",
                            font=("System",7))
        self._msg.pack()
        self._refresh()

    def _refresh(self):
        try:
            d = self.cli.gpio_list()
            if isinstance(d,list):
                for item in d:
                    p = item.get("pin")
                    s = item.get("state",0)
                    if p in self._btns:
                        self._states[p] = s
                        self._btns[p].config(
                            text=f"GPIO {p}\n{'ON' if s else 'OFF'}",
                            bg=BTN_FACE)
        except: pass

    def _toggle(self, pin):
        new = 0 if self._states.get(pin) else 1
        try:
            r = self.cli.gpio_set(pin, new)
            if isinstance(r,dict) and r.get("status")=="ok":
                self._states[pin] = new
                self._btns[pin].config(
                    text=f"GPIO {pin}\n{'ON' if new else 'OFF'}")
                self._msg.config(text=f"GPIO {pin} -> {'LIGADO' if new else 'DESLIGADO'}")
        except Exception as e:
            self._msg.config(text=f"Erro: {e}")


class EditorContent(tk.Frame):
    def __init__(self, parent, cli):
        super().__init__(parent, bg=WIN_BG)
        self.cli = cli
        tk.Label(self, text="Editor de Arquivos", font=("System",9,"bold"),
                bg=WIN_BG).pack()
        paned = tk.PanedWindow(self, orient="horizontal", bg=WIN_BG,
                              sashwidth=3)
        paned.pack(fill="both", expand=True, padx=4, pady=4)

        # File list
        left = tk.Frame(paned, bg=WIN_BG)
        paned.add(left, width=180)
        self._listbox = tk.Listbox(left, bg="white", fg="black",
                                   font=("System",8), relief="sunken")
        self._listbox.pack(fill="both", expand=True)
        self._listbox.bind("<<ListboxSelect>>", self._select)
        btn = tk.Frame(left, bg=WIN_BG)
        btn.pack(fill="x", pady=2)
        tk.Button(btn, text="Listar", bg=BTN_FACE, relief="raised",
                 command=self._list).pack(side="left", padx=2)
        tk.Button(btn, text="Deletar", bg=BTN_FACE, relief="raised",
                 command=self._delete).pack(side="left")

        # Editor
        right = tk.Frame(paned, bg=WIN_BG)
        paned.add(right, width=350)
        top = tk.Frame(right, bg=WIN_BG)
        top.pack(fill="x")
        self._path = tk.Entry(top, bg="white", fg="black", font=("System",8),
                             relief="sunken")
        self._path.pack(side="left", fill="x", expand=True, padx=(0,4))
        self._path.insert(0,"arquivo.txt")
        tk.Button(top, text="Carregar", bg=BTN_FACE, relief="raised",
                 command=self._load).pack(side="left", padx=1)
        tk.Button(top, text="Salvar", bg=BTN_FACE, relief="raised",
                 command=self._save).pack(side="left")
        self._text = scrolledtext.ScrolledText(right, bg="white", fg="black",
                                               font=("Courier",9),
                                               relief="sunset", bd=2)
        self._text.pack(fill="both", expand=True)
        self._msg = tk.Label(right, text="", bg=WIN_BG, fg="black",
                            font=("System",7))
        self._msg.pack()

    def _list(self):
        try:
            d = self.cli.fs_list("/")
            self._listbox.delete(0,"end")
            if isinstance(d,list):
                for f in d:
                    if not f.get("dir"):
                        n = f.get("name","").lstrip("/")
                        s = f.get("size",0)
                        self._listbox.insert("end", f"{n} ({s} B)")
                self._msg.config(text=f"{self._listbox.size()} arquivos")
        except Exception as e:
            self._msg.config(text=f"Erro: {e}")

    def _select(self, e):
        sel = self._listbox.curselection()
        if sel:
            n = self._listbox.get(sel[0]).split(" (")[0]
            self._path.delete(0,"end"); self._path.insert(0,n)
            self._load()

    def _load(self):
        n = self._path.get().strip()
        if not n: return
        if not n.startswith("/"): n="/"+n
        try:
            d = self.cli.fs_read(n)
            if isinstance(d,dict) and "content" in d:
                self._text.delete("1.0","end")
                self._text.insert("1.0", d["content"])
                self._msg.config(text=f"Carregado: {n}")
        except Exception as e:
            self._msg.config(text=f"Erro: {e}")

    def _save(self):
        n = self._path.get().strip()
        if not n: return
        if not n.startswith("/"): n="/"+n
        c = self._text.get("1.0","end-1c")
        try:
            r = self.cli.fs_write(n, c)
            if isinstance(r,dict) and r.get("status")=="ok":
                self._msg.config(text=f"Salvo: {n}")
                self._list()
        except Exception as e:
            self._msg.config(text=f"Erro: {e}")

    def _delete(self):
        sel = self._listbox.curselection()
        if not sel: return
        n = self._listbox.get(sel[0]).split(" (")[0]
        if not n.startswith("/"): n="/"+n
        if messagebox.askyesno("Deletar", f"Deletar {n}?"):
            try:
                r = self.cli.fs_delete(n)
                if isinstance(r,dict) and r.get("status")=="ok":
                    self._msg.config(text=f"Deletado: {n}")
                    self._list()
            except Exception as e:
                self._msg.config(text=f"Erro: {e}")


class ConfigContent(tk.Frame):
    def __init__(self, parent, cli):
        super().__init__(parent, bg=WIN_BG)
        self.cli = cli
        tk.Label(self, text="Configuracoes", font=("System",9,"bold"),
                bg=WIN_BG).pack()
        f1 = tk.LabelFrame(self, text="WiFi", bg=WIN_BG, fg="black",
                          font=("System",7), relief="groove", padx=6, pady=4)
        f1.pack(fill="x", padx=6, pady=4)
        tk.Label(f1, text="SSID", bg=WIN_BG).grid(row=0,column=0,sticky="w")
        self._ssid = tk.Entry(f1, bg="white", fg="black", font=("System",8),
                             relief="sunken")
        self._ssid.grid(row=0,column=1,sticky="ew",padx=4)
        f1.grid_columnconfigure(1,weight=1)
        tk.Label(f1, text="Senha", bg=WIN_BG).grid(row=1,column=0,sticky="w")
        self._pass = tk.Entry(f1, bg="white", fg="black", font=("System",8),
                             relief="sunken", show="*")
        self._pass.grid(row=1,column=1,sticky="ew",padx=4)

        f2 = tk.LabelFrame(self, text="Seguranca", bg=WIN_BG, fg="black",
                          font=("System",7), relief="groove", padx=6, pady=4)
        f2.pack(fill="x", padx=6, pady=4)
        tk.Label(f2, text="PIN", bg=WIN_BG).grid(row=0,column=0,sticky="w")
        self._pin = tk.Entry(f2, bg="white", fg="black", font=("System",8),
                            relief="sunken", show="*")
        self._pin.grid(row=0,column=1,sticky="ew",padx=4)
        f2.grid_columnconfigure(1,weight=1)
        tk.Label(f2, text="Nome", bg=WIN_BG).grid(row=1,column=0,sticky="w")
        self._name = tk.Entry(f2, bg="white", fg="black", font=("System",8),
                             relief="sunken")
        self._name.grid(row=1,column=1,sticky="ew",padx=4)

        btn = tk.Frame(self, bg=WIN_BG)
        btn.pack(pady=4)
        tk.Button(btn, text="Salvar", bg=BTN_FACE, relief="raised",
                 command=self._save).pack(side="left",padx=2)
        tk.Button(btn, text="Reiniciar", bg=BTN_FACE, relief="raised",
                 command=self._restart).pack(side="left",padx=2)
        tk.Button(btn, text="Status", bg=BTN_FACE, relief="raised",
                 command=self._status).pack(side="left",padx=2)

        self._out = scrolledtext.ScrolledText(self, bg="white", fg="black",
                                              font=("Courier",8),
                                              relief="sunken", height=6)
        self._out.pack(fill="x", padx=6, pady=4)

    def _log(self, m):
        self._out.insert("end", m+"\n"); self._out.see("end")

    def _save(self):
        s=self._ssid.get().strip()
        p=self._pass.get().strip()
        pin=self._pin.get().strip()
        n=self._name.get().strip()
        if not s: self._log("SSID obrigatorio"); return
        try:
            r=self.cli.save_config(s,p,pin,n)
            self._log(f"Salvo: {r}")
        except Exception as e: self._log(f"Erro: {e}")

    def _restart(self):
        if messagebox.askyesno("Reiniciar","Reiniciar ESP32?"):
            try: self.cli.restart(); self._log("Reiniciando...")
            except Exception as e: self._log(f"Erro: {e}")

    def _status(self):
        try:
            s=self.cli.status()
            self._out.delete("1.0","end")
            if isinstance(s,dict):
                for k,v in s.items(): self._out.insert("end",f"{k}: {v}\n")
            else: self._out.insert("end",str(s))
        except Exception as e: self._log(f"Erro: {e}")


class StoreContent(tk.Frame):
    def __init__(self, parent, cli):
        super().__init__(parent, bg=WIN_BG)
        self.cli = cli; self._apps = []
        tk.Label(self,text="Loja de Apps",font=("System",9,"bold"),
                bg=WIN_BG).pack()
        btn = tk.Frame(self, bg=WIN_BG)
        btn.pack(fill="x", padx=4, pady=2)
        tk.Button(btn,text="Buscar",bg=BTN_FACE,relief="raised",
                 command=self._fetch).pack(side="left",padx=2)
        tk.Button(btn,text="Instalados",bg=BTN_FACE,relief="raised",
                 command=self._installed).pack(side="left",padx=2)
        tk.Button(btn,text="Instalar",bg=BTN_FACE,relief="raised",
                 command=self._install).pack(side="right",padx=2)
        paned = tk.PanedWindow(self,orient="horizontal",bg=WIN_BG,sashwidth=3)
        paned.pack(fill="both",expand=True,padx=4,pady=4)
        left = tk.Frame(paned,bg=WIN_BG)
        paned.add(left,width=250)
        tk.Label(left,text="Disponiveis",bg=WIN_BG,font=("System",7)).pack()
        self._list = tk.Listbox(left,bg="white",fg="black",font=("System",8),
                               relief="sunken")
        self._list.pack(fill="both",expand=True)
        right = tk.Frame(paned,bg=WIN_BG)
        paned.add(right,width=180)
        tk.Label(right,text="Instalados",bg=WIN_BG,font=("System",7)).pack()
        self._inst = tk.Listbox(right,bg="white",fg="black",font=("System",8),
                               relief="sunken")
        self._inst.pack(fill="both",expand=True)
        self._msg = tk.Label(self,text="",bg=WIN_BG,font=("System",7))
        self._msg.pack()

    def _fetch(self):
        self._msg.config(text="Buscando...")
        self.update()
        try:
            import urllib.request
            u="https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/apps/manifest.json"
            r=urllib.request.urlopen(u,timeout=10)
            d=json.loads(r.read())
            self._apps=d.get("apps",[])
            self._list.delete(0,"end")
            for a in self._apps:
                self._list.insert("end",f"{a.get('icon','')} {a['name']} - {a.get('desc','')}")
            self._msg.config(text=f"{len(self._apps)} apps")
        except Exception as e: self._msg.config(text=f"Erro: {e}")

    def _installed(self):
        try:
            d=self.cli.apps()
            self._inst.delete(0,"end")
            if isinstance(d,dict):
                for a in d.get("apps",[]):
                    self._inst.insert("end",f"{a['id']} ({a.get('size',0)} B)")
                self._msg.config(text=f"{len(d['apps'])} instalados")
        except Exception as e: self._msg.config(text=f"Erro: {e}")

    def _install(self):
        sel=self._list.curselection()
        if not sel or not self._apps: return
        a=self._apps[sel[0]]
        u=f"https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/apps/{a.get('file',a['id']+'.html')}"
        self._msg.config(text=f"Instalando {a['name']}...")
        self.update()
        try: self.cli.install_app(u); self._msg.config(text=f"OK {a['name']} instalado!"); self._installed()
        except Exception as e: self._msg.config(text=f"Erro: {e}")


class TerminalContent(tk.Frame):
    def __init__(self, parent, cli):
        super().__init__(parent, bg=WIN_BG)
        self.cli = cli
        self._log = scrolledtext.ScrolledText(self, bg="black", fg="#0f0",
                                             font=("Courier",9),
                                             relief="sunken")
        self._log.pack(fill="both",expand=True,padx=4,pady=4)

        bottom = tk.Frame(self, bg=WIN_BG)
        bottom.pack(fill="x", padx=4, pady=(0,4))
        self._input = tk.Entry(bottom, bg="white", fg="black",
                              font=("Courier",9), relief="sunken")
        self._input.pack(side="left", fill="x", expand=True)
        self._input.bind("<Return>", self._send)
        tk.Button(bottom, text="Enviar", bg=BTN_FACE, relief="raised",
                 command=self._send).pack(side="left", padx=(4,0))
        tk.Button(bottom, text="Limpar", bg=BTN_FACE, relief="raised",
                 command=lambda: self._log.delete("1.0","end")).pack(side="left", padx=2)

    def _send(self, e=None):
        cmd = self._input.get().strip()
        if not cmd: return
        self._input.delete(0,"end")
        self._log.insert("end", f"> {cmd}\n")
        self._log.see("end")
        try:
            if self.cli.mode=="serial":
                resp=self.cli._serial_cmd(cmd)
            elif self.cli.mode=="tcp":
                if cmd.startswith("GET "):
                    resp=self.cli.api_get(cmd[4:])
                elif cmd.startswith("POST "):
                    rest=cmd[5:]; sp=rest.index(" ")
                    resp=self.cli.api_post(rest[:sp], json.loads(rest[sp+1:]))
                else: resp="GET /path ou POST /path {...}"
            else: resp="Nao conectado"
            self._log.insert("end", f"{resp}\n")
        except Exception as ex:
            self._log.insert("end", f"Erro: {ex}\n")
        self._log.see("end")


# ── Desktop ──────────────────────────────────────────────────────

class ESPortableDesktop:
    def __init__(self):
        self.cli = ESPClient()
        self._wins = []

        self.root = tk.Tk()
        self.root.title("ESPortable32")
        self.root.geometry("960x640")
        self.root.minsize(640, 480)
        self.root.configure(bg=DESKTOP)

        self.canvas = tk.Frame(self.root, bg=DESKTOP)
        self.canvas.pack(fill="both", expand=True)

        # Desktop icons
        self._icons = []
        for i, (name, icon) in enumerate(ICON_APPS):
            dic = DesktopIcon(self.canvas, name, icon,
                             command=lambda n=name: self._open_app(n))
            dic.place(x=20 + (i%3)*100, y=20 + (i//3)*100)
            self._icons.append(dic)

        # Taskbar
        self.taskbar = Taskbar(self.root, self._show_start)

        # Create window objects
        self._window_defs = {
            "Painel":   {"icon":"\u2302", "w":500,"h":280,
                        "fn":lambda p: DashboardContent(p,self.cli)},
            "GPIO":     {"icon":"\u26A1", "w":520,"h":340,
                        "fn":lambda p: GPIOContent(p,self.cli)},
            "Editor":   {"icon":"\u270E", "w":600,"h":380,
                        "fn":lambda p: EditorContent(p,self.cli)},
            "Config":   {"icon":"\u2699", "w":480,"h":360,
                        "fn":lambda p: ConfigContent(p,self.cli)},
            "Loja":     {"icon":"\u2728", "w":540,"h":360,
                        "fn":lambda p: StoreContent(p,self.cli)},
            "Terminal": {"icon":"\u2328", "w":560,"h":360,
                        "fn":lambda p: TerminalContent(p,self.cli)},
        }
        self._windows = {}

        # Start menu
        self._start_menu = None

        # Auto-scan
        self._scan()

        self.root.protocol("WM_DELETE_WINDOW", self._quit)
        self.root.mainloop()

    def _quit(self):
        self.cli.disconnect()
        self.root.destroy()

    def _scan(self):
        def scan():
            ports = []
            try:
                import serial.tools.list_ports
                for p in serial.tools.list_ports.comports():
                    ports.append(p.device)
            except: pass
            if not ports:
                if sys.platform=="win32":
                    ports=[f"COM{i}" for i in range(1,10)]
                elif sys.platform=="darwin":
                    ports=["/dev/cu.usbmodem101","/dev/cu.usbserial-110"]
                else:
                    ports=["/dev/ttyACM0","/dev/ttyACM1","/dev/ttyUSB0","/dev/ttyUSB1"]
            for port in ports:
                ok,_=self.cli.connect_serial(port,115200)
                if ok:
                    self.cli.mode="serial"
                    return
        threading.Thread(target=scan, daemon=True).start()

    def _open_app(self, name):
        if name not in self._window_defs:
            return
        d = self._window_defs[name]
        if name not in self._windows or not self._windows[name]._open:
            w = Win98Window(self, name, d["icon"], d["fn"], w=d["w"], h=d["h"])
            self._windows[name] = w
            w.open()
        else:
            self._windows[name]._raise()

    def _show_start(self):
        if self._start_menu and self._start_menu.winfo_exists():
            self._start_menu.destroy()
            self._start_menu = None
            return
        menu = tk.Frame(self.root, bg=BTN_FACE, bd=0,
                       highlightbackground=DARK_SHAD, highlightthickness=2)
        menu.place(x=2, y=self.root.winfo_height()-130, width=200)
        items = [
            ("\u2302 Painel", lambda: self._open_app("Painel")),
            ("\u26A1 GPIO", lambda: self._open_app("GPIO")),
            ("\u270E Editor", lambda: self._open_app("Editor")),
            ("\u2699 Config", lambda: self._open_app("Config")),
            ("\u2728 Loja", lambda: self._open_app("Loja")),
            ("\u2328 Terminal", lambda: self._open_app("Terminal")),
            ("---", None),
            ("\u2B95 Desconectar", self._disconnect),
            ("\u274C Sair", self._quit),
        ]
        for txt, cmd in items:
            if txt.startswith("---"):
                tk.Frame(menu, bg=BTN_SHAD, height=1).pack(fill="x", padx=4, pady=4)
                continue
            b = tk.Label(menu, text=f"  {txt}", bg=BTN_FACE, fg="black",
                        font=("System",8), anchor="w", cursor="hand2",
                        padx=8, pady=2)
            b.pack(fill="x")
            if cmd:
                b.bind("<Button-1>", lambda e, c=cmd: [menu.destroy(), c()])
                b.bind("<Enter>", lambda e, b=b: b.configure(bg="#000080",fg="white"))
                b.bind("<Leave>", lambda e, b=b: b.configure(bg=BTN_FACE,fg="black"))
        self._start_menu = menu
        # Close menu when clicking elsewhere
        self.root.bind("<Button-1>", self._close_start, add="+")

    def _close_start(self, e):
        if self._start_menu and self._start_menu.winfo_exists():
            if e.widget != self.taskbar.start_btn:
                try:
                    self._start_menu.destroy()
                    self._start_menu = None
                except: pass

    def _disconnect(self):
        self.cli.disconnect()
        # Close all windows
        for n,w in self._windows.items():
            if w._open: w.close()

    def _on_window_open(self, wid):
        self.taskbar.add_button(wid)

    def _on_window_close(self, wid):
        self.taskbar.remove_button(wid)

    def _on_window_raise(self, wid):
        self.taskbar.highlight(wid)

    def _on_window_minimize(self, wid):
        pass

    def _on_window_restore(self, wid):
        self.taskbar.add_button(wid)


def main():
    ESPortableDesktop()

if __name__ == "__main__":
    main()
