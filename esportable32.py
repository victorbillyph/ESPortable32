#!/usr/bin/env python3
"""ESPortable32 - Ferramenta geral do projeto ESP32."""

import argparse
import glob
import os
import subprocess
import sys
import time

PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR = os.path.join(PROJECT_DIR, ".pio", "build", "esportable32")

VERMELHO = "\033[91m"
VERDE = "\033[92m"
AMARELO = "\033[93m"
AZUL = "\033[94m"
CIANO = "\033[96m"
NEGRITO = "\033[1m"
RESET = "\033[0m"


def info(msg):  print(f"  {AZUL}i{RESET} {msg}")
def ok(msg):    print(f"  {VERDE}✓{RESET} {msg}")
def warn(msg):  print(f"  {AMARELO}⚠{RESET} {msg}")
def err(msg):   print(f"  {VERMELHO}✗{RESET} {msg}")
def header(t):  print(f"\n  {NEGRITO}{CIANO}{'─' * 50}{RESET}\n  {NEGRITO}{t}{RESET}\n  {CIANO}{'─' * 50}{RESET}\n")


# ── Utilitários ───────────────────────────────────────────────────

def executar(cmd, **kwargs):
    return subprocess.run(cmd, **kwargs)


def achar_pio():
    for p in ("/tmp/pio-venv/bin/pio", f"{PROJECT_DIR}/.venv/bin/pio"):
        if os.path.exists(p):
            return p
    for cmd in ("pio", "platformio"):
        try:
            subprocess.run([cmd, "--version"], capture_output=True, check=True)
            return cmd
        except (FileNotFoundError, subprocess.CalledProcessError):
            continue
    return None


def achar_esptool():
    for p in ("/tmp/pio-venv/bin/esptool.py", "/tmp/pio-venv/bin/esptool",
              f"{PROJECT_DIR}/.venv/bin/esptool.py", f"{PROJECT_DIR}/.venv/bin/esptool"):
        if os.path.exists(p):
            return p
    for cmd in ("esptool.py", "esptool"):
        try:
            subprocess.run([cmd, "version"], capture_output=True, check=True)
            return cmd
        except (FileNotFoundError, subprocess.CalledProcessError):
            continue
    return None


def achar_porta():
    ports = glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*")
    return ports[0] if ports else None


# ── 1: Instalar no ESP32 ──────────────────────────────────────────

def opcao_instalar():
    header("Instalar ESPortable32 no ESP32")

    pio = achar_pio()
    if not pio:
        err("PlatformIO não encontrado. Execute primeiro: python3 esportable32.py")
        sys.exit(1)

    port = achar_porta()
    if not port:
        err("Nenhum ESP32 encontrado. Conecte o cabo USB.")
        sys.exit(1)

    info(f"ESP32 detectado em: {port}")
    print()

    # 1. Compilar
    info("Compilando firmware...")
    r = executar([pio, "run", "--project-dir", PROJECT_DIR], capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stderr)
        err("Falha na compilação!")
        sys.exit(1)
    ok("Firmware compilado")

    # 2. Apagar flash (formatação completa)
    info("A pagando flash (formatação completa)...")
    esptool = achar_esptool()
    executar([esptool, "--port", port, "--baud", "921600", "erase_flash"],
             capture_output=False)
    ok("Flash apagado")

    # 3. Gravar firmware + bootloader + partitions
    info("Gravando firmware...")
    bl = os.path.join(BUILD_DIR, "bootloader.bin")
    pt = os.path.join(BUILD_DIR, "partitions.bin")
    fw = os.path.join(BUILD_DIR, "firmware.bin")
    lfs = os.path.join(BUILD_DIR, "littlefs.bin")

    args = [esptool, "--port", port, "--baud", "921600", "write_flash"]
    for addr, path in [("0x1000", bl), ("0x8000", pt), ("0xe000",
                       os.path.join(BUILD_DIR, "boot_app0.bin")),
                       ("0x10000", fw)]:
        if os.path.exists(path):
            args.extend([addr, path])

    if os.path.exists(lfs):
        args.extend(["0x2E0000", lfs])

    r = executar(args, capture_output=False)
    if r.returncode != 0:
        err("Falha na gravação!")
        sys.exit(1)

    ok("Instalação concluída!")
    print()
    info("Modo setup ativo. Configure o WiFi:")
    print()
    info("  1. Conecte-se à rede WiFi: ESPortable32-Setup (senha: configurar)")
    info("  2. Acesse http://192.168.4.1")
    info("  Ou configure via serial:")
    info("    python3 " + sys.argv[0])
    print()


