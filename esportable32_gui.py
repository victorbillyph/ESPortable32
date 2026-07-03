#!/usr/bin/env python3
"""ESPortable32 Desktop — Windows 98 style boot, setup wizard & desktop GUI."""

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import threading
import time
import json
import os
import sys
import queue

try: import requests
except: requests = None

# ── Palette ──
DESKTOP  = "#008080"
TITLE_BG = "#000080"
TITLE_FG = "#ffffff"
WIN_BG   = "#c0c0c0"
BTN_FACE = "#c0c0c0"
BTN_HIGH = "#ffffff"
BTN_SHAD = "#808080"
DARK_SHAD = "#404040"
BLACK    = "#000000"

PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))

ICON_APPS = [
    ("Painel",  "\u2302"),
    ("GPIO",    "\u26A1"),
    ("Editor",  "\u270E"),
    ("Config",  "\u2699"),
    ("Loja",    "\u2728"),
    ("Terminal","\u2328"),
]

# ── Cliente ESP32 ────────────────────────────────────────────────

class ESPClient:
    def __init__(self):
        self.mode = None; self._serial = None; self._base = None; self._lock = threading.Lock()

    def connect_tcp(self, host, port=80):
        if not requests: return False,"requests nao instalado"
        self._base=f"http://{host}:{port}"
        try:
            r=requests.get(self._base+"/api/status",timeout=5)
            if r.status_code==200: self.mode="tcp"; return True,r.json()
            return False,f"HTTP {r.status_code}"
        except Exception as e: return False,str(e)

    def connect_serial(self, port, baud=115200):
        try: import serial
        except: return False,"pyserial nao instalado"
        try:
            s=serial.Serial(port,baud,timeout=2); s.dtr=False; time.sleep(0.1)
            s.dtr=True; time.sleep(2); s.reset_input_buffer()
            s.write(b"STATUS\n"); time.sleep(0.5); out=b""
            while s.in_waiting: out+=s.read(s.in_waiting)
            if b"State:" in out or b"ESPortable" in out:
                self._serial=s; self.mode="serial"
                return True,out.decode("utf-8",errors="replace")
            s.close(); return False,"ESP32 nao respondeu"
        except Exception as e: return False,str(e)

    def disconnect(self):
        if self._serial:
            try: self._serial.close()
            except: pass
        self._serial=None; self._base=None; self.mode=None

    def _tcp(self, method, path, **kw):
        if not self._base: raise Exception("Nao conectado")
        fn=getattr(requests,method.lower())
        with self._lock: r=fn(self._base+path,timeout=10,**kw)
        if r.status_code>=400: raise Exception(f"HTTP {r.status_code}: {r.text[:100]}")
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
        if self.mode=="tcp": return self._tcp("GET",path)
        return self._serial_cmd(f"API {path}")
    def api_post(self,path,data=None):
        if self.mode=="tcp": return self._tcp("POST",path,json=data)
        return self._serial_cmd(f"API {path} {json.dumps(data) if data else ''}")
    def status(self): return self.api_get("/api/status")
    def gpio_list(self): return self.api_get("/api/gpio")
    def gpio_set(self,p,s): return self.api_post("/api/gpio",{"pin":p,"state":s})
    def fs_list(self,p="/"): return self.api_get(f"/api/fs/list?path={p}")
    def fs_read(self,p): return self.api_get(f"/api/fs/read?path={p}")
    def fs_write(self,p,c): return self.api_post("/api/fs/write",{"path":p,"content":c})
    def fs_delete(self,p):
        if self.mode=="tcp":
            r=requests.delete(self._base+f"/api/fs/delete?path={p}",timeout=10); return r.json()
        return self._serial_cmd(f"API /api/fs/delete?path={p}")
    def apps(self): return self.api_get("/api/apps")
    def install_app(self,url,fn=None):
        b={"url":url}
        if fn: b["path"]=fn
        return self.api_post("/api/apps",b)
    def restart(self): return self.api_post("/api/restart")


# ── BIOS POST Screen ─────────────────────────────────────────────

class BIOSScreen(tk.Frame):
    """BIOS-style POST screen: black bg, green text, scanning messages."""
    def __init__(self, parent, cli, on_found, on_skip):
        super().__init__(parent, bg=BLACK)
        self.parent=parent; self.cli=cli; self.on_found=on_found; self.on_skip=on_skip
        self._running=True
        self.pack(fill="both",expand=True)

        self.text=tk.Text(self,bg=BLACK,fg="#0f0",font=("Courier",10),bd=0,relief="flat",
                         state="disabled",cursor="xterm",insertbackground="#0f0")
        self.text.pack(fill="both",expand=True,padx=20,pady=10)

        self.skip_btn=tk.Button(self,text="Pular (inserir manualmente)",bg=BTN_FACE,
                               relief="raised",cursor="hand2",command=on_skip)
        self.skip_btn.place(relx=1.0,rely=1.0,anchor="se",x=-10,y=-10)

        # progress line at bottom
        self.prog=tk.Label(self,text="",bg=BLACK,fg="#0a0",font=("Courier",9),anchor="w")
        self.prog.pack(side="bottom",fill="x",padx=20,pady=(0,5))

        self._msg_queue=queue.Queue()
        self._poll_queue()

    def _poll_queue(self):
        while not self._msg_queue.empty():
            msg=self._msg_queue.get_nowait()
            self.text.configure(state="normal")
            self.text.insert("end",msg+"\n"); self.text.see("end")
            self.text.configure(state="disabled")
        if self._running: self.after(50,self._poll_queue)

    def write(self,msg):
        self._msg_queue.put(msg)

    def set_progress(self,msg):
        self.after(0,lambda: self.prog.configure(text=msg))

    def start(self):
        def run():
            self.after(0,lambda: self.skip_btn.place_forget())
            self.write("ESPortable32 BIOS v1.0.0")
            time.sleep(0.4)
            self.write("Copyright (c) 2024")
            time.sleep(0.3)
            self.write("")
            self.write("CPU: Tensilica Xtensa LX6 @ 240MHz")
            time.sleep(0.2)
            self.write("DRAM: 520KB SRAM")
            time.sleep(0.1)
            self.write("Flash: 4MB SPI flash")
            time.sleep(0.2)
            self.write("")
            self.write("POST sequence...")
            time.sleep(0.3)
            self.write("  CPU check... OK")
            time.sleep(0.1)
            self.write("  Memory test... 520KB OK")
            time.sleep(0.15)
            self.write("  Flash ID... 0xEF4016 OK")
            time.sleep(0.1)
            self.write("")
            self.write("Boot device: Serial / USB")
            time.sleep(0.3)
            self.write("")

            # Scan serial ports
            ports=[]
            try:
                import serial.tools.list_ports
                for p in serial.tools.list_ports.comports(): ports.append(p.device)
            except: pass
            if not ports:
                if sys.platform=="win32": ports=[f"COM{i}" for i in range(1,10)]
                elif sys.platform=="darwin": ports=["/dev/cu.usbmodem101","/dev/cu.usbserial-110"]
                else: ports=["/dev/ttyACM0","/dev/ttyACM1","/dev/ttyUSB0","/dev/ttyUSB1"]

            self.write("Scanning serial ports...")
            for port in ports:
                self.set_progress(f"Testing {port}...")
                self.write(f"  {port}... ",end=False)
                time.sleep(0.15)
                ok,_=self.cli.connect_serial(port,115200)
                if ok:
                    self.cli.mode="serial"
                    self.write("DETECTED")
                    time.sleep(0.3)
                    self.write("")
                    self.write(f"MAC: {port}")
                    self.write("Firmware: ESPortable32")
                    self.write("")
                    self.write("Boot device ready.")
                    self.set_progress("")
                    time.sleep(0.5)
                    self.after(0,lambda: self.on_found())
                    return
                self.write("not found")
            self.write("")
            self.write("ERROR: No ESP32 found")
            self.set_progress("")
            self.after(0,lambda: self.skip_btn.place(relx=1.0,rely=1.0,anchor="se",x=-10,y=-10))

        threading.Thread(target=run,daemon=True).start()

    def write(self,msg,end=True):
        self._msg_queue.put(("line",msg,end))

    def set_progress(self,msg):
        self.after(0,lambda: self.prog.configure(text=msg))

    def _poll_queue(self):
        while not self._msg_queue.empty():
            item=self._msg_queue.get_nowait()
            if item[0]=="line":
                _,msg,end=item
                self.text.configure(state="normal")
                if end: self.text.insert("end",msg+"\n")
                else: self.text.insert("end",msg)
                self.text.see("end")
                self.text.configure(state="disabled")
        if self._running: self.after(50,self._poll_queue)

    def stop(self):
        self._running=False
        self.destroy()


