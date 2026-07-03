#!/usr/bin/env python3
"""ESPortable32 - Ferramenta geral do projeto ESP32."""

import argparse
import glob
import os
import subprocess
import sys
import textwrap
import time

PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR = os.path.join(PROJECT_DIR, ".pio", "build", "esportable32")
DATA_DIR = os.path.join(PROJECT_DIR, "data")

# ── Cores ──────────────────────────────────────────────────────────
VERMELHO = "\033[91m"
VERDE = "\033[92m"
AMARELO = "\033[93m"
AZUL = "\033[94m"
CIANO = "\033[96m"
NEGRITO = "\033[1m"
RESET = "\033[0m"


def info(msg):    print(f"  {AZUL}i{RESET} {msg}")
def ok(msg):      print(f"  {VERDE}✓{RESET} {msg}")
def warn(msg):    print(f"  {AMARELO}⚠{RESET} {msg}")
def err(msg):     print(f"  {VERMELHO}✗{RESET} {msg}")
def header(t):    print(f"\n  {NEGRITO}{CIANO}{'─' * 50}{RESET}\n  {NEGRITO}{t}{RESET}\n  {CIANO}{'─' * 50}{RESET}\n")


# ── Dependências ───────────────────────────────────────────────────
VENV_PIO = "/tmp/pio-venv/bin/pio"
VENV_ESPTOOL = "/tmp/pio-venv/bin/esptool.py"


def find_pio():
    if os.path.exists(VENV_PIO):
        return VENV_PIO
    for cmd in ("pio", "platformio"):
        try:
            subprocess.run([cmd, "--version"], capture_output=True, check=True)
            return cmd
        except (FileNotFoundError, subprocess.CalledProcessError):
            continue
    return None


def find_esptool():
    if os.path.exists(VENV_ESPTOOL):
        return VENV_ESPTOOL
    for cmd in ("esptool.py", "esptool"):
        try:
            subprocess.run([cmd, "version"], capture_output=True, check=True)
            return cmd
        except (FileNotFoundError, subprocess.CalledProcessError):
            continue
    return None


def find_serial():
    ports = glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*")
    return ports[0] if ports else None


# ── Comandos ───────────────────────────────────────────────────────

def cmd_install():
    """Instala dependências (PlatformIO + esptool)."""
    header("Instalando dependências")

    pio = find_pio()
    if pio:
        ok(f"PlatformIO já instalado: {pio}")
    else:
        info("Criando virtual environment...")
        subprocess.run([sys.executable, "-m", "venv", "/tmp/pio-venv"], check=True)
        info("Instalando PlatformIO...")
        subprocess.run(["/tmp/pio-venv/bin/pip", "install", "platformio", "esptool"], check=True)
        ok("PlatformIO + esptool instalados!")

    esptool = find_esptool()
    if esptool:
        ok(f"esptool já instalado: {esptool}")

    # udev rules
    rules = "/etc/udev/rules.d/99-platformio-udev.rules"
    if not os.path.exists(rules):
        warn("Regras udev não encontradas.")
        info("Para acesso à porta serial sem sudo:")
        info("  curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/master/scripts/99-platformio-udev.rules | sudo tee /etc/udev/rules.d/99-platformio-udev.rules")
        info("  sudo udevadm control --reload-rules")
        info("  sudo usermod -aG dialout $USER")
    else:
        ok("Regras udev OK")


def cmd_build():
    """Compila firmware + LittleFS."""
    pio = find_pio()
    if not pio:
        err("PlatformIO não encontrado. Execute: python3 esportable32.py install")
        return

    header("Compilando firmware")
    r = subprocess.run([pio, "run", "--project-dir", PROJECT_DIR], capture_output=True, text=True)
    print(r.stdout)

    if r.returncode != 0:
        print(r.stderr)
        err("Falha na compilação!")
        sys.exit(1)

    ok("Firmware compilado!")
    elf = os.path.join(BUILD_DIR, "firmware.elf")
    if os.path.exists(elf):
        r2 = subprocess.run([pio, "run", "--project-dir", PROJECT_DIR, "--target", "size"],
                           capture_output=True, text=True)
        for line in r2.stdout.splitlines():
            if "RAM" in line or "Flash" in line:
                print(f"     {line.strip()}")