# ── 2: Configurar ESP32 ───────────────────────────────────────────

def opcao_configurar():
    header("Configurar ESP32")

    port = achar_porta()
    if not port:
        err("Nenhum ESP32 encontrado. Conecte o cabo USB.")
        return

    info(f"ESP32 detectado em: {port}")

    try:
        import serial
    except ImportError:
        err("Biblioteca pyserial necessária. Instale: pip install pyserial")
        return

    s = serial.Serial(port, 115200, timeout=3)
    s.dtr = False
    time.sleep(0.1)
    s.dtr = True
    time.sleep(2)

    # Ler boot
    out = b""
    start = time.time()
    while time.time() - start < 10:
        if s.in_waiting:
            out += s.read(s.in_waiting)
            if b"Digite HELP" in out or b"Ready at" in out:
                break
        time.sleep(0.1)

    txt = out.decode("utf-8", errors="replace")
    ja_configurado = "Ready at" in txt

    if ja_configurado:
        info("ESP32 já está configurado e conectado ao WiFi.")
        print()
        for line in txt.splitlines():
            if "Ready at" in line:
                print(f"  {line.strip()}")
        print()
        r = input(f"  {NEGRITO}Deseja reconfigurar (resetar configuração)? (s/N): {RESET}").strip().lower()
        if r == "s":
            s.write(b"RESET\n")
            time.sleep(1)
            ok("Resetado! Configure novamente.")
            s.close()
            return opcao_configurar()
        s.close()
        return

    if "Modo Setup" not in txt:
        warn("ESP32 não respondeu. Verifique a conexão.")
        s.close()
        return

    # Modo setup — configurar WiFi
    print()
    ssid = input(f"  {NEGRITO}SSID{RESET} da rede WiFi: ").strip()
    if not ssid:
        warn("Cancelado.")
        s.close()
        return

    senha = input(f"  {NEGRITO}Senha{RESET} (Enter se for aberta): ").strip()
    pin = input(f"  {NEGRITO}PIN{RESET} de bloqueio (opcional): ").strip()

    print()
    info("Enviando configurações...")

    s.write(f"WIFI={ssid},{senha}\n".encode())
    time.sleep(0.3)
    if pin:
        s.write(f"PIN={pin}\n".encode())
        time.sleep(0.3)
    s.write(b"SAVE\n")
    time.sleep(1.5)

    resp = b""
    while s.in_waiting:
        resp += s.read(s.in_waiting)
    s.close()

    if b"OK" in resp:
        ok("Configuração salva! ESP32 vai reiniciar.")
        print()
        info("Aguarde ~20s e descubra o IP:")
        info("  python3 " + sys.argv[0])
        info("  (opção 2 de novo para ver o status)")
    else:
        warn("Algo deu errado. Tente novamente.")


# ── Menu principal ────────────────────────────────────────────────

def menu():
    while True:
        os.system("clear" if os.name == "posix" else "cls")
        header("ESPortable32")
        print(f"  {NEGRITO}1{RESET}   Instalar ESPortable32 em um ESP32")
        print(f"  {NEGRITO}2{RESET}   Configurar ESP32")
        print(f"  {NEGRITO}3{RESET}   Sair")
        print()
        try:
            op = input(f"  {NEGRITO}>>>{RESET} ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            break

        if op == "1":
            opcao_instalar()
        elif op == "2":
            opcao_configurar()
        elif op == "3":
            print()
            ok("Até logo!")
            break
        else:
            warn("Opção inválida! Digite 1, 2 ou 3.")
            time.sleep(1)
            continue

        print(f"\n  {NEGRITO}Pressione Enter para voltar ao menu...{RESET}", end="")
        try:
            input()
        except (EOFError, KeyboardInterrupt):
            break


# ── Main ───────────────────────────────────────────────────────────

def main():
    if len(sys.argv) > 1:
        # Modo direto: esportable32 install / config
        acao = sys.argv[1]
        if acao == "install":
            opcao_instalar()
        elif acao == "config":
            opcao_configurar()
        else:
            print(f"Uso: {sys.argv[0]} [install|config]")
            print(f"     {sys.argv[0]}           (menu interativo)")
        return

    menu()


if __name__ == "__main__":
    main()
