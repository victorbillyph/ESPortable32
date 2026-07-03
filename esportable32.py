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

    # 1. Compilar firmware
    info("Compilando firmware...")
    r = executar([pio, "run", "--project-dir", PROJECT_DIR], capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stderr)
        err("Falha na compilação!")
        sys.exit(1)
    ok("Firmware compilado")

    # 2. Compilar filesystem (LittleFS)
    data_files = [f for f in os.listdir(os.path.join(PROJECT_DIR, "data")) if f != ".gitkeep"]
    if data_files:
        info("Compilando sistema de arquivos (LittleFS)...")
        r = executar([pio, "run", "--project-dir", PROJECT_DIR, "--target", "buildfs"], capture_output=True, text=True)
        if r.returncode != 0:
            warn("Falha ao compilar LittleFS. Verifique se a pasta data/ existe e tem arquivos.")
            print(r.stderr)
        else:
            ok("LittleFS compilado")
    else:
        info("LittleFS vazio (sem arquivos estáticos) — pode ser preenchido via API posteriormente.")

    # 3. Apagar flash (formatação completa)
    info("Apagando flash (formatação completa)...")
    esptool = achar_esptool()
    executar([esptool, "--port", port, "--baud", "921600", "erase_flash"],
             capture_output=False)
    ok("Flash apagado")

    # 4. Gravar firmware + bootloader + partitions + LittleFS
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


# ── 3: Reparação ──────────────────────────────────────────────────

def opcao_reparar():
    header("Reparação - Diagnóstico e reparo")
    port = achar_porta()

    if not port:
        err("Nenhum ESP32 encontrado. Conecte o cabo USB.")
        info("Se o ESP32 estiver conectado, tente:")
        info("  - Verificar o cabo USB")
        info("  - Instalar regras udev: sudo usermod -aG dialout $USER")
        info("  - Verificar se outra porta não está em uso (screen, miniterm)")
        return

    info(f"ESP32 detectado em: {port}")
    print()

    # Ler serial do ESP32
    problemas = []
    try:
        import serial
    except ImportError:
        err("pyserial não instalado. pip install pyserial")
        return

    s = serial.Serial(port, 115200, timeout=3)
    s.dtr = False
    time.sleep(0.1)
    s.dtr = True
    time.sleep(2)

    log = b""
    start = time.time()
    while time.time() - start < 8:
        if s.in_waiting:
            log += s.read(s.in_waiting)
        time.sleep(0.1)

    txt = log.decode("utf-8", errors="replace")

    # Análise
    info("Analisando logs do ESP32...")
    print()

    # Verificar se respondeu
    if not txt.strip():
        problemas.append(("sem_comunicacao", "ESP32 não está respondendo"))
    else:
        ok("ESP32 está respondendo via serial")

        # Verificar modo
        if "Modo Setup" in txt:
            warn("ESP32 está em MODO SETUP (sem config de WiFi)")
            problemas.append(("modo_setup", "WiFi não configurado"))
        elif "Ready at" in txt:
            ok("ESP32 está configurado e conectado ao WiFi")
            for line in txt.splitlines():
                if "Ready at" in line:
                    print(f"     {line.strip()}")
        else:
            warn("Estado desconhecido")

        # LittleFS
        if "Mount" in txt and "Failed" in txt:
            problemas.append(("littlefs", "LittleFS não montou"))
            err("LittleFS com problemas de montagem")
        elif "Storage" in txt and "Mounted" in txt:
            ok("LittleFS montado")
            # Verificar se tem arquivos
            for line in txt.splitlines():
                if "Used: 0" in line:
                    warn("LittleFS está vazio! Nenhum arquivo web encontrado.")
                    problemas.append(("littlefs_vazio", "LittleFS vazio (sem arquivos web)"))
                    break
                elif "Used:" in line:
                    try:
                        used = int(line.split("Used:")[1].strip().rstrip(','))
                        if used < 100:
                            warn(f"LittleFS com poucos arquivos: {used} bytes")
                            problemas.append(("littlefs_vazio", "LittleFS quase vazio"))
                        else:
                            ok(f"LittleFS com {used} bytes de arquivos")
                    except ValueError:
                        pass

        # Core dump / panic
        if "panic" in txt.lower() or "Guru Meditation" in txt or "abort()" in txt:
            problemas.append(("crash", "ESP32 está dando crash (panic/abort)"))
            err("Crash detectado!")

        if "rst:" in txt and "SW_CPU_RESET" in txt:
            for line in txt.splitlines():
                if "rst:" in line and "SW_CPU_RESET" in line:
                    pass  # Reset normal após SAVE
            # Contar resets
            resets = txt.count("rst:0x")
            if resets > 3:
                problemas.append(("bootloop", "ESP32 em boot loop (muitos resets)"))
                err("Boot loop detectado!")

        # Config
        if "No config file found" in txt:
            problemas.append(("sem_config", "Nenhuma configuração salva"))
            warn("Nenhuma configuração encontrada")

        if "WiFi] Failed to connect" in txt:
            problemas.append(("wifi_falhou", "Falha ao conectar no WiFi"))
            err("Conexão WiFi falhou")

        # Memória
        for line in txt.splitlines():
            if "heap" in line.lower() or "Heap" in line:
                try:
                    h = int(line.split("Heap:")[1].split(",")[0].strip())
                    if h < 10000:
                        problemas.append(("heap_baixo", f"Heap baixo: {h} bytes"))
                        warn(f"Heap baixo: {h} bytes")
                    else:
                        ok(f"Heap: {h} bytes livres")
                except (IndexError, ValueError):
                    pass

    s.close()
    print()

    # Se sem comunicação, tenta coisas básicas
    if "sem_comunicacao" in [p[0] for p in problemas]:
        err("Não foi possível comunicar com o ESP32.")
        print()
        info("Tentando reparos básicos...")
        print()

        # Testar se o chip responde via esptool
        esptool = achar_esptool()
        if esptool:
            info("Verificando conexão via esptool...")
            r = executar([esptool, "--port", port, "chip_id"], capture_output=True)
            if r.returncode == 0:
                ok("ESP32 detectado pelo esptool!")
                info("O firmware pode estar corrompido ou em loop.")
                r = input(f"\n  {NEGRITO}Deseja reinstalar o firmware? (s/N): {RESET}").strip().lower()
                if r == "s":
                    opcao_instalar()
                    return
            else:
                warn("ESP32 não responde pelo esptool.")
                info("Verifique: cabo USB, driver, outra porta.")
        else:
            warn("esptool não encontrado.")

        print()
        info("Sugestões:")
        info("  1. Desconecte e reconecte o USB")
        info("  2. Segure o boot do ESP32 enquanto conecta")
        info("  3. Tente: python3 esportable32.py install")
        return

    # Diagnóstico
    if not problemas:
        ok("Nenhum problema encontrado!")
        return

    print(f"  {NEGRITO}Problemas encontrados:{RESET}")
    for i, (_, desc) in enumerate(problemas, 1):
        print(f"    {i}. {VERMELHO}{desc}{RESET}")
    print()

    # Reparos
    for cod, desc in problemas:
        if cod == "modo_setup" or cod == "sem_config" or cod == "wifi_falhou":
            info(f"Problema: {desc}")
            print()
            info("Use a interface gráfica ou de terminal para configurar:")
            info("  esportable32 gui      (interface gráfica)")
            info("  esportableui          (interface de terminal)")
            r = input(f"\n  {NEGRITO}Abrir GUI agora? (s/N): {RESET}").strip().lower()
            if r == "s":
                abrir_gui()
            return

        elif cod == "bootloop" or cod == "crash":
            info(f"Problema: {desc}")
            r = input(f"  {NEGRITO}Resolver: reinstalar firmware do zero? (s/N): {RESET}").strip().lower()
            if r == "s":
                opcao_instalar()
                return

        elif cod == "littlefs" or cod == "littlefs_vazio":
            info(f"Problema: {desc}")
            r = input(f"  {NEGRITO}Resolver: reenviar LittleFS (arquivos web)? (s/N): {RESET}").strip().lower()
            if r == "s":
                pio = achar_pio()
                if pio:
                    executar([pio, "run", "--project-dir", PROJECT_DIR, "--target", "uploadfs"])
                return

        elif cod == "heap_baixo":
            info(f"Problema: {desc}")
            info("Sugestão: reinicie o ESP32 (desconecte USB e reconecte)")
            r = input(f"  {NEGRITO}Tentar reparo automático (reinstalar)? (s/N): {RESET}").strip().lower()
            if r == "s":
                opcao_instalar()
                return

    print()
    ok("Processo de reparo concluído.")
    info("Se os problemas persistirem, tente:")
    info("  python3 esportable32.py install   (instalação limpa)")


