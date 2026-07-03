#!/usr/bin/env python3
"""ESPortable32 CLI - Build, flash, and monitor tool."""

import argparse
import os
import subprocess
import sys
import time

PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR = os.path.join(PROJECT_DIR, ".pio", "build", "esportable32")
DATA_DIR = os.path.join(PROJECT_DIR, "data")
SKETCH_DIR = os.path.join(PROJECT_DIR, "esportable32")
PLATFORMIO_INI = os.path.join(PROJECT_DIR, "platformio.ini")


def print_header(title):
    print(f"\n  {'─' * 50}")
    print(f"  {title}")
    print(f"  {'─' * 50}\n")


def check_dependencies():
    """Check that pio is available."""
    # Prefer venv path if it exists
    venv_pio = "/tmp/pio-venv/bin/pio"
    if os.path.exists(venv_pio):
        return venv_pio
    try:
        subprocess.run(["pio", "--version"], capture_output=True, check=True)
        return "pio"
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass
    try:
        subprocess.run(["platformio", "--version"], capture_output=True, check=True)
        return "platformio"
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("[!] PlatformIO Core not found. Install it:")
        print("    pip install platformio")
        print("    or https://platformio.org/install")
        sys.exit(1)


def find_esptool():
    """Find esptool.py."""
    venv_esptool = "/tmp/pio-venv/bin/esptool.py"
    if os.path.exists(venv_esptool):
        return venv_esptool
    return "esptool.py"


def cmd_build(pio):
    print_header("Compilando firmware")
    result = subprocess.run(
        [pio, "run", "--project-dir", PROJECT_DIR],
        capture_output=True, text=True
    )
    print(result.stdout)
    if result.returncode != 0:
        print(result.stderr)
        print("[!] Compilação falhou!")
        sys.exit(1)
    print("[OK] Compilação concluída!")


def cmd_flash(pio):
    cmd_build(pio)

    # Find the firmware binaries (PlatformIO puts them in .pio/build/<env>/)
    firmware = os.path.join(BUILD_DIR, "firmware.bin")
    bootloader = os.path.join(BUILD_DIR, "bootloader.bin")
    partitions = os.path.join(BUILD_DIR, "partitions.bin")
    littlefs_img = os.path.join(BUILD_DIR, "littlefs.bin")

    port = find_serial_port()
    if not port:
        print("[!] Nenhuma porta serial encontrada!")
        sys.exit(1)

    print_header(f"Gravando no ESP32 ({port})")
    esptool = find_esptool()

    # Erase flash first
    print("  Apagando flash...")
    subprocess.run(
        [esptool, "--port", port, "erase_flash"],
        capture_output=False
    )
    print("  [OK] Flash apagado")

    # Write firmware
    addr_firmware = "0x10000"
    addr_bootloader = "0x1000"
    addr_partitions = "0x8000"
    addr_littlefs = "0x2E0000"

    files_to_write = []
    if os.path.exists(bootloader):
        files_to_write.extend([addr_bootloader, bootloader])
    if os.path.exists(partitions):
        files_to_write.extend([addr_partitions, partitions])
    if os.path.exists(firmware):
        files_to_write.extend([addr_firmware, firmware])
    if os.path.exists(littlefs_img):
        files_to_write.extend([addr_littlefs, littlefs_img])

    if not files_to_write:
        print("[!] Nenhum arquivo de firmware encontrado!")
        print(f"    Procurei em: {fw_dir}")
        sys.exit(1)

    cmd = [esptool, "--port", port, "--baud", "921600", "write_flash"] + files_to_write
    print("  Gravando...")
    subprocess.run(cmd, capture_output=False)
    print("  [OK] Gravação concluída!")
    print("\n  Aguarde ~15s para o ESP32 reiniciar")


def cmd_monitor(pio):
    print_header("Monitor Serial (Ctrl+C para sair)")
    subprocess.run([pio, "device", "monitor", "--project-dir", PROJECT_DIR])


def cmd_build_fs(pio):
    """Build LittleFS image."""
    print_header("Criando LittleFS")
    result = subprocess.run(
        [pio, "run", "--project-dir", PROJECT_DIR, "--target", "buildfs"],
        capture_output=True, text=True
    )
    print(result.stdout)
    if result.returncode != 0:
        print(result.stderr)
        print("[!] Falha ao criar LittleFS!")
        sys.exit(1)
    print("[OK] LittleFS criado")


def cmd_upload_fs(pio):
    """Upload LittleFS image."""
    cmd_build_fs(pio)
    print_header("Enviando LittleFS")
    result = subprocess.run(
        [pio, "run", "--project-dir", PROJECT_DIR, "--target", "uploadfs"],
        capture_output=True, text=True
    )
    print(result.stdout)
    if result.returncode != 0:
        print(result.stderr)
        print("[!] Falha ao enviar LittleFS!")
        sys.exit(1)
    print("[OK] LittleFS enviado!")


def cmd_all(pio):
    """Build, flash, and monitor."""
    cmd_flash(pio)
    print("\n  Iniciando monitor em 5s...")
    time.sleep(5)
    cmd_monitor(pio)


def find_serial_port():
    """Find the ESP32 serial port."""
    import glob
    candidates = glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*")
    if candidates:
        return candidates[0]
    return None


def main():
    parser = argparse.ArgumentParser(description="ESPortable32 CLI")
    parser.add_argument("action", nargs="?", default="all",
                        choices=["build", "flash", "monitor", "all", "buildfs", "uploadfs"],
                        help="Ação a executar")
    args = parser.parse_args()

    pio = check_dependencies()

    actions = {
        "build": lambda: cmd_build(pio),
        "flash": lambda: cmd_flash(pio),
        "monitor": lambda: cmd_monitor(pio),
        "all": lambda: cmd_all(pio),
        "buildfs": lambda: cmd_build_fs(pio),
        "uploadfs": lambda: cmd_upload_fs(pio),
    }

    actions[args.action]()


if __name__ == "__main__":
    main()