def cmd_flash():
    """Compila e grava firmware + LittleFS no ESP32."""
    cmd_build()

    port = find_serial()
    if not port:
        err("Nenhuma porta serial encontrada (/dev/ttyACM* ou /dev/ttyUSB*)")
        sys.exit(1)

    esptool = find_esptool()
    if not esptool:
        err("esptool não encontrado. Execute: python3 esportable32.py install")
        sys.exit(1)

    header(f"Gravando no ESP32 ({port})")

    print()
    info(f"Porta:  {port}")
    info(f"Baud:   921600")

    # Firmware + bootloader + partitions
    files = [
        ("0x1000",  os.path.join(BUILD_DIR, "bootloader.bin")),
        ("0x8000",  os.path.join(BUILD_DIR, "partitions.bin")),
        ("0xe000",  os.path.join(BUILD_DIR, "boot_app0.bin")),
        ("0x10000", os.path.join(BUILD_DIR, "firmware.bin")),
    ]

    # LittleFS
    littlefs = os.path.join(BUILD_DIR, "littlefs.bin")
    if os.path.exists(littlefs):
        files.append(("0x2E0000", littlefs))
    else:
        warn("LittleFS image não encontrada. Execute: python3 esportable32.py uploadfs")

    args = [esptool, "--port", port, "--baud", "921600", "write_flash"]
    missing = []
    for addr, path in files:
        if os.path.exists(path):
            args.extend([addr, path])
            size_kb = os.path.getsize(path) / 1024
            print(f"     {VERDE}{addr}{RESET} → {os.path.basename(path)} ({size_kb:.0f} KB)")
        else:
            missing.append(os.path.basename(path))

    if not any(os.path.exists(p) for _, p in files):
        err("Nenhum arquivo de firmware encontrado! Compile primeiro.")
        sys.exit(1)

    if missing:
        warn(f"Não encontrado: {', '.join(missing)}")

    r = subprocess.run(args)
    if r.returncode == 0:
        ok("Gravação concluída!")
        print()
        info("Desconecte e reconecte o USB ou aguarde o reset automático.")
        info("Para configurar o WiFi, execute: python3 esportable32.py config")
    else:
        err("Falha na gravação!")


def cmd_monitor():
    """Abre o monitor serial."""
    pio = find_pio()
    if not pio:
        err("PlatformIO não encontrado.")
        return

    header(f"Monitor Serial (Ctrl+C para sair)")
    info("Conectando...")
    subprocess.run([pio, "device", "monitor", "--project-dir", PROJECT_DIR, "--baud", "115200"])


def cmd_uploadfs():
    """Compila e envia LittleFS."""
    pio = find_pio()
    if not pio:
        err("PlatformIO não encontrado.")
        return

    header("LittleFS - Upload")
    info("Compilando filesystem...")
    r = subprocess.run([pio, "run", "--project-dir", PROJECT_DIR, "--target", "buildfs"],
                       capture_output=True, text=True)
    for line in r.stdout.splitlines():
        if "/" in line and ("html" in line or "js" in line or "css" in line):
            print(f"     {line}")

    if r.returncode != 0:
        print(r.stderr)
        err("Falha ao compilar LittleFS!")
        sys.exit(1)

    info("Enviando para o ESP32...")
    r2 = subprocess.run([pio, "run", "--project-dir", PROJECT_DIR, "--target", "uploadfs"],
                        capture_output=True, text=True)
    print(r2.stdout)
    if r2.returncode == 0:
        ok("LittleFS atualizado! Reinicie o ESP32.")
    else:
        err("Falha ao enviar LittleFS!")


