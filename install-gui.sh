#!/usr/bin/env bash
set -euo pipefail

REPO_URL="https://github.com/victorbillyph/ESPortable32"
INSTALL_DIR="${HOME}/.local/share/esportable32"
BIN_DIR="${HOME}/.local/bin"
APPIMAGE_DEST="${BIN_DIR}/ESPortable32.AppImage"

GREEN='\033[92m'
CYAN='\033[96m'
YELLOW='\033[93m'
RED='\033[91m'
BOLD='\033[1m'
RESET='\033[0m'

step()  { echo -e "  ${CYAN}→${RESET} $1"; }
ok()    { echo -e "  ${GREEN}✓${RESET} $1"; }
warn()  { echo -e "  ${YELLOW}⚠${RESET} $1"; }
err()   { echo -e "  ${RED}✗${RESET} $1"; }

echo ""
echo -e "  ${BOLD}${CYAN}──────────────────────────────────────────${RESET}"
echo -e "  ${BOLD}ESPortable32 GUI - Instalação${RESET}"
echo -e "  ${BOLD}${CYAN}──────────────────────────────────────────${RESET}"
echo ""
echo -e "  Instala a interface gráfica como AppImage + atalho no menu"
echo ""

# 1. Verificar dependências
step "Verificando dependências..."
for cmd in python3 git wget curl; do
    if ! command -v "$cmd" &>/dev/null; then
        err "$cmd não encontrado. Instale com: sudo apt install $cmd"
        exit 1
    fi
done
ok "Python 3, git, wget/curl"

# 2. Clonar / atualizar repositório
if [ -d "${INSTALL_DIR}/.git" ]; then
    step "Atualizando repositório..."
    git -C "${INSTALL_DIR}" pull --ff-only 2>/dev/null || warn "Não foi possível atualizar"
    ok "Repositório atualizado"
else
    step "Clonando repositório..."
    mkdir -p "${HOME}/.local/share"
    git clone --depth 1 "${REPO_URL}" "${INSTALL_DIR}"
    ok "Repositório clonado em ${INSTALL_DIR}"
fi

# 3. Construir AppImage
step "Construindo AppImage (pode levar alguns minutos)..."
cd "${INSTALL_DIR}"
bash build-appimage.sh
ok "AppImage construída"

# 4. Instalar AppImage
step "Instalando AppImage em ${BIN_DIR}..."
mkdir -p "${BIN_DIR}"
# Encontra o AppImage gerado
APPIMAGE_FILE=$(ls -t "${INSTALL_DIR}"/ESPortable32-*.AppImage 2>/dev/null | head -1)
if [ -z "${APPIMAGE_FILE}" ]; then
    warn "AppImage não encontrado na pasta do projeto"
    warn "Procure manualmente ou execute: bash build-appimage.sh"
    exit 0
fi
mv "${APPIMAGE_FILE}" "${APPIMAGE_DEST}"
chmod +x "${APPIMAGE_DEST}"
ok "AppImage instalado: ${APPIMAGE_DEST}"

# 5. Instalar desktop entry
step "Instalando atalho no menu de aplicativos..."
mkdir -p "${HOME}/.local/share/applications"
mkdir -p "${HOME}/.local/share/icons/hicolor/256x256/apps"
mkdir -p "${HOME}/.local/share/icons/hicolor/scalable/apps"

# Gerar icone PNG
python3 -c "
import struct, zlib
def create_png(width, height, pixels):
    def chunk(chunk_type, data):
        c = chunk_type + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
    header = b'\x89PNG\r\n\x1a\n'
    ihdr = chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
    raw = b''
    for row in pixels:
        raw += b'\x00' + row
    idat = chunk(b'IDAT', zlib.compress(raw))
    iend = chunk(b'IEND', b'')
    return header + ihdr + idat + iend
w, h = 256, 256
pixels = bytearray()
for y in range(h):
    for x in range(w):
        cx, cy = x - w//2, y - h//2
        in_circle = (cx*cx + cy*cy) < 40*40
        is_corner = x < 32 or x >= w-32 or y < 32 or y >= h-32
        if is_corner:
            pixels.extend([0, 0, 0, 0])
        elif in_circle:
            pixels.extend([76, 175, 80, 255])
        elif 48 <= x <= 208 and 40 <= y <= 216:
            pixels.extend([15, 52, 96, 255])
        else:
            pixels.extend([26, 26, 46, 255])
rows = [bytes(pixels[i*w*4:(i+1)*w*4]) for i in range(h)]
with open('${HOME}/.local/share/icons/hicolor/256x256/apps/esportable32.png', 'wb') as f:
    f.write(create_png(w, h, rows))
"
cp "${INSTALL_DIR}/esportable32.svg" "${HOME}/.local/share/icons/hicolor/scalable/apps/" 2>/dev/null || true

# Criar desktop entry apontando para o AppImage
cat > "${HOME}/.local/share/applications/esportable32.desktop" << DESKTOP
[Desktop Entry]
Type=Application
Name=ESPortable32
Comment=Interface desktop para ESP32 — GPIO, editor, terminal, loja
Exec=${APPIMAGE_DEST}
Icon=esportable32
Categories=Development;Electronics;
Terminal=false
StartupNotify=true
DESKTOP

ok "Atalho criado: Menu de aplicativos → ESPortable32"

# 6. Verificar PATH
case ":${PATH}:" in
    *:"${BIN_DIR}":*)
        ;;
    *)
        SHELL_CONFIG="${HOME}/.bashrc"
        if [ -n "${ZSH_VERSION:-}" ]; then
            SHELL_CONFIG="${HOME}/.zshrc"
        fi
        echo "" >> "${SHELL_CONFIG}"
        echo "# ESPortable32 GUI" >> "${SHELL_CONFIG}"
        echo "export PATH=\"\${PATH}:${BIN_DIR}\"" >> "${SHELL_CONFIG}"
        warn "Adicione ${BIN_DIR} ao seu PATH:"
        warn "  source ${SHELL_CONFIG}"
        ;;
esac

echo ""
echo -e "  ${BOLD}${GREEN}──────────────────────────────────────────${RESET}"
echo -e "  ${BOLD}Instalação concluída!${RESET}"
echo -e "  ${BOLD}${GREEN}──────────────────────────────────────────${RESET}"
echo ""
echo -e "  Para abrir:"
echo -e "    • Menu de aplicativos → ${BOLD}ESPortable32${RESET}"
echo -e "    • ${BOLD}${APPIMAGE_DEST}${RESET}"
echo ""
echo -e "  Para iniciar pelo terminal:"
echo -e "    ${BOLD}ESPortable32.AppImage${RESET}"
echo ""
