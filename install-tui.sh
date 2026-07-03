#!/usr/bin/env bash
set -euo pipefail

REPO_URL="https://github.com/victorbillyph/ESPortable32"
INSTALL_DIR="${HOME}/.local/share/esportable32"
BIN_DIR="${HOME}/.local/bin"
CMD="${BIN_DIR}/esportableui"

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
echo -e "  ${BOLD}${CYAN}──────────────────────────────────────${RESET}"
echo -e "  ${BOLD}ESPortable32 TUI - Instalação${RESET}"
echo -e "  ${BOLD}${CYAN}──────────────────────────────────────${RESET}"
echo ""
echo -e "  Instala o comando ${BOLD}esportableui${RESET} (interface de terminal)"
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
VENV_DIR="${INSTALL_DIR}/.venv-tui"
if [ ! -d "${VENV_DIR}" ]; then
    step "Criando virtual environment..."
    python3 -m venv "${VENV_DIR}"
    ok "Virtual environment criado"
fi

step "Instalando dependências (textual, requests, pyserial)..."
"${VENV_DIR}/bin/pip" install -q textual requests pyserial 2>/dev/null
ok "Dependências instaladas"

# 3. Instalar comando esportableui
mkdir -p "${BIN_DIR}"

cat > "${CMD}" << SCRIPT
#!/usr/bin/env bash
PROJECT_DIR="${INSTALL_DIR}"
VENV_PYTHON="${VENV_DIR}/bin/python"

# Se estiver dentro do diretório do projeto, usa ele diretamente
if [ -f "./esportable32_tui.py" ]; then
    exec python3 ./esportable32_tui.py "\$@"
fi

# Senão, usa o instalado
exec "\${VENV_PYTHON}" "\${PROJECT_DIR}/esportable32_tui.py" "\$@"
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
        echo "# ESPortable32 TUI" >> "${SHELL_CONFIG}"
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
echo -e "    ${BOLD}esportableui${RESET}"
echo ""
echo -e "  Para abrir a interface de terminal do ESPortable32."
echo -e "  Certifique-se de que o ESP32 está ligado e acessível."
echo ""
