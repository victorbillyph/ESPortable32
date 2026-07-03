# ESPortable32

**Transforme seu ESP32 em um servidor portátil com API REST completa, acessível via TCP/IP ou Serial.**

O ESPortable32 é um firmware para ESP32 que expõe uma API REST completa (GPIO, sistema de arquivos, apps, proxy, scan WiFi, configuração) mais uma interface de linha de comando para instalação/configuração. Acompanha duas interfaces de usuário:

| Interface | Descrição | Instalação |
|---|---|---|
| **GUI** | Interface gráfica desktop (AppImage) | `bash <(curl -sL https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/install-gui.sh)` |
| **TUI** | Interface de terminal (Textual) | `bash <(curl -sL https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/install-tui.sh)` |
| **CLI** | Ferramenta de linha de comando (instalar firmware, configurar, reparar) | `bash <(curl -sL https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/install.sh)` |

---

## 1. Métodos de Instalação

### Interface Gráfica (GUI)

```bash
bash <(curl -sL https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/install-gui.sh)
```

Constrói um AppImage e instala no sistema com atalho no menu de aplicativos. Requer Python 3 e git.

Após instalar, procure por **ESPortable32** no menu ou execute:

```bash
~/.local/bin/ESPortable32.AppImage
```

A GUI oferece 6 abas: **Dashboard**, **GPIO**, **Editor de Arquivos**, **Config**, **Loja de Apps** e **Terminal**.

### Interface de Terminal (TUI)

```bash
bash <(curl -sL https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/install-tui.sh)
```

Instala o comando `esportableui` com ambiente virtual próprio. Requer Python 3 e git.

Após instalar:

```bash
esportableui
```

Atalhos de teclado: **d** Dashboard, **g** GPIO, **e** Editor, **c** Config, **s** Store, **t** Terminal, **q** desconectar.

### Firmware no ESP32

```bash
bash <(curl -sL https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/install.sh)
esportable32 install
```

Conecte o ESP32 via USB antes de executar. A CLI compila o firmware, grava na flash e envia o sistema de arquivos (LittleFS).

Também disponível via a GUI ou TUI após conectar no ESP32 — abra a aba **Config** e ajuste WiFi, PIN e nome do dispositivo.

---

## 2. Comandos

### CLI (`esportable32`)

| Comando | Descrição |
|---|---|
| `esportable32` | Menu interativo |
| `esportable32 install` | Compilar e gravar firmware no ESP32 |
| `esportable32 config` | Configurar WiFi do ESP32 |
| `esportable32 repair` | Diagnosticar e reparar problemas |
| `esportable32 gui` | Abrir interface gráfica |
| `esportable32 flash` | Compilar e gravar (alias) |

### GUI / TUI

Ambas as interfaces oferecem as mesmas funcionalidades organizadas em abas:

- **Dashboard** — Estado do ESP32 em tempo real (heap, RSSI, CPU, uptime, IP)
- **GPIO** — Controle individual de 19 pinos (liga/desliga)
- **Editor** — Navegar, criar, editar e deletar arquivos no LittleFS
- **Config** — Configurar WiFi, PIN de bloqueio, nome do dispositivo, reiniciar
- **Loja** — Buscar e instalar apps do repositório GitHub
- **Terminal** — Enviar comandos diretos (GET/POST para a API REST)

---

## 3. Visão Técnica

### API REST

O ESP32 expõe uma API REST completa via HTTP. Exemplo:

```bash
curl http://192.168.2.38/api/status
```

| Rota | Método | Descrição |
|---|---|---|
| `/api/status` | GET | Status completo do dispositivo |
| `/api/unlock` | POST | Desbloquear com PIN |
| `/api/restart` | POST | Reiniciar o ESP32 |
| `/api/gpio` | GET/POST | Listar/controlar pinos GPIO |
| `/api/fs/list` | GET | Listar arquivos no LittleFS |
| `/api/fs/read` | GET | Ler conteúdo de arquivo |
| `/api/fs/write` | POST | Escrever arquivo no LittleFS |
| `/api/fs/delete` | DELETE | Deletar arquivo |
| `/api/apps` | GET/POST | Listar/instalar apps |
| `/api/proxy` | POST | Proxy HTTP (fetch externo) |
| `/api/wifi/scan` | GET | Escanear redes WiFi disponíveis |
| `/api/setup` | POST | Salvar WiFi/PIN e reiniciar |