# ── Boot Screen ──────────────────────────────────────────────────

class BootScreen(tk.Frame):
    """Windows 98-style boot screen with logo and progress bar."""
    def __init__(self,parent,status,on_done):
        super().__init__(parent,bg=DESKTOP)
        self.status=status; self.on_done=on_done
        self.pack(fill="both",expand=True)

        # Logo
        self.logo=tk.Label(self,text="ESPortable32",font=("System",28,"bold"),
                          fg="white",bg=DESKTOP)
        self.logo.pack(expand=True)

        self.sub=tk.Label(self,text="",font=("System",9),fg="#fff",bg=DESKTOP)
        self.sub.pack()

        # Progress
        self.prog_frame=tk.Frame(self,bg="white",highlightbackground="white",
                                highlightthickness=2,width=300,height=20)
        self.prog_frame.pack(pady=(15,5))
        self.prog_frame.pack_propagate(False)
        self.prog_bar=tk.Frame(self.prog_frame,bg="#000080",width=0,height=20)
        self.prog_bar.pack(side="left",fill="y")

        self.status_text=tk.Label(self,text="",font=("System",8),fg="white",bg=DESKTOP)
        self.status_text.pack(pady=(0,20))

        self.after(100,self._animate)

    def _animate(self,i=0):
        steps=[("Preparando hardware...",30),("Carregando modulos...",55),
               ("Inicializando sistema...",75),("Abrindo interface...",90),
               ("Pronto!",100)]
        for txt,pct in steps:
            if i<=pct:
                self.status_text.configure(text=txt)
                break
        pct=i
        self.prog_bar.configure(width=int(300*pct/100))
        self.prog_frame.update_idletasks()
        if i<100:
            delay=15 if i<90 else 20
            self.after(delay,self._animate,i+1)
        else:
            self.after(300,self.on_done)


# ── Setup Wizard ─────────────────────────────────────────────────

