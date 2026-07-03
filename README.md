# ESPortable32

**Servidor web portátil no ESP32 com interface desktop, API REST, WebSocket e Bluetooth.**

Um firmware completo para ESP32 que transforma o microcontrolador em um servidor web acessível via WiFi, com uma interface inspirada em sistemas desktop, suporte a apps, bloqueio por PIN, configuração via serial/Bluetooth, e muito mais.

![License](https://img.shields.io/badge/license-MIT-blue)
![Platform](https://img.shields.io/badge/platform-ESP32-red)
![Framework](https://img.shields.io/badge/framework-Arduino-00979D)

---

## ✨ Funcionalidades

| Funcionalidade | Detalhes |
|---|---|
| **Servidor Web** | Serve arquivos HTML/CSS/JS do LittleFS via ESPAsyncWebServer |
| **Interface Desktop** | Ambiente gráfico no navegador com janelas, barra de tarefas e menu iniciar |
| **API REST** | Endpoints JSON para status, desbloqueio e controle |
| **WebSocket** | Comunicação em tempo real com o navegador |
| **WiFi AP+STA** | Modo Access Point para configuração inicial + Station para WiFi normal |
| **Bluetooth Serial** | Configuração via Bluetooth (SPP) |
| **Setup por USB** | Modo setup com comandos via serial |
| **Sistema de Apps** | Apps em iframes com SDK de comunicação com o desktop |
| **Bloqueio por PIN** | Tela de bloqueio para proteger o acesso |
| **LittleFS** | Sistema de arquivos na flash para armazenar páginas web |

---

## 🚀 Começando

### 1. Instalar dependências

```bash
python3 esportable32.py install
```

Isso cria um ambiente virtual e instala **PlatformIO** e **esptool**.

### 2. Conectar o ESP32

Conecte o ESP32 via USB. Para acesso sem sudo, instale as regras udev:

```bash
curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/master/scripts/99-platformio-udev.rules | sudo tee /etc/udev/rules.d/99-platformio-udev.rules
sudo udevadm control --reload-rules
sudo usermod -aG dialout $USER
# Faça logout e login novamente
```

### 3. Compilar e gravar

```bash
python3 esportable32.py flash
```

Isso compila o firmware, cria o sistema de arquivos LittleFS e grava tudo no ESP32.

### 4. Configurar WiFi

Após o primeiro boot, o ESP32 entra em **modo setup** com uma rede WiFi própria:

```
SSID:   ESPortable32-Setup
Senha:  configurar
```

Conecte-se a essa rede e acesse http://192.168.4.1 para configurar via navegador, ou use o serial:

```bash
python3 esportable32.py config
```

### 5. Acessar

Após configurado, o ESP32 se conecta à sua rede WiFi. Descubra o IP:

```bash
python3 esportable32.py status
```

E acesse `http://<ip>/` no navegador.

---

## 🛠️ Comandos da ferramenta

```
python3 esportable32.py              Menu interativo
python3 esportable32.py install      Instalar dependências
python3 esportable32.py build        Compilar firmware
python3 esportable32.py flash        Compilar + gravar no ESP32
python3 esportable32.py monitor      Monitor serial
python3 esportable32.py uploadfs     Atualizar arquivos web (LittleFS)
python3 esportable32.py config       Configurar WiFi via serial
python3 esportable32.py status       Status do ESP32
```

### Modo interativo

```
python3 esportable32.py
```

```
┌──────────────────────────────────────────────────┐
│           ESPortable32 - Menu Principal           │
├──────────────────────────────────────────────────┤
│  1   Compilar firmware                           │
│  2   Gravar no ESP32 (flash + littlefs)           │
│  3   Monitor serial                              │
│  4   Upload LittleFS (arquivos web)               │
│  5   Configurar WiFi via serial                  │
│  6   Status do ESP32                             │
│  7   Instalar dependências                       │
│  0   Sair                                        │
└──────────────────────────────────────────────────┘
```

---

## 📁 Estrutura do projeto

```
ESPortable32/
├── esportable32.py              # Ferramenta CLI principal
├── platformio.ini               # Configuração do PlatformIO
├── esportable32_partitions.csv  # Tabela de partições da flash
├── src/                         # Código fonte do firmware
│   ├── esportable32.ino         # Entry point (setup/loop)
│   ├── StateManager.h/.cpp      # Máquina de estados
│   ├── ConfigManager.h/.cpp     # Configuração em JSON (LittleFS)
│   ├── BluetoothService.h/.cpp  # Bluetooth Serial
│   ├── WebServer.h/.cpp         # Servidor HTTP + API REST
│   └── WebSockets.h/.cpp        # WebSocket handler
├── data/                        # Arquivos do LittleFS (web UI)
│   ├── index.html               # Página principal (desktop)
│   ├── unlock.html              # Tela de bloqueio
│   ├── setup.html               # Página de configuração
│   ├── css/
│   │   ├── desktop.css          # Estilo do desktop
│   │   └── setup.css            # Estilo da página de setup
│   ├── js/
│   │   ├── desktop.js           # Lógica do desktop (janelas, menu)
│   │   ├── sdk.js               # SDK para comunicação com apps
│   │   ├── api.js               # Cliente da API REST
│   │   ├── apps.js              # Gerenciador de apps
│   │   ├── websocket.js         # Cliente WebSocket
│   │   └── lib/vanilla.js       # Utilitários DOM
│   └── apps/
│       ├── store.html           # Loja de apps
│       ├── editor.html          # Editor de texto
│       └── proxy.html           # Proxy HTTP
└── .gitignore
```

---

## 🔌 API REST

| Rota | Método | Descrição |
|---|---|---|
| `/api/status` | GET | Status completo do dispositivo |
| `/api/unlock` | POST | Desbloquear com PIN |
| `/api/restart` | POST | Reiniciar o ESP32 |

### Exemplo

```bash
curl http://192.168.2.38/api/status
```

Resposta:

```json
{
  "status": "ok",
  "state": 4,
  "state_name": "READY",
  "locked": false,
  "uptime": 120,
  "free_heap": 52824,
  "wifi_rssi": -60,
  "ip": "192.168.2.38",
  "ssid": "MinhaRede",
  "version": "1.0.0"
}
```

---

## 📡 Comandos Serial

Conecte via monitor serial (`python3 esportable32.py monitor`) e digite:

| Comando | Descrição |
|---|---|
| `HELP` | Lista todos os comandos |
| `STATUS` | Status do sistema |
| `WIFI=ssid,pass` | Configurar WiFi |
| `NAME=nome` | Nome do dispositivo |
| `PIN=1234` | Configurar PIN de bloqueio |
| `SAVE` | Salvar configuração e reiniciar |
| `RESET` | Reset de fábrica |
| `BT=on\|off` | Ligar/desligar Bluetooth |

---

## 🔧 Patch na biblioteca ESPAsyncWebServer

O projeto inclui um patch crítico na biblioteca `ESPAsyncWebServer`:

**Problema:** A função `_ack()` usava `malloc(outLen + headLen)` para alocar um buffer de resposta, que falhava quando o heap do ESP32 estava fragmentado, causando crash.

**Solução:** Substituímos o `malloc()` por um buffer fixo de 512 bytes na stack (`uint8_t buf[512]`), eliminando completamente a alocação dinâmica no path de resposta.

O patch está em `.pio/libdeps/esportable32/ESPAsyncWebServer/src/WebResponses.cpp`.

---

## 🧱 Arquitetura

```
                    ┌──────────────┐
                    │   Browser    │
                    │ (Desktop UI) │
                    └──────┬───────┘
                           │ HTTP / WS
                    ┌──────▼───────┐
                    │  WebServer   │
                    │  (REST+WS)   │
                    └──────┬───────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
        ┌─────▼────┐ ┌────▼────┐ ┌────▼────┐
        │LittleFS  │ │  WiFi   │ │  State  │
        │(Arquivos)│ │ AP+STA  │ │ Manager │
        └──────────┘ └─────────┘ └─────────┘
              │                        │
        ┌─────▼────┐             ┌─────▼──────┐
        │  Config  │             │ Bluetooth  │
        │ (JSON)   │             │  Serial    │
        └──────────┘             └────────────┘
```

---

## 📦 Dependências

| Biblioteca | Versão | Função |
|---|---|---|
| [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) | 3.6.0+ | Servidor HTTP/WebSocket assíncrono |
| [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) | 3.3.2+ | TCP assíncrono para ESP32 |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | 7.x | Leitura/escrita de JSON |

---

## 📄 Licença

MIT
