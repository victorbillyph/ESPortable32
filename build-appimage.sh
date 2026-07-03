#!/usr/bin/env bash
set -euo pipefail

APP="ESPortable32"
VERSION="1.0.0"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

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
echo -e "  ${BOLD}${APP} - AppImage Builder${RESET}"
echo -e "  ${BOLD}${CYAN}──────────────────────────────────────────${RESET}"
echo ""

BUILD_DIR="$(mktemp -d)"
APPDIR="${BUILD_DIR}/${APP}.AppDir"
DEST="${SCRIPT_DIR}/${APP}-${VERSION}-x86_64.AppImage"

cleanup() { rm -rf "${BUILD_DIR}"; }
trap cleanup EXIT

# ── 1. Verificar Python ──
step "Verificando Python..."
PYTHON=$(command -v python3 || command -v python)
if [ -z "${PYTHON}" ]; then
    err "Python 3 nao encontrado"
    exit 1
fi
ok "${PYTHON}"

# ── 2. Criar AppDir ──
step "Criando estrutura AppDir..."
mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"
mkdir -p "${APPDIR}/usr/share/${APP}/app"
ok "Estrutura criada"

# ── 3. Virtualenv com dependencias ──
step "Criando virtualenv e instalando dependencias..."
VENV_DIR="${APPDIR}/usr/share/${APP}/venv"
"${PYTHON}" -m venv "${VENV_DIR}"
source "${VENV_DIR}/bin/activate"
pip install -q requests pyserial 2>/dev/null
deactivate
ok "Dependencias instaladas (requests, pyserial)"

# ── 4. Copiar app ──
step "Copiando aplicacao..."
cp "${SCRIPT_DIR}/esportable32_gui.py" "${APPDIR}/usr/share/${APP}/app/"
cp "${SCRIPT_DIR}/esportable32.py" "${APPDIR}/usr/share/${APP}/app/" 2>/dev/null || true
ok "App copiado"

# ── 5. AppRun ──
step "Criando AppRun..."
cat > "${APPDIR}/AppRun" << 'EOF'
#!/usr/bin/env bash
set -euo pipefail
HERE="$(dirname "$(readlink -f "$0")")"
export PATH="${HERE}/usr/share/ESPortable32/venv/bin:${PATH}"
exec "${HERE}/usr/share/ESPortable32/venv/bin/python3" \
    "${HERE}/usr/share/ESPortable32/app/esportable32_gui.py" "$@"
EOF
chmod +x "${APPDIR}/AppRun"
ok "AppRun criado"

# ── 6. Desktop entry ──
step "Copiando desktop entry..."
cp "${SCRIPT_DIR}/ESPortable32.desktop" "${APPDIR}/"
cp "${SCRIPT_DIR}/ESPortable32.desktop" "${APPDIR}/usr/share/applications/"
ok "Desktop entry copiado"

# ── 7. Icon ──
step "Gerando icone PNG..."
"${PYTHON}" -c "
import struct, zlib

def create_png(width, height, pixels):
    def chunk(chunk_type, data):
        c = chunk_type + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)

    header = b'\\x89PNG\\r\\n\\x1a\\n'
    ihdr = chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
    raw = b''
    for row in pixels:
        raw += b'\\x00' + row
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
with open('${APPDIR}/esportable32.png', 'wb') as f:
    f.write(create_png(w, h, rows))
"
cp "${APPDIR}/esportable32.png" "${APPDIR}/usr/share/icons/hicolor/256x256/apps/"
# Copia SVG tambem
cp "${SCRIPT_DIR}/esportable32.svg" "${APPDIR}/usr/share/icons/hicolor/scalable/apps/" 2>/dev/null || true
ok "Icone gerado"

# ── 8. appimagetool ──
step "Preparando appimagetool..."
APPIMAGETOOL="${BUILD_DIR}/appimagetool"
if ! command -v appimagetool &>/dev/null; then
    ARCH=$(uname -m)
    if [ "${ARCH}" = "x86_64" ]; then
        step "Baixando appimagetool..."
        wget -q "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-${ARCH}.AppImage" \
            -O "${APPIMAGETOOL}" 2>/dev/null || \
        curl -sL "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-${ARCH}.AppImage" \
            -o "${APPIMAGETOOL}" 2>/dev/null || {
            warn "Nao foi possivel baixar appimagetool"
            warn "Tente instalar: sudo apt install appimagetool"
            ok "AppDir criado em: ${APPDIR}"
            ok "Execute manualmente: appimagetool ${APPDIR}"
            exit 0
        }
        chmod +x "${APPIMAGETOOL}"
        APPIMAGETOOL_BIN="${APPIMAGETOOL}"
    else
        # Para arquiteturas nao-x86_64, precisa do appimagetool via系统
        if command -v appimagetool &>/dev/null; then
            APPIMAGETOOL_BIN="appimagetool"
        else
            warn "appimagetool nao disponivel para ${ARCH}"
            ok "AppDir criado em: ${APPDIR}"
            ok "Execute: appimagetool ${APPDIR}"
            exit 0
        fi
    fi
else
    APPIMAGETOOL_BIN="appimagetool"
fi

# ── 9. Criar AppImage ──
step "Criando AppImage..."
APPIMAGETOOL_BIN="${APPIMAGETOOL_BIN:-appimagetool}"
ARCH=x86_64 "${APPIMAGETOOL_BIN}" "${APPDIR}" "${DEST}" 2>&1 | tail -3
ok "AppImage criada: ${DEST}"

echo ""
echo -e "  ${BOLD}${GREEN}──────────────────────────────────────────${RESET}"
echo -e "  ${BOLD}AppImage pronta!${RESET}"
echo -e "  ${BOLD}${GREEN}──────────────────────────────────────────${RESET}"
echo ""
echo -e "  Arquivo: ${BOLD}${DEST}${RESET}"
echo -e "  Tamanho: $(du -h "${DEST}" | cut -f1)"
echo ""
echo -e "  Para usar:"
echo -e "    ${BOLD}chmod +x \"${DEST}\"${RESET}"
echo -e "    ${BOLD}./${APP}-${VERSION}-x86_64.AppImage${RESET}"
echo ""