### API via Serial

Conecte via monitor serial (115200 baud) e envie comandos JSON:

```json
{"action":"gpio_list"}
{"action":"gpio_set","pin":2,"state":1}
{"action":"fs_list","path":"/"}
{"action":"fs_read","path":"/config.json"}
```

Respostas seguem o formato `{"status":"ok","data":...}`.

### Arquitetura do Firmware

```
                    ┌──────────────┐
                    │  GUI / TUI  │  (Python — desktop ou terminal)
                    └──────┬───────┘
                           │ HTTP / Serial
                    ┌──────▼───────┐
                    │  WebServer   │  ESPAsyncWebServer
                    │  (REST API)  │
                    └──────┬───────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
        ┌─────▼────┐ ┌────▼────┐ ┌────▼────┐
        │ LittleFS │ │  WiFi   │ │  State  │
        │(Arquivos)│ │ AP+STA  │ │ Manager │
        └──────────┘ └─────────┘ └─────────┘
              │                        │
        ┌─────▼────┐             ┌─────▼──────┐
        │  Config  │             │ Bluetooth  │
        │ (JSON)   │             │  Serial    │
        └──────────┘             └────────────┘
```

### Estrutura do Repositório

```
ESPortable32/
├── esportable32.py              # CLI principal
├── esportable32_gui.py          # Interface gráfica (tkinter)
├── esportable32_tui.py          # Interface de terminal (Textual)
├── install.sh                   # Instalador da CLI
├── install-tui.sh               # Instalador do TUI
├── install-gui.sh               # Instalador do GUI (AppImage)
├── build-appimage.sh            # Construtor de AppImage
├── ESPortable32.desktop         # Atalho de menu (.desktop)
├── esportable32.svg             # Ícone do projeto
├── platformio.ini               # Configuração do PlatformIO
├── esportable32_partitions.csv  # Tabela de partições da flash
├── src/                         # Firmware C++ do ESP32
│   ├── esportable32.ino         # Setup/loop + comandos serial JSON
│   ├── StateManager.h/.cpp      # Máquina de estados do dispositivo
│   ├── ConfigManager.h/.cpp     # Configuração em JSON (LittleFS)
│   ├── BluetoothService.h/.cpp  # Comunicação Bluetooth Serial (SPP)
│   ├── WebServer.h/.cpp         # Servidor HTTP + API REST
│   └── WebSockets.h/.cpp        # WebSocket para tempo real
├── apps/                        # Catálogo de apps (manifest + HTML)
└── data/                        # Conteúdo do LittleFS
```

### Stack

| Componente | Tecnologia |
|---|---|
| **Microcontrolador** | ESP32 (ESP-WROOM-32) |
| **Framework** | Arduino (platformio/espressif32) |
| **Servidor HTTP** | ESPAsyncWebServer |
| **Serialização** | ArduinoJson 7 |
| **Sistema de Arquivos** | LittleFS |
| **Interface Desktop** | Python 3 + tkinter + requests |
| **Interface Terminal** | Python 3 + Textual |
| **Comunicação Serial** | pyserial |

---

## 4. Licença e Créditos

**Licença:** MIT — use, modifique e distribua livremente.

**Créditos:**
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) — Servidor HTTP/WebSocket assíncrono
- [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) — TCP assíncrono para ESP32
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) — Biblioteca JSON para Arduino
- [Textual](https://github.com/Textualize/textual) — Framework TUI para Python
- [PlatformIO](https://platformio.org) — Ecossistema de desenvolvimento embarcado

---

<p align="center">
  <sub>Feito com ☕ e ESP32</sub>
</p>