class SetupWizard(tk.Toplevel):
    """Windows 98-style configuration wizard for ESP32 in setup mode."""
    def __init__(self,parent,cli,on_complete):
        super().__init__(parent)
        self.cli=cli; self.on_complete=on_complete
        self._page=0

        self.title("Assistente de Configuracao")
        self.geometry("520x400")
        self.resizable(False,False)
        self.configure(bg=WIN_BG)
        self.transient(parent)
        self.grab_set()

        # Banner
        self.banner=tk.Frame(self,bg=TITLE_BG,height=40)
        self.banner.pack(fill="x")
        self.banner.pack_propagate(False)
        tk.Label(self.banner,text="  Assistente de Configuracao ESPortable32",
                bg=TITLE_BG,fg=TITLE_FG,font=("System",10,"bold"),anchor="w").pack(side="left",padx=8)

        # Page area
        self.page_frame=tk.Frame(self,bg="white",bd=2,relief="sunken")
        self.page_frame.pack(fill="both",expand=True,padx=8,pady=8)

        # Buttons
        self.btn_frame=tk.Frame(self,bg=WIN_BG)
        self.btn_frame.pack(fill="x",padx=8,pady=(0,8))
        self.back_btn=tk.Button(self.btn_frame,text="< Voltar",bg=BTN_FACE,
                               relief="raised",state="disabled",command=self._prev)
        self.back_btn.pack(side="left",padx=2)
        self.next_btn=tk.Button(self.btn_frame,text="Avançar >",bg=BTN_FACE,
                               relief="raised",command=self._next)
        self.next_btn.pack(side="right",padx=2)
        self.cancel_btn=tk.Button(self.btn_frame,text="Cancelar",bg=BTN_FACE,
                                 relief="raised",command=self._cancel)
        self.cancel_btn.pack(side="right",padx=2)

        # Pages
        self._pages=[
            self._page_welcome,
            self._page_wifi,
            self._page_security,
            self._page_summary,
        ]
        self._widgets={}
        self._render()
        self.protocol("WM_DELETE_WINDOW",self._cancel)

    def _clear(self):
        for w in self.page_frame.winfo_children(): w.destroy()

    def _render(self):
        self._clear()
        self._pages[self._page]()
        self.back_btn.configure(state="disabled" if self._page==0 else "normal")
        is_last=self._page==len(self._pages)-1
        self.next_btn.configure(text="Concluir" if is_last else "Avançar >")

    def _page_welcome(self):
        f=self.page_frame
        tk.Label(f,text="Bem-vindo ao Assistente de Configuracao",font=("System",9,"bold"),
                bg="white",fg="black").pack(pady=(30,15))
        tk.Label(f,text="Este assistente vai configurar seu ESP32\npara conexao WiFi e seguranca.",
                bg="white",fg="black",font=("System",8),justify="left").pack(pady=5)
        tk.Label(f,text="O dispositivo esta em MODO SETUP.\n\n"
                 "Configure a rede WiFi e um PIN opcional\npara proteger o acesso.",
                bg="white",fg="black",font=("System",8),justify="left").pack(pady=10)

    def _page_wifi(self):
        f=self.page_frame
        tk.Label(f,text="Conexao WiFi",font=("System",9,"bold"),
                bg="white",fg="black").pack(pady=(20,10),anchor="w",padx=20)
        tk.Label(f,text="SSID da rede:",bg="white",fg="black",font=("System",8),
                anchor="w").pack(fill="x",padx=20)
        self._widgets["ssid"]=tk.Entry(f,bg="white",fg="black",font=("System",8),
                                       relief="sunken",bd=2)
        self._widgets["ssid"].pack(fill="x",padx=20,pady=(2,10))
        self._widgets["ssid"].focus_set()
        tk.Label(f,text="Senha (deixe em branco se for rede aberta):",bg="white",
                fg="black",font=("System",8),anchor="w").pack(fill="x",padx=20)
        self._widgets["pass"]=tk.Entry(f,bg="white",fg="black",font=("System",8),
                                       relief="sunken",bd=2,show="*")
        self._widgets["pass"].pack(fill="x",padx=20,pady=(2,10))

    def _page_security(self):
        f=self.page_frame
        tk.Label(f,text="Seguranca",font=("System",9,"bold"),
                bg="white",fg="black").pack(pady=(20,10),anchor="w",padx=20)
        tk.Label(f,text="PIN de bloqueio (opcional):",bg="white",fg="black",
                font=("System",8),anchor="w").pack(fill="x",padx=20)
        self._widgets["pin"]=tk.Entry(f,bg="white",fg="black",font=("System",8),
                                       relief="sunken",bd=2,show="*")
        self._widgets["pin"].pack(fill="x",padx=20,pady=(2,10))
        tk.Label(f,text="Nome do dispositivo:",bg="white",fg="black",
                font=("System",8),anchor="w").pack(fill="x",padx=20)
        self._widgets["name"]=tk.Entry(f,bg="white",fg="black",font=("System",8),
                                       relief="sunken",bd=2)
        self._widgets["name"].insert(0,"ESPortable32")
        self._widgets["name"].pack(fill="x",padx=20,pady=(2,10))

    def _page_summary(self):
        f=self.page_frame
        s=self._widgets["ssid"].get().strip()
        p=self._widgets["pass"].get().strip()
        pin=self._widgets["pin"].get().strip()
        n=self._widgets["name"].get().strip()
        tk.Label(f,text="Resumo da Configuracao",font=("System",9,"bold"),
                bg="white",fg="black").pack(pady=(20,15),anchor="w",padx=20)
        items=[
            ("SSID:", s or "(nao informado)"),
            ("Senha:", "********" if p else "(nenhuma)"),
            ("PIN:",    "********" if pin else "(nenhum)"),
            ("Dispositivo:", n or "ESPortable32"),
        ]
        for lbl,val in items:
            r=tk.Frame(f,bg="white")
            r.pack(fill="x",padx=20,pady=2)
            tk.Label(r,text=lbl,bg="white",fg="black",font=("System",8,"bold"),
                    width=14,anchor="w").pack(side="left")
            tk.Label(r,text=val,bg="white",fg="black",font=("System",8),
                    anchor="w").pack(side="left",fill="x",expand=True)
        tk.Label(f,text="\nAs configuracoes serao enviadas para o ESP32.\n"
                 "O dispositivo sera reiniciado apos a configuracao.",
                bg="white",fg="black",font=("System",8),justify="left").pack(pady=(20,5),padx=20)

    def _next(self):
        if self._page<len(self._pages)-1:
            # Validate
            if self._page==0:
                if not self._widgets["ssid"].get().strip():
                    messagebox.showwarning("Aviso","Informe o SSID da rede WiFi",parent=self)
                    return
            self._page+=1
            self._render()
        else:
            self._finish()

    def _prev(self):
        if self._page>0:
            self._page-=1
            self._render()

    def _cancel(self):
        if messagebox.askyesno("Cancelar","Deseja cancelar a configuracao?",parent=self):
            self.destroy()

    def _finish(self):
        ssid=self._widgets["ssid"].get().strip()
        passwd=self._widgets["pass"].get().strip()
        pin=self._widgets["pin"].get().strip()
        name=self._widgets["name"].get().strip()
        if not ssid:
            messagebox.showwarning("Aviso","Informe o SSID",parent=self); return

        # Disable buttons
        self.next_btn.configure(state="disabled"); self.back_btn.configure(state="disabled")
        self.cancel_btn.configure(state="disabled")
        self._clear()
        tk.Label(self.page_frame,text="Enviando configuracao...",font=("System",10,"bold"),
                bg="white",fg="black").pack(pady=(60,10))
        tk.Label(self.page_frame,text="Aguardando ESP32...",bg="white",fg="black",
                font=("System",8)).pack()
        self.update()

        # Send config
        try:
            cmd=f"WIFI={ssid},{passwd}\n"
            if pin: cmd+=f"PIN={pin}\n"
            if name: cmd+=f"NAME={name}\n"
            cmd+="SAVE\n"
            resp=self.cli._serial_cmd(cmd,timeout=5)
            self._clear()
            tk.Label(self.page_frame,text="Configuracao salva com sucesso!",
                    font=("System",10,"bold"),bg="white",fg="green").pack(pady=(40,10))
            tk.Label(self.page_frame,text=f"Resposta: {resp[:100]}",
                    bg="white",fg="black",font=("System",8)).pack()
            tk.Label(self.page_frame,text="\nO ESP32 sera reiniciado.\n"
                     "A interface vai tentar reconectar automaticamente.",
                    bg="white",fg="black",font=("System",8),justify="left").pack(pady=15)
            tk.Button(self.page_frame,text="OK",bg=BTN_FACE,relief="raised",
                     command=lambda: [self.destroy(), self.on_complete()]).pack(pady=10)
        except Exception as e:
            tk.Label(self.page_frame,text=f"Erro: {e}",bg="white",fg="red",
                    font=("System",8)).pack(pady=10)


# ── Win98 Window (Desktop) ───────────────────────────────────────

