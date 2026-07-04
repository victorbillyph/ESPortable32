#!/usr/bin/env bash
set -euo pipefail

REPO_URL="https://github.com/victorbillyph/ESPortable32"
INSTALL_DIR="${HOME}/.local/share/esportable32"
BIN_DIR="${HOME}/.local/bin"
CMD="${BIN_DIR}/esportable32"
VENV_DIR="${INSTALL_DIR}/.venv-tui"

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

# 2. Python venv para TUI (Textual)
if [ ! -d "${VENV_DIR}" ]; then
    step "Criando virtual environment para TUI..."
    python3 -m venv "${VENV_DIR}"
    ok "Virtual environment criado"
fi

step "Instalando dependencias (Textual, pyserial)..."
"${VENV_DIR}/bin/pip" install -q textual pyserial requests 2>/dev/null || true
ok "Dependencias instaladas"

# 3. Instalar comando único
mkdir -p "${BIN_DIR}"

cat > "${CMD}" << SCRIPT
#!/usr/bin/env bash
PROJECT_DIR="${INSTALL_DIR}"
VENV_PYTHON="${VENV_DIR}/bin/python"

if [ -f "./esportable32_tui.py" ]; then
    exec python3 ./esportable32_tui.py "\$@"
fi

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
echo -e "  A ferramenta vai fazer o boot, detectar seu ESP32"
echo -e "  e abrir o desktop. Se não encontrar, oferece:"
echo -e "    - Instalar firmware no ESP32"
echo -e "    - Reparar conexão"
echo -e "    - Buscar atualizações"
echo ""