def cmd_config():
    """Configura WiFi e PIN via serial."""
    port = find_serial()
    if not port:
        err("Nenhuma porta serial encontrada!")
        return

    header("Configuração via Serial")
    info(f"Porta: {port}")
    print()

    try:
        import serial
    except ImportError:
        err("Biblioteca 'pyserial' não encontrada.")
        info("Instale: pip install pyserial")
        return

    s = serial.Serial(port, 115200, timeout=3)
    s.dtr = False
    time.sleep(0.1)
    s.dtr = True
    time.sleep(2)

    # Ler boot
    out = b""
    start = time.time()
    while time.time() - start < 8:
        if s.in_waiting:
            out += s.read(s.in_waiting)
            if b"Digite HELP" in out or b"URL:" in out or b"Ready at" in out:
                break
        time.sleep(0.1)

    txt = out.decode("utf-8", errors="replace")

    if "Ready at" in txt:
        ok("ESP32 já está conectado ao WiFi!")
        for line in txt.splitlines():
            if "Ready at" in line:
                print(f"     {line.strip()}")
        s.close()
        return

    if "Modo Setup" not in txt:
        warn("ESP32 não está em modo setup.")
        info("Se estiver configurado mas você quer trocar a rede,")
        info("conecte via serial e digite RESET para resetar.")
        s.close()
        return

    print(txt.split("Comandos:")[-1].split("───")[0].strip())
    print()

    # WiFi
    ssid = input(f"  {NEGRITO}SSID{RESET} da rede WiFi: ").strip()
    if not ssid:
        warn("SSID não informado. Cancelando.")
        s.close()
        return

    password = input(f"  {NEGRITO}Senha{RESET} (vazio se for aberta): ").strip()

    # PIN opcional
    pin = input(f"  {NEGRITO}PIN{RESET} de bloqueio (opcional, Enter para pular): ").strip()

    print()
    info("Enviando configurações...")

    s.write(f"WIFI={ssid},{password}\n".encode())
    time.sleep(0.3)

    if pin:
        s.write(f"PIN={pin}\n".encode())
        time.sleep(0.3)

    s.write(b"SAVE\n")
    time.sleep(1)

    # Ler resposta
    resp = b""
    time.sleep(0.5)
    while s.in_waiting:
        resp += s.read(s.in_waiting)
    s.close()

    resp_txt = resp.decode("utf-8", errors="replace")
    print(f"  {resp_txt.replace(chr(10), chr(10)+'  ')}")

    if "OK" in resp_txt:
        ok("Configuração salva! ESP32 vai reiniciar.")
        info("Aguarde ~20s e acesse:")
        info("  http://esportable32.local")
        info("  ou veja o IP no serial: python3 esportable32.py monitor")
    else:
        warn("Pode ter ocorrido um erro. Verifique o monitor serial.")
        info("python3 esportable32.py monitor")


