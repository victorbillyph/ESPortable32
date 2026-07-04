# ESPortable32

**Transforme seu ESP32 em um servidor portátil com interface desktop Windows 98.**

Firmware para ESP32 com API REST completa, mais uma ferramenta TUI tudo-em-um para instalar, configurar e controlar o dispositivo — com visual retro Win95/Win98.

---

## Instalação Rápida

```bash
bash <(curl -sL https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/install.sh)
```

Requer Python 3, git e PlatformIO (instalado automaticamente).

Após instalar:

```bash
esportable32
```

O comando abre a interface de terminal com sequência de boot (BIOS POST), scan automático do ESP32 e desktop com 6 aplicativos.

---

## Funcionalidades

### Tudo-em-um

| Funcionalidade | Descrição |
|---|---|
| **BIOS POST** | Sequência de boot estilo BIOS (verde/preta) ao iniciar |
| **Scan automático** | Detecta o ESP32 em portas seriais (ACM/USB primeiro) |
| **Desktop Win98** | Ambiente gráfico no terminal com ícones, taskbar, menu iniciar |
| **Dashboard** | Status do ESP32 em tempo real (heap, RSSI, CPU, IP) |
| **GPIO** | Controle individual de pinos (liga/desliga) |
| **Editor** | Navegar, criar, editar e deletar arquivos no LittleFS |
| **Config** | Configurar WiFi, PIN, nome do dispositivo |
| **Loja** | Buscar e instalar apps do repositório |
| **Terminal** | Comandos diretos via serial ou TCP |
| **Setup Wizard** | Assistente passo-a-passo para primeira configuração |
| **Instalar firmware** | Compila e grava o firmware no ESP32 via PlatformIO |
| **Reparo** | Diagnóstico de conexão e reparo automático |
| **Auto-update** | Verifica atualizações no GitHub automaticamente |

### Conexão

O ESP32 pode ser acessado via:
- **Serial** — USB (auto-detectado)
- **TCP/IP** — IP da rede WiFi

---

## API REST do ESP32

| Rota | Método | Descrição |
|---|---|---|
| `/api/status` | GET | Status completo |
| `/api/unlock` | POST | Desbloquear com PIN |
| `/api/restart` | POST | Reiniciar |
| `/api/gpio` | GET/POST | Listar/controlar GPIO |
| `/api/fs/list` | GET | Listar arquivos |
| `/api/fs/read` | GET | Ler arquivo |
| `/api/fs/write` | POST | Escrever arquivo |
| `/api/fs/delete` | DELETE | Deletar arquivo |
| `/api/apps` | GET/POST | Listar/instalar apps |
| `/api/proxy` | POST | Proxy HTTP |
| `/api/wifi/scan` | GET | Escanear WiFi |
| `/api/setup` | POST | Salvar WiFi/PIN |

### Serial

Comandos via monitor serial (115200 baud):

```
STATUS
WIFI=ssid,pass
PIN=1234
NAME=meu-esp
SAVE
RESET
HELP
```

---

## Estrutura do Repositório

```
ESPortable32/
├── esportable32_tui.py          # ⭐ Ferramenta TUI tudo-em-um
├── esportable32.py              # CLI auxiliar (instalar/reparar)
├── esportable32_gui.py          # Interface gráfica legada (tkinter)
├── install.sh                   # Instalador oficial
├── platformio.ini               # Config PlatformIO
├── src/                         # Firmware C++ do ESP32
│   ├── esportable32.ino         # Setup/loop + serial JSON
│   ├── StateManager.h/.cpp      # Máquina de estados
│   ├── ConfigManager.h/.cpp     # Config JSON (LittleFS)
│   ├── BluetoothService.h/.cpp  # Bluetooth Serial (SPP)
│   ├── WebServer.h/.cpp         # Servidor HTTP + API REST
│   └── WebSockets.h/.cpp        # WebSocket
└── apps/                        # Catálogo de apps
```

---

## Stack

| Componente | Tecnologia |
|---|---|
| **Microcontrolador** | ESP32 (ESP-WROOM-32) |
| **Framework** | Arduino (platformio/espressif32) |
| **Servidor HTTP** | ESPAsyncWebServer |
| **Serialização** | ArduinoJson 7 |
| **Sistema de Arquivos** | LittleFS |
| **Interface TUI** | Python 3 + Textual |
| **Firmware Tooling** | PlatformIO + esptool |

---

## Licença

MIT — use, modifique e distribua livremente.