# ── Abrir GUI ─────────────────────────────────────────────────────

def abrir_gui():
    gui_path = os.path.join(PROJECT_DIR, "esportable32_gui.py")
    if not os.path.exists(gui_path):
        err("GUI não encontrada: esportable32_gui.py")
        return
    info("Abrindo interface gráfica...")
    try:
        subprocess.Popen([sys.executable, gui_path])
    except Exception as e:
        err(f"Erro ao abrir GUI: {e}")


# ── Menu principal ────────────────────────────────────────────────

def menu():
    while True:
        os.system("clear" if os.name == "posix" else "cls")
        header("ESPortable32")
        print(f"  {NEGRITO}1{RESET}   Instalar/Reinstalar firmware no ESP32")
        print(f"  {NEGRITO}2{RESET}   Reparação (diagnóstico + reparo)")
        print(f"  {NEGRITO}3{RESET}   Abrir GUI (app desktop)")
        print(f"  {NEGRITO}0{RESET}   Sair")
        print()
        try:
            op = input(f"  {NEGRITO}>>>{RESET} ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            break

        if op == "1":
            opcao_instalar()
        elif op == "2":
            opcao_reparar()
        elif op == "3":
            abrir_gui()
        elif op == "0":
            print()
            ok("Até logo!")
            break
        else:
            warn("Opção inválida! Digite 1, 2, 3 ou 0.")
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
        # Modo direto: esportable32 install / repair / gui
        acao = sys.argv[1]
        if acao == "install":
            opcao_instalar()
        elif acao == "repair":
            opcao_reparar()
        elif acao == "gui":
            abrir_gui()
        else:
            print(f"Uso: {sys.argv[0]} [install|repair|gui]")
            print(f"     {sys.argv[0]}           (menu interativo)")
        return

    menu()


if __name__ == "__main__":
    main()
