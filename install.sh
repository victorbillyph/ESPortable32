#!/usr/bin/env bash
set -euo pipefail

REPO_URL="https://github.com/victorbillyph/ESPortable32"
INSTALL_DIR="${HOME}/.local/share/esportable32"
BIN_DIR="${HOME}/.local/bin"
CMD="${BIN_DIR}/esportable32"

GREEN='\033[92m'
CYAN='\033[96m'
YELLOW='\033[93m'
BOLD='\033[1m'
RESET='\033[0m'

step()  { echo -e "  ${CYAN}→${RESET} $1"; }
ok()    { echo -e "  ${GREEN}✓${RESET} $1"; }
warn()  { echo -e "  ${YELLOW}⚠${RESET} $1"; }

echo ""
echo -e "  ${BOLD}${CYAN}──────────────────────────────────────${RESET}"
echo -e "  ${BOLD}ESPortable32 - Instalação${RESET}"
echo -e "  ${BOLD}${CYAN}──────────────────────────────────────${RESET}"
echo ""

# 1. Clonar / atualizar repositório
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

# 2. Python venv
VENV_DIR="${INSTALL_DIR}/.venv"
if [ ! -d "${VENV_DIR}" ]; then
    step "Criando virtual environment..."
    python3 -m venv "${VENV_DIR}"
    ok "Virtual environment criado"
fi

step "Instalando PlatformIO + esptool..."
"${VENV_DIR}/bin/pip" install -q platformio esptool 2>/dev/null
ok "Dependências instaladas"

# 3. Instalar comando
mkdir -p "${BIN_DIR}"

cat > "${CMD}" << 'SCRIPT'
#!/usr/bin/env bash
PROJECT_DIR="${HOME}/.local/share/esportable32"
VENV_PIO="${PROJECT_DIR}/.venv/bin/pio"
VENV_PYTHON="${PROJECT_DIR}/.venv/bin/python"

# Se estiver dentro do diretório do projeto, usa ele
if [ -f "./esportable32.py" ]; then
    exec python3 ./esportable32.py "$@"
fi

# Senão, usa o instalado
exec "${VENV_PYTHON}" "${PROJECT_DIR}/esportable32.py" "$@"
SCRIPT

chmod +x "${CMD}"
ok "Comando instalado: ${CMD}"

# 4. Verificar PATH
case ":${PATH}:" in
    *:"${BIN_DIR}":*)
        ;;
    *)
        SHELL_CONFIG="${HOME}/.bashrc"
        if [ -n "${ZSH_VERSION:-}" ]; then
            SHELL_CONFIG="${HOME}/.zshrc"
        fi
        echo "" >> "${SHELL_CONFIG}"
        echo "# ESPortable32" >> "${SHELL_CONFIG}"
        echo "export PATH=\"\${PATH}:${BIN_DIR}\"" >> "${SHELL_CONFIG}"
        warn "Adicione ${BIN_DIR} ao seu PATH:"
        warn "  source ${SHELL_CONFIG}"
        ;;
esac

echo ""
echo -e "  ${BOLD}${GREEN}──────────────────────────────────────${RESET}"
echo -e "  ${BOLD}Instalação concluída!${RESET}"
echo -e "  ${BOLD}${GREEN}──────────────────────────────────────${RESET}"
echo ""
echo -e "  Agora é só digitar:"
echo ""
echo -e "    ${BOLD}esportable32${RESET}"
echo ""
echo -e "  Para ver o menu completo."
echo -e "  Ou:"
echo -e "    ${BOLD}esportable32 flash${RESET}       Compilar e gravar"
echo -e "    ${BOLD}esportable32 config${RESET}      Configurar WiFi"
echo -e "    ${BOLD}esportable32 monitor${RESET}     Monitor serial"
echo ""