class Win98TitleBar(tk.Frame):
    def __init__(self,parent,title,icon,on_close,on_minimize,**kw):
        super().__init__(parent,bg=TITLE_BG,**kw)
        self.parent=parent; self._on_close=on_close; self._on_minimize=on_minimize
        self._drag={"x":0,"y":0}
        self.pack(fill="x")
        self._bar=tk.Frame(self,bg=TITLE_BG,height=22)
        self._bar.pack(fill="x"); self._bar.pack_propagate(False)
        lbl=tk.Label(self._bar,text=f"  {icon or chr(9632)}  {title}",
                     bg=TITLE_BG,fg=TITLE_FG,font=("System",8,"bold"),anchor="w")
        lbl.pack(side="left",fill="x",expand=True)
        bf=tk.Frame(self._bar,bg=TITLE_BG); bf.pack(side="right",padx=2)
        for txt,cmd in [("\u2013",on_minimize),("\u2716",on_close)]:
            b=tk.Label(bf,text=txt,bg=TITLE_BG,fg=TITLE_FG,font=("System",8,"bold"),
                      padx=4,cursor="hand2")
            b.pack(side="left",padx=1); b.bind("<Button-1>",lambda e,c=cmd: c() if c else None)
        self._bar.bind("<Button-1>",self._drag_start)
        self._bar.bind("<B1-Motion>",self._drag_move)
        lbl.bind("<Button-1>",self._drag_start); lbl.bind("<B1-Motion>",self._drag_move)

    def _drag_start(self,e):
        self._drag["x"]=e.x_root-self.parent.winfo_x()
        self._drag["y"]=e.y_root-self.parent.winfo_y()

    def _drag_move(self,e):
        self.parent.place(x=e.x_root-self._drag["x"],y=e.y_root-self._drag["y"])


class Win98Window:
    win_count=0
    def __init__(self,desktop,title,icon,content_fn,w=480,h=360):
        self.desktop=desktop; self.title=title; self.icon=icon
        self.content_fn=content_fn; self.minimized=False; self.frame=None
        self._w=w; self._h=h; self._open=False; self.content=None

    def open(self,x=None,y=None):
        if self._open: self._raise(); return
        self._open=True
        Win98Window.win_count+=1; off=((Win98Window.win_count%5)*30)+20
        if x is None: x=off
        if y is None: y=off
        f=tk.Frame(self.desktop.canvas,bg=WIN_BG,bd=0,
                  highlightbackground=DARK_SHAD,highlightthickness=1)
        f.place(x=x,y=y,width=self._w,height=self._h); self.frame=f
        f.tkraise()
        Win98TitleBar(f,self.title,self.icon,on_close=self.close,on_minimize=self._minimize)
        self.content=self.content_fn(f)
        self.content.pack(fill="both",expand=True,padx=2,pady=(0,2))
        grip=tk.Frame(f,bg=WIN_BG,cursor="bottom_right_corner",width=16,height=16)
        grip.pack(side="bottom",anchor="se")
        grip.bind("<Button-1>",self._resize_start)
        grip.bind("<B1-Motion>",self._resize_move)
        self.desktop._on_window_open(self)

    def _raise(self):
        if self.frame: self.frame.tkraise(); self.desktop._on_window_raise(self)

    def _minimize(self):
        if self.frame: self.frame.place_forget(); self.minimized=True; self.desktop._on_window_minimize(self)

    def restore(self):
        if self.minimized and self.frame:
            self._open=False; self.open(30,30); self.minimized=False; self.desktop._on_window_restore(self)

    def close(self):
        if self.frame: self.frame.destroy(); self.frame=None
        self._open=False; self.minimized=False; self.desktop._on_window_close(self)

    def _resize_start(self,e):
        self._rs={"x":e.x_root,"y":e.y_root,"w":self._w,"h":self._h}

    def _resize_move(self,e):
        dw=e.x_root-self._rs["x"]; dh=e.y_root-self._rs["y"]
        self._w=max(200,self._rs["w"]+dw); self._h=max(150,self._rs["h"]+dh)
        if self.frame: self.frame.configure(width=self._w,height=self._h)


class DesktopIcon(tk.Frame):
    def __init__(self,parent,label,icon_char,command):
        super().__init__(parent,bg=DESKTOP,cursor="hand2",bd=0,padx=4,pady=4)
        self.command=command
        self.lbl=tk.Label(self,text=icon_char,font=("System",24),bg=DESKTOP,fg=TITLE_FG)
        self.lbl.pack()
        self.txt=tk.Label(self,text=label,font=("System",8),bg=DESKTOP,fg=TITLE_FG,wraplength=80,justify="center")
        self.txt.pack()
        for w in (self,self.lbl,self.txt):
            w.bind("<Button-1>",self._click); w.bind("<Double-Button-1>",self._dbl)

    def _click(self,e):
        self.configure(bg="#000080"); self.lbl.configure(bg="#000080"); self.txt.configure(bg="#000080")
        self.after(100,lambda: (self.configure(bg=DESKTOP),self.lbl.configure(bg=DESKTOP),self.txt.configure(bg=DESKTOP)))

    def _dbl(self,e): self.command()


class Taskbar(tk.Frame):
    def __init__(self,parent,on_start):
        super().__init__(parent,bg=BTN_FACE,highlightbackground=BTN_SHAD,highlightthickness=1,height=30)
        self.parent=parent; self._on_start=on_start; self._buttons={}
        self.pack(side="bottom",fill="x"); self.pack_propagate(False)
        self.start_btn=tk.Label(self,text="  \u25A0  Iniciar  ",bg=BTN_FACE,fg="black",
                               font=("System",8,"bold"),relief="raised",cursor="hand2",padx=4,pady=2)
        self.start_btn.pack(side="left",padx=2,pady=2)
        self.start_btn.bind("<Button-1>",lambda e: self._on_start())
        self.btn_container=tk.Frame(self,bg=BTN_FACE)
        self.btn_container.pack(side="left",fill="x",expand=True,padx=4)
        self.tray=tk.Frame(self,bg=BTN_FACE,highlightbackground=BTN_SHAD,highlightthickness=1,padx=6)
        self.tray.pack(side="right",padx=2,pady=2)
        self.clock=tk.Label(self.tray,text="",bg=BTN_FACE,font=("System",7))
        self.clock.pack(); self._update_clock()

    def _update_clock(self):
        self.clock.config(text=time.strftime("%H:%M")); self.after(10000,self._update_clock)

    def add_button(self,wid):
        n=wid.title
        if n in self._buttons: return
        btn=tk.Label(self.btn_container,text=f"  {wid.icon} {n}  ",bg=BTN_FACE,relief="raised",
                    cursor="hand2",font=("System",7),padx=4)
        btn.pack(side="left",padx=1,pady=2)
        btn.bind("<Button-1>",lambda e: self._toggle(wid))
        self._buttons[n]=btn

    def _toggle(self,wid):
        if wid.minimized: wid.restore()
        else: wid._minimize()

    def remove_button(self,wid):
        n=wid.title
        if n in self._buttons: self._buttons[n].destroy(); del self._buttons[n]

    def highlight(self,wid):
        n=wid.title
        for name,b in self._buttons.items():
            b.configure(relief="sunken" if name==n else "raised")


# ── App Content Frames ───────────────────────────────────────────

