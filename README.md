# ESPortable32

**Servidor portátil no ESP32 com API REST e interfaces desktop/terminal.**

Firmware para ESP32 que expõe uma API REST completa (GPIO, sistema de arquivos, apps, proxy, WiFi)
acessível via TCP/IP ou Serial. Acompanha duas interfaces para o usuário:

| Interface | Comando de instalação |
|---|---|
| **GUI** (AppImage) | `bash <(curl -sL https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/install-gui.sh)` |
| **TUI** (terminal) | `bash <(curl -sL https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/install-tui.sh)` |

---

## Interface Gráfica (GUI)

Instala o AppImage + atalho no menu de aplicativos:

```bash
bash <(curl -sL https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/install-gui.sh)
```

Depois de instalado, procure por **ESPortable32** no menu ou execute:

```bash
~/.local/bin/ESPortable32.AppImage
```

A GUI oferece 6 abas: Dashboard, GPIO, Editor de Arquivos, Configurações, Loja de Apps e Terminal.

## Interface de Terminal (TUI)

Instala o comando `esportableui` no sistema:

```bash
bash <(curl -sL https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/install-tui.sh)
```

Depois de instalado:

```bash
esportableui
```

Navegue entre as abas com as teclas **d** (Dashboard), **g** (GPIO), **e** (Editor), **c** (Config), **s** (Store), **t** (Terminal). Pressione **q** para desconectar.

---

## Instalação do Firmware no ESP32

```bash
bash <(curl -sL https://raw.githubusercontent.com/victorbillyph/ESPortable32/main/install.sh)
esportable32 install
```

Conecte o ESP32 via USB antes de executar.

---

## API REST

O ESP32 expõe os seguintes endpoints:

| Rota | Método | Descrição |
|---|---|---|
| `/api/status` | GET | Status completo do dispositivo |
| `/api/unlock` | POST | Desbloquear com PIN |
| `/api/restart` | POST | Reiniciar o ESP32 |
| `/api/gpio` | GET/POST | Listar/controlar pinos GPIO |
| `/api/fs/list?path=` | GET | Listar arquivos no LittleFS |
| `/api/fs/read?path=` | GET | Ler conteúdo de arquivo |
| `/api/fs/write` | POST | Escrever arquivo |
| `/api/fs/delete?path=` | DELETE | Deletar arquivo |
| `/api/apps` | GET/POST | Listar/instalar apps |
| `/api/proxy` | POST | Proxy HTTP |
| `/api/wifi/scan` | GET | Escanear redes WiFi |
| `/api/setup` | POST | Configurar WiFi/PIN |

---

## Estrutura do Projeto

```
ESPortable32/
├── esportable32.py              # CLI principal (instalar/configurar/reparar)
├── esportable32_gui.py          # Interface gráfica (tkinter)
├── esportable32_tui.py          # Interface de terminal (Textual)
├── install.sh                   # Instalador da CLI
├── install-tui.sh               # Instalador do TUI
├── install-gui.sh               # Instalador do GUI (AppImage)
├── build-appimage.sh            # Construtor de AppImage
├── ESPortable32.desktop         # Atalho de menu
├── esportable32.svg             # Ícone
├── platformio.ini               # Configuração PlatformIO
├── src/                         # Firmware C++ (ESP32)
│   ├── esportable32.ino         # Setup/loop + comandos serial JSON
│   ├── StateManager.h/.cpp      # Máquina de estados
│   ├── ConfigManager.h/.cpp     # Config JSON no LittleFS
│   ├── BluetoothService.h/.cpp  # Bluetooth Serial
│   ├── WebServer.h/.cpp         # Servidor HTTP + API REST
│   └── WebSockets.h/.cpp        # WebSocket
├── apps/                        # Catálogo de apps para a loja
└── data/                        # LittleFS (arquivos estáticos)
```

---

## Licença

MIT