def cmd_status():
    """Mostra o status do ESP32 via serial."""
    port = find_serial()
    if not port:
        err("Nenhuma porta serial encontrada!")
        return

    header("Status do ESP32")

    try:
        import serial
    except ImportError:
        err("pyserial não instalada. pip install pyserial")
        return

    s = serial.Serial(port, 115200, timeout=1)
    s.dtr = False
    time.sleep(0.1)
    s.dtr = True
    time.sleep(2)

    # Espera boot + envia STATUS
    start = time.time()
    booted = False
    out = b""
    while time.time() - start < 15:
        if s.in_waiting:
            data = s.read(s.in_waiting)
            out += data
            txt = data.decode("utf-8", errors="replace")
            for line in txt.splitlines():
                if "Digite HELP" in line or "Ready at" in line or "Heartbeat" in line:
                    booted = True
        if booted:
            break
        time.sleep(0.1)

    if not booted:
        warn("ESP32 não respondeu. Verifique a conexão USB.")
        s.close()
        return

    s.write(b"STATUS\n")
    time.sleep(1)

    resp = b""
    while s.in_waiting:
        resp += s.read(s.in_waiting)
    s.close()

    txt = resp.decode("utf-8", errors="replace")
    for line in txt.splitlines():
        if line.strip():
            key = line.split(":")[0] if ":" in line else ""
            val = ":".join(line.split(":")[1:]) if ":" in line else ""
            if key:
                print(f"  {NEGRITO}{key.strip()}{RESET}:{val}")
            else:
                print(f"  {line}")

    # Tenta também via HTTP
    import socket
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(2)
        sock.connect(("192.168.2.38", 80))
        sock.send(b"GET /api/status HTTP/1.1\r\nHost: 192.168.2.38\r\nConnection: close\r\n\r\n")
        data = b""
        while True:
            chunk = sock.recv(1024)
            if not chunk:
                break
            data += chunk
        sock.close()
        if data:
            body = data.split(b"\r\n\r\n")[-1].decode()
            print(f"\n  {CIANO}── HTTP ──{RESET}")
            print(f"  {body}")
    except Exception:
        pass


def cmd_menu():
    """Modo interativo com menu."""
    while True:
        os.system("clear" if os.name == "posix" else "cls")
        header("ESPortable32 - Menu Principal")
        print(f"  {NEGRITO}1{RESET}   Compilar firmware")
        print(f"  {NEGRITO}2{RESET}   Gravar no ESP32 (flash + littlefs)")
        print(f"  {NEGRITO}3{RESET}   Monitor serial")
        print(f"  {NEGRITO}4{RESET}   Upload LittleFS (arquivos web)")
        print(f"  {NEGRITO}5{RESET}   Configurar WiFi via serial")
        print(f"  {NEGRITO}6{RESET}   Status do ESP32")
        print(f"  {NEGRITO}7{RESET}   Instalar dependências")
        print(f"  {NEGRITO}0{RESET}   Sair")
        print()

        try:
            op = input(f"  {NEGRITO}>>>{RESET} ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            break

        actions = {
            "1": cmd_build,
            "2": cmd_flash,
            "3": cmd_monitor,
            "4": cmd_uploadfs,
            "5": cmd_config,
            "6": cmd_status,
            "7": cmd_install,
            "0": lambda: sys.exit(0),
        }

        if op in actions:
            actions[op]()
            if op != "0":
                print(f"\n  {NEGRITO}Pressione Enter para voltar ao menu...{RESET}", end="")
                try:
                    input()
                except (EOFError, KeyboardInterrupt):
                    break
        else:
            warn("Opção inválida!")
            time.sleep(1)


# ── Main ───────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        prog="esportable32.py",
        description="Ferramenta geral do ESPortable32 - ESP32 web server",
        epilog=textwrap.dedent("""\
            Exemplos:
              python3 esportable32.py install     Instalar dependências
              python3 esportable32.py build       Compilar firmware
              python3 esportable32.py flash       Gravar no ESP32
              python3 esportable32.py config      Configurar WiFi
              python3 esportable32.py menu        Modo interativo
              python3 esportable32.py monitor     Monitor serial
        """),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument(
        "acao",
        nargs="?",
        default="menu",
        choices=["install", "build", "flash", "monitor", "uploadfs",
                 "config", "status", "menu", "help"],
        help="Ação a executar (default: menu)",
    )

    args = parser.parse_args()

    if args.acao == "help":
        parser.print_help()
        return

    actions = {
        "install": cmd_install,
        "build": cmd_build,
        "flash": cmd_flash,
        "monitor": cmd_monitor,
        "uploadfs": cmd_uploadfs,
        "config": cmd_config,
        "status": cmd_status,
        "menu": cmd_menu,
    }

    actions[args.acao]()


if __name__ == "__main__":
    main()