class DashboardContent(tk.Frame):
    def __init__(self,parent,cli):
        super().__init__(parent,bg=WIN_BG)
        self.cli=cli; self._widgets={}
        info=[("status","Estado"),("ip","IP"),("ssid","WiFi"),("free_heap","Heap"),
              ("wifi_rssi","RSSI"),("uptime","Uptime"),("cpu_freq","CPU"),("state_name","Modo")]
        g=tk.Frame(self,bg=WIN_BG); g.pack(fill="both",expand=True,padx=8,pady=8)
        for i,(k,lbl) in enumerate(info):
            f=tk.LabelFrame(g,text=lbl,bg=WIN_BG,fg="black",font=("System",7),relief="groove",padx=6,pady=4)
            f.grid(row=i//4,column=i%4,sticky="nsew",padx=3,pady=3); g.grid_columnconfigure(i%4,weight=1)
            w=tk.Label(f,text="--",bg=WIN_BG,fg="black",font=("System",14,"bold"))
            w.pack(); self._widgets[k]=w
        tk.Button(self,text="Atualizar",bg=BTN_FACE,relief="raised",command=self._refresh).pack(pady=4)

    def _refresh(self):
        try:
            d=self.cli.status()
            if not isinstance(d,dict): return
            for k,w in self._widgets.items():
                v=d.get(k,"--")
                if k=="free_heap": v=f"{v} bytes"
                elif k=="wifi_rssi": v=f"{v} dBm"
                elif k=="uptime": v=f"{v}s"
                elif k=="cpu_freq": v=f"{v} MHz"
                w.config(text=str(v))
        except: pass


class GPIOContent(tk.Frame):
    PINS=[2,4,5,12,13,14,15,16,17,18,19,21,22,23,25,26,27,32,33]
    def __init__(self,parent,cli):
        super().__init__(parent,bg=WIN_BG)
        self.cli=cli; self._states={}; self._btns={}
        tk.Label(self,text="GPIO",font=("System",9,"bold"),bg=WIN_BG).pack()
        g=tk.Frame(self,bg=WIN_BG); g.pack(fill="both",expand=True,padx=4,pady=4)
        for i,p in enumerate(self.PINS):
            btn=tk.Button(g,text=f"GPIO {p}\n--",width=8,height=2,bg=BTN_FACE,relief="raised",cursor="hand2")
            btn.grid(row=i//5,column=i%5,sticky="nsew",padx=2,pady=2); g.grid_columnconfigure(i%5,weight=1)
            btn.config(command=lambda pin=p: self._toggle(pin)); self._btns[p]=btn
        self._msg=tk.Label(self,text="",bg=WIN_BG,fg="black",font=("System",7))
        self._msg.pack(); self._refresh()

    def _refresh(self):
        try:
            d=self.cli.gpio_list()
            if isinstance(d,list):
                for item in d:
                    p=item.get("pin"); s=item.get("state",0)
                    if p in self._btns: self._states[p]=s; self._btns[p].config(text=f"GPIO {p}\n{'ON' if s else 'OFF'}")
        except: pass

    def _toggle(self,pin):
        new=0 if self._states.get(pin) else 1
        try:
            r=self.cli.gpio_set(pin,new)
            if isinstance(r,dict) and r.get("status")=="ok":
                self._states[pin]=new; self._btns[pin].config(text=f"GPIO {pin}\n{'ON' if new else 'OFF'}")
                self._msg.config(text=f"GPIO {pin} -> {'LIGADO' if new else 'DESLIGADO'}")
        except Exception as e: self._msg.config(text=f"Erro: {e}")


class EditorContent(tk.Frame):
    def __init__(self,parent,cli):
        super().__init__(parent,bg=WIN_BG)
        self.cli=cli
        tk.Label(self,text="Editor de Arquivos",font=("System",9,"bold"),bg=WIN_BG).pack()
        p=tk.PanedWindow(self,orient="horizontal",bg=WIN_BG,sashwidth=3)
        p.pack(fill="both",expand=True,padx=4,pady=4)
        left=tk.Frame(p,bg=WIN_BG); p.add(left,width=180)
        self._listbox=tk.Listbox(left,bg="white",fg="black",font=("System",8),relief="sunken")
        self._listbox.pack(fill="both",expand=True)
        self._listbox.bind("<<ListboxSelect>>",self._select)
        bf=tk.Frame(left,bg=WIN_BG); bf.pack(fill="x",pady=2)
        tk.Button(bf,text="Listar",bg=BTN_FACE,relief="raised",command=self._list).pack(side="left",padx=2)
        tk.Button(bf,text="Deletar",bg=BTN_FACE,relief="raised",command=self._delete).pack(side="left")
        right=tk.Frame(p,bg=WIN_BG); p.add(right,width=350)
        top=tk.Frame(right,bg=WIN_BG); top.pack(fill="x")
        self._path=tk.Entry(top,bg="white",fg="black",font=("System",8),relief="sunken")
        self._path.pack(side="left",fill="x",expand=True,padx=(0,4)); self._path.insert(0,"arquivo.txt")
        tk.Button(top,text="Carregar",bg=BTN_FACE,relief="raised",command=self._load).pack(side="left",padx=1)
        tk.Button(top,text="Salvar",bg=BTN_FACE,relief="raised",command=self._save).pack(side="left")
        self._text=scrolledtext.ScrolledText(right,bg="white",fg="black",font=("Courier",9),relief="sunset",bd=2)
        self._text.pack(fill="both",expand=True)
        self._msg=tk.Label(right,text="",bg=WIN_BG,fg="black",font=("System",7))
        self._msg.pack()

    def _list(self):
        try:
            d=self.cli.fs_list("/"); self._listbox.delete(0,"end")
            if isinstance(d,list):
                for f in d:
                    if not f.get("dir"): self._listbox.insert("end",f"{f.get('name','').lstrip('/')} ({f.get('size',0)} B)")
                self._msg.config(text=f"{self._listbox.size()} arquivos")
        except Exception as e: self._msg.config(text=f"Erro: {e}")

    def _select(self,e):
        sel=self._listbox.curselection()
        if sel:
            self._path.delete(0,"end"); self._path.insert(0,self._listbox.get(sel[0]).split(" (")[0]); self._load()

    def _load(self):
        n=self._path.get().strip()
        if not n: return
        if not n.startswith("/"): n="/"+n
        try:
            d=self.cli.fs_read(n)
            if isinstance(d,dict) and "content" in d:
                self._text.delete("1.0","end"); self._text.insert("1.0",d["content"]); self._msg.config(text=f"Carregado: {n}")
        except Exception as e: self._msg.config(text=f"Erro: {e}")

    def _save(self):
        n=self._path.get().strip()
        if not n: return
        if not n.startswith("/"): n="/"+n
        c=self._text.get("1.0","end-1c")
        try:
            r=self.cli.fs_write(n,c)
            if isinstance(r,dict) and r.get("status")=="ok": self._msg.config(text=f"Salvo: {n}"); self._list()
        except Exception as e: self._msg.config(text=f"Erro: {e}")

    def _delete(self):
        sel=self._listbox.curselection()
        if not sel: return
        n=self._listbox.get(sel[0]).split(" (")[0]
        if not n.startswith("/"): n="/"+n
        if messagebox.askyesno("Deletar",f"Deletar {n}?"):
            try:
                r=self.cli.fs_delete(n)
                if isinstance(r,dict) and r.get("status")=="ok": self._msg.config(text=f"Deletado: {n}"); self._list()
            except Exception as e: self._msg.config(text=f"Erro: {e}")


class ConfigContent(tk.Frame):
    def __init__(self,parent,cli):
        super().__init__(parent,bg=WIN_BG)
        self.cli=cli
        tk.Label(self,text="Configuracoes",font=("System",9,"bold"),bg=WIN_BG).pack()
        f1=tk.LabelFrame(self,text="WiFi",bg=WIN_BG,fg="black",font=("System",7),relief="groove",padx=6,pady=4)
        f1.pack(fill="x",padx=6,pady=4)
        tk.Label(f1,text="SSID",bg=WIN_BG).grid(row=0,column=0,sticky="w")
        self._ssid=tk.Entry(f1,bg="white",fg="black",font=("System",8),relief="sunken")
        self._ssid.grid(row=0,column=1,sticky="ew",padx=4); f1.grid_columnconfigure(1,weight=1)
        tk.Label(f1,text="Senha",bg=WIN_BG).grid(row=1,column=0,sticky="w")
        self._pass=tk.Entry(f1,bg="white",fg="black",font=("System",8),relief="sunken",show="*")
        self._pass.grid(row=1,column=1,sticky="ew",padx=4)
        f2=tk.LabelFrame(self,text="Seguranca",bg=WIN_BG,fg="black",font=("System",7),relief="groove",padx=6,pady=4)
        f2.pack(fill="x",padx=6,pady=4)
        tk.Label(f2,text="PIN",bg=WIN_BG).grid(row=0,column=0,sticky="w")
        self._pin=tk.Entry(f2,bg="white",fg="black",font=("System",8),relief="sunken",show="*")
        self._pin.grid(row=0,column=1,sticky="ew",padx=4); f2.grid_columnconfigure(1,weight=1)
        tk.Label(f2,text="Nome",bg=WIN_BG).grid(row=1,column=0,sticky="w")
        self._name=tk.Entry(f2,bg="white",fg="black",font=("System",8),relief="sunken")
        self._name.grid(row=1,column=1,sticky="ew",padx=4)
        bf=tk.Frame(self,bg=WIN_BG); bf.pack(pady=4)
        tk.Button(bf,text="Salvar",bg=BTN_FACE,relief="raised",command=self._save).pack(side="left",padx=2)
        tk.Button(bf,text="Reiniciar",bg=BTN_FACE,relief="raised",command=self._restart).pack(side="left",padx=2)
        tk.Button(bf,text="Status",bg=BTN_FACE,relief="raised",command=self._status).pack(side="left",padx=2)
        self._out=scrolledtext.ScrolledText(self,bg="white",fg="black",font=("Courier",8),relief="sunken",height=6)
        self._out.pack(fill="x",padx=6,pady=4)

    def _log(self,m): self._out.insert("end",m+"\n"); self._out.see("end")
    def _save(self):
        s=self._ssid.get().strip(); p=self._pass.get().strip()
        pin=self._pin.get().strip(); n=self._name.get().strip()
        if not s: self._log("SSID obrigatorio"); return
        try: r=self.cli.save_config(s,p,pin,n); self._log(f"Salvo: {r}")
        except Exception as e: self._log(f"Erro: {e}")
    def _restart(self):
        if messagebox.askyesno("Reiniciar","Reiniciar ESP32?"):
            try: self.cli.restart(); self._log("Reiniciando...")
            except Exception as e: self._log(f"Erro: {e}")
    def _status(self):
        try:
            s=self.cli.status(); self._out.delete("1.0","end")
            if isinstance(s,dict):
                for k,v in s.items(): self._out.insert("end",f"{k}: {v}\n")
            else: self._out.insert("end",str(s))
        except Exception as e: self._log(f"Erro: {e}")


class StoreContent(tk.Frame):
    def __init__(self,parent,cli):
        super().__init__(parent,bg=WIN_BG)
        self.cli=cli; self._apps=[]
        tk.Label(self,text="Loja de Apps",font=("System",9,"bold"),bg=WIN_BG).pack()
        bf=tk.Frame(self,bg=WIN_BG); bf.pack(fill="x",padx=4,pady=2)
        tk.Button(bf,text="Buscar",bg=BTN_FACE,relief="raised",command=self._fetch).pack(side="left",padx=2)
        tk.Button(bf,text="Instalados",bg=BTN_FACE,relief="raised",command=self._installed).pack(side="left",padx=2)
        tk.Button(bf,text="Instalar",bg=BTN_FACE,relief="raised",command=self._install).pack(side="right",padx=2)
        p=tk.PanedWindow(self,orient="horizontal",bg=WIN_BG,sashwidth=3)
        p.pack(fill="both",expand=True,padx=4,pady=4)
        left=tk.Frame(p,bg=WIN_BG); p.add(left,width=250)
        tk.Label(left,text="Disponiveis",bg=WIN_BG,font=("System",7)).pack()
        self._list=tk.Listbox(left,bg="white",fg="black",font=("System",8),relief="sunken")
        self._list.pack(fill="both",expand=True)
        right=tk.Frame(p,bg=WIN_BG); p.add(right,width=180)
        tk.Label(right,text="Instalados",bg=WIN_BG,font=("System",7)).pack()
        self._inst=tk.Listbox(right,bg="white",fg="black",font=("System",8),relief="sunken")
        self._inst.pack(fill="both",expand=True)
        self._msg=tk.Label(self,text="",bg=WIN_BG,font=("System",7)); self._msg.pack()

    def _fetch(self):
        self._msg.config(text="Buscando..."); self.update()
        try:
            import urllib.request
            u="https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/apps/manifest.json"
            r=urllib.request.urlopen(u,timeout=10); d=json.loads(r.read())
            self._apps=d.get("apps",[]); self._list.delete(0,"end")
            for a in self._apps: self._list.insert("end",f"{a.get('icon','')} {a['name']} - {a.get('desc','')}")
            self._msg.config(text=f"{len(self._apps)} apps")
        except Exception as e: self._msg.config(text=f"Erro: {e}")

    def _installed(self):
        try:
            d=self.cli.apps(); self._inst.delete(0,"end")
            if isinstance(d,dict):
                for a in d.get("apps",[]): self._inst.insert("end",f"{a['id']} ({a.get('size',0)} B)")
                self._msg.config(text=f"{len(d['apps'])} instalados")
        except Exception as e: self._msg.config(text=f"Erro: {e}")

    def _install(self):
        sel=self._list.curselection()
        if not sel or not self._apps: return
        a=self._apps[sel[0]]
        u=f"https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/apps/{a.get('file',a['id']+'.html')}"
        self._msg.config(text=f"Instalando {a['name']}..."); self.update()
        try: self.cli.install_app(u); self._msg.config(text=f"OK {a['name']}!"); self._installed()
        except Exception as e: self._msg.config(text=f"Erro: {e}")


class TerminalContent(tk.Frame):
    def __init__(self,parent,cli):
        super().__init__(parent,bg=WIN_BG)
        self.cli=cli
        self._log=scrolledtext.ScrolledText(self,bg="black",fg="#0f0",font=("Courier",9),relief="sunken")
        self._log.pack(fill="both",expand=True,padx=4,pady=4)
        b=tk.Frame(self,bg=WIN_BG); b.pack(fill="x",padx=4,pady=(0,4))
        self._input=tk.Entry(b,bg="white",fg="black",font=("Courier",9),relief="sunken")
        self._input.pack(side="left",fill="x",expand=True)
        self._input.bind("<Return>",self._send)
        tk.Button(b,text="Enviar",bg=BTN_FACE,relief="raised",command=self._send).pack(side="left",padx=(4,0))
        tk.Button(b,text="Limpar",bg=BTN_FACE,relief="raised",
                 command=lambda: self._log.delete("1.0","end")).pack(side="left",padx=2)

    def _send(self,e=None):
        cmd=self._input.get().strip()
        if not cmd: return
        self._input.delete(0,"end"); self._log.insert("end",f"> {cmd}\n"); self._log.see("end")
        try:
            if self.cli.mode=="serial": resp=self.cli._serial_cmd(cmd)
            elif self.cli.mode=="tcp":
                if cmd.startswith("GET "): resp=self.cli.api_get(cmd[4:])
                elif cmd.startswith("POST "):
                    rest=cmd[5:]; sp=rest.index(" "); resp=self.cli.api_post(rest[:sp],json.loads(rest[sp+1:]))
                else: resp="GET /path ou POST /path {...}"
            else: resp="Nao conectado"
            self._log.insert("end",f"{resp}\n")
        except Exception as ex: self._log.insert("end",f"Erro: {ex}\n")
        self._log.see("end")


# ── Desktop ──────────────────────────────────────────────────────

class ESPortableDesktop:
    def __init__(self):
        self.cli=ESPClient()
        self._wins={}; self._win_defs={}; self._icons=[]; self._start_menu=None

        self.root=tk.Tk()
        self.root.title("ESPortable32")
        self.root.geometry("960x640")
        self.root.minsize(640,480)
        self.root.configure(bg=DESKTOP)

        self._show_bios()
        self.root.protocol("WM_DELETE_WINDOW",self._quit)
        self.root.mainloop()

    def _quit(self):
        self.cli.disconnect(); self.root.destroy()

    # ── Boot sequence ────────────────────────────────────────────

    def _clear(self):
        for w in self.root.winfo_children(): w.destroy()
        if hasattr(self,'_start_menu') and self._start_menu:
            try: self._start_menu.destroy()
            except: pass
            self._start_menu=None

    def _show_bios(self):
        self._clear()
        self.root.geometry("800x500")
        self.bios=BIOSScreen(self.root,self.cli,on_found=self._on_bios_found,
                            on_skip=self._show_connect)
        self.bios.start()

    def _on_bios_found(self):
        self.bios.stop()
        self._show_boot()

    def _show_boot(self):
        self._clear()
        self.root.geometry("600x400")
        self.boot=BootScreen(self.root,"setup",on_done=self._on_boot_done)

    def _on_boot_done(self):
        self.boot.destroy()
        # Check status
        try:
            s=self.cli.status()
            if isinstance(s,dict):
                mode=s.get("state_name","")
                if mode=="SETUP" or "setup" in str(mode).lower():
                    self._show_setup_wizard()
                    return
        except: pass
        # Default: go to desktop
        self._show_desktop()

    def _show_setup_wizard(self):
        self._clear()
        wiz=SetupWizard(self.root,self.cli,on_complete=self._on_setup_done)

    def _on_setup_done(self):
        # Wait for ESP32 to reboot, then try to reconnect
        self._clear()
        f=tk.Frame(self.root,bg=BLACK)
        f.pack(fill="both",expand=True)
        tk.Label(f,text="Aguardando reinicializacao do ESP32...",font=("System",9,"bold"),
                fg="#0f0",bg=BLACK).pack(expand=True)
        tk.Label(f,text="A interface vai conectar automaticamente.",fg="#0a0",bg=BLACK,
                font=("System",8)).pack()
        self.root.update()

        def wait():
            self.cli.disconnect()
            time.sleep(3)
            # Re-scan
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
                ok,_=self.cli.connect_serial(port,115200)
                if ok:
                    self.cli.mode="serial"
                    self.root.after(0,self._show_boot)
                    return
            self.root.after(0,self._show_connect)

        threading.Thread(target=wait,daemon=True).start()

    # ── Manual connect ───────────────────────────────────────────

    def _show_connect(self):
        self._clear()
        self.root.geometry("500x430")

        f=tk.Frame(self.root,bg=DESKTOP); f.pack(expand=True,fill="both",padx=40,pady=40)
        tk.Label(f,text="ESPortable32",font=("",22,"bold"),fg=TITLE_FG,bg=DESKTOP).pack(pady=(0,5))
        tk.Label(f,text="Conectar ao ESP32",font=("",12),fg="#fff",bg=DESKTOP).pack(pady=(0,20))

        nb=ttk.Notebook(f); nb.pack(fill="both",expand=True)
        # TCP tab
        tcp_f=tk.Frame(nb,bg=WIN_BG,padx=20,pady=20); nb.add(tcp_f,text=" TCP/IP ")
        tk.Label(tcp_f,text="IP",fg="black",bg=WIN_BG,anchor="w").pack(fill="x")
        self.tcp_host=tk.Entry(tcp_f,bg="white",fg="black",font=("",11),relief="sunken",bd=2)
        self.tcp_host.insert(0,"192.168.2.38"); self.tcp_host.pack(fill="x",pady=(2,10))
        tk.Label(tcp_f,text="Porta",fg="black",bg=WIN_BG,anchor="w").pack(fill="x")
        self.tcp_port=tk.Entry(tcp_f,bg="white",fg="black",font=("",11),relief="sunken",bd=2)
        self.tcp_port.insert(0,"80"); self.tcp_port.pack(fill="x",pady=(2,15))
        self.tcp_btn=tk.Button(tcp_f,text="Conectar TCP/IP",bg=BTN_FACE,relief="raised",
                              font=("",10,"bold"),cursor="hand2",padx=20,pady=8,command=self._connect_tcp)
        self.tcp_btn.pack()
        # Serial tab
        ser_f=tk.Frame(nb,bg=WIN_BG,padx=20,pady=20); nb.add(ser_f,text=" Serial ")
        tk.Label(ser_f,text="Porta",fg="black",bg=WIN_BG,anchor="w").pack(fill="x")
        self.ser_port=tk.Entry(ser_f,bg="white",fg="black",font=("",11),relief="sunken",bd=2)
        self.ser_port.insert(0,"/dev/ttyACM0"); self.ser_port.pack(fill="x",pady=(2,10))
        tk.Label(ser_f,text="Baud",fg="black",bg=WIN_BG,anchor="w").pack(fill="x")
        self.ser_baud=tk.Entry(ser_f,bg="white",fg="black",font=("",11),relief="sunken",bd=2)
        self.ser_baud.insert(0,"115200"); self.ser_baud.pack(fill="x",pady=(2,15))
        self.ser_btn=tk.Button(ser_f,text="Conectar Serial",bg=BTN_FACE,relief="raised",
                              font=("",10,"bold"),cursor="hand2",padx=20,pady=8,command=self._connect_serial)
        self.ser_btn.pack()
        self.con_status=tk.Label(f,text="",fg="#fff",bg=DESKTOP,font=("",9))
        self.con_status.pack(pady=(15,0))

    def _connect_tcp(self):
        h=self.tcp_host.get().strip(); p=self.tcp_port.get().strip()
        if not h: self.con_status.config(text="Informe o IP",fg="red"); return
        self.tcp_btn.config(state="disabled",text="Conectando..."); self.root.update()
        ok,r=self.cli.connect_tcp(h,int(p) if p else 80)
        if ok: self._on_connect(r)
        else: self.con_status.config(text=f"Erro: {r}",fg="red"); self.tcp_btn.config(state="normal",text="Conectar TCP/IP")

    def _connect_serial(self):
        p=self.ser_port.get().strip(); b=self.ser_baud.get().strip()
        if not p: self.con_status.config(text="Informe a porta",fg="red"); return
        self.ser_btn.config(state="disabled",text="Conectando..."); self.root.update()
        ok,r=self.cli.connect_serial(p,int(b) if b else 115200)
        if ok: self._on_connect(None)
        else: self.con_status.config(text=f"Erro: {r}",fg="red"); self.ser_btn.config(state="normal",text="Conectar Serial")

    def _on_connect(self,status_data):
        self._show_desktop()

    # ── Desktop ──────────────────────────────────────────────────

    def _show_desktop(self):
        self._clear()
        self.root.geometry("960x640")

        self.canvas=tk.Frame(self.root,bg=DESKTOP)
        self.canvas.pack(fill="both",expand=True)

        self._icons=[]
        for i,(name,icon) in enumerate(ICON_APPS):
            dic=DesktopIcon(self.canvas,name,icon,command=lambda n=name: self._open_app(n))
            dic.place(x=20+(i%3)*100,y=20+(i//3)*100); self._icons.append(dic)

        self.taskbar=Taskbar(self.root,self._show_start)

        self._win_defs={
            "Painel":  {"icon":"\u2302","w":500,"h":280,"fn":lambda p: DashboardContent(p,self.cli)},
            "GPIO":    {"icon":"\u26A1","w":520,"h":340,"fn":lambda p: GPIOContent(p,self.cli)},
            "Editor":  {"icon":"\u270E","w":600,"h":380,"fn":lambda p: EditorContent(p,self.cli)},
            "Config":  {"icon":"\u2699","w":480,"h":360,"fn":lambda p: ConfigContent(p,self.cli)},
            "Loja":    {"icon":"\u2728","w":540,"h":360,"fn":lambda p: StoreContent(p,self.cli)},
            "Terminal":{"icon":"\u2328","w":560,"h":360,"fn":lambda p: TerminalContent(p,self.cli)},
        }
        self._windows={}
        self._start_menu=None

    def _open_app(self,name):
        if name not in self._win_defs: return
        d=self._win_defs[name]
        if name not in self._windows or not self._windows[name]._open:
            w=Win98Window(self,name,d["icon"],d["fn"],w=d["w"],h=d["h"])
            self._windows[name]=w; w.open()
        else: self._windows[name]._raise()

    def _show_start(self):
        if self._start_menu and self._start_menu.winfo_exists():
            self._start_menu.destroy(); self._start_menu=None; return
        menu=tk.Frame(self.root,bg=BTN_FACE,bd=0,highlightbackground=DARK_SHAD,highlightthickness=2)
        menu.place(x=2,y=self.root.winfo_height()-130,width=200)
        items=[
            ("\u2302 Painel",lambda: self._open_app("Painel")),
            ("\u26A1 GPIO",lambda: self._open_app("GPIO")),
            ("\u270E Editor",lambda: self._open_app("Editor")),
            ("\u2699 Config",lambda: self._open_app("Config")),
            ("\u2728 Loja",lambda: self._open_app("Loja")),
            ("\u2328 Terminal",lambda: self._open_app("Terminal")),
            ("---",None),
            ("\u2B95 Desconectar",self._disconnect),
            ("\u274C Sair",self._quit),
        ]
        for txt,cmd in items:
            if txt.startswith("---"):
                tk.Frame(menu,bg=BTN_SHAD,height=1).pack(fill="x",padx=4,pady=4); continue
            b=tk.Label(menu,text=f"  {txt}",bg=BTN_FACE,fg="black",font=("System",8),
                      anchor="w",cursor="hand2",padx=8,pady=2)
            b.pack(fill="x")
            if cmd:
                b.bind("<Button-1>",lambda e,c=cmd: [menu.destroy(),c()])
                b.bind("<Enter>",lambda e,b=b: b.configure(bg="#000080",fg="white"))
                b.bind("<Leave>",lambda e,b=b: b.configure(bg=BTN_FACE,fg="black"))
        self._start_menu=menu
        self.root.bind("<Button-1>",self._close_start,add="+")

    def _close_start(self,e):
        if self._start_menu and self._start_menu.winfo_exists():
            if e.widget!=self.taskbar.start_btn:
                try: self._start_menu.destroy(); self._start_menu=None
                except: pass

    def _disconnect(self):
        self.cli.disconnect()
        for n,w in self._windows.items():
            if w._open: w.close()

    def _on_window_open(self,w): self.taskbar.add_button(w)
    def _on_window_close(self,w): self.taskbar.remove_button(w)
    def _on_window_raise(self,w): self.taskbar.highlight(w)
    def _on_window_minimize(self,w): pass
    def _on_window_restore(self,w): self.taskbar.add_button(w)


def main():
    ESPortableDesktop()

if __name__=="__main__":
    main()
