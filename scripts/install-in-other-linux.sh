#!/bin/bash
set -euo pipefail

# ----------------------------------------------------------------------
# 🦎 Gecko Installer
# ----------------------------------------------------------------------

# Cores e estilo
RED='\033[1;31m'
GREEN='\033[1;32m'
BOLD='\033[1m'
NC='\033[0m'

trap 'echo -e "\n${RED}[✗] Erro inesperado na linha $LINENO${NC}" >&2; exit 1' ERR

# --------------------------------------------
# Funções de animação
# --------------------------------------------
spinner() {
    local pid=$1
    local delay=0.08
    local spinstr='⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏'
    local i=0
    while kill -0 "$pid" 2>/dev/null; do
        printf "\b${spinstr:$i:1}"
        i=$(( (i+1) % ${#spinstr} ))
        sleep "$delay"
    done
    printf "\b"
}

run_step() {
    local description="$1"
    shift
    printf "  ${BOLD}%-40s${NC} " "$description"
    ("$@" > /dev/null 2>&1) &
    local cmd_pid=$!
    spinner $cmd_pid
    wait $cmd_pid
    local exit_code=$?
    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}✓${NC}"
    else
        echo -e "${RED}✗ (código $exit_code)${NC}"
        trap - ERR
        exit $exit_code
    fi
}

# --------------------------------------------
# Localização da raiz do projeto
# --------------------------------------------
find_project_root() {
    local script_dir
    script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

    if [[ -f "$script_dir/CMakeLists.txt" ]]; then
        echo "$script_dir"
        return
    fi

    if [[ -f "$script_dir/../CMakeLists.txt" ]]; then
        echo "$(cd "$script_dir/.." && pwd)"
        return
    fi

    local dir="$script_dir"
    for _ in {1..3}; do
        dir="$(dirname "$dir")"
        if [[ -f "$dir/CMakeLists.txt" ]]; then
            echo "$dir"
            return
        fi
    done

    echo -e "${RED}Não foi possível encontrar a raiz do projeto (CMakeLists.txt).${NC}" >&2
    exit 1
}

# --------------------------------------------
# Detecção do gerenciador de pacotes
# --------------------------------------------
detect_package_manager() {
    if command -v apt &>/dev/null; then
        echo "apt"
    elif command -v dnf &>/dev/null; then
        echo "dnf"
    elif command -v pacman &>/dev/null; then
        echo "pacman"
    else
        echo -e "${RED}Gerenciador de pacotes não suportado.${NC}" >&2
        exit 1
    fi
}

detect_os() {
    case "$OSTYPE" in
        linux-gnu*) echo "Linux" ;;
        darwin*)    echo "macOS" ;;
        *)
            echo -e "${RED}Sistema operacional não suportado ($OSTYPE).${NC}" >&2
            exit 1
            ;;
    esac
}

# --------------------------------------------
# Instalação de dependências
# --------------------------------------------
install_dependencies() {
    local pm="$1"
    echo -e "\n${BOLD}📦 Instalando dependências via ${pm}...${NC}"

    case "$pm" in
        apt)
            run_step "Atualizando lista de pacotes"          sudo apt-get update -y
            run_step "Instalando git, cmake, clang..."       sudo apt-get install -y git cmake clang build-essential python3 micro
            ;;
        dnf)
            run_step "Atualizando repositórios"              sudo dnf update -y
            run_step "Instalando git, cmake, clang..."       sudo dnf install -y git cmake clang gcc-c++ python3 micro
            ;;
        pacman)
            run_step "Sincronizando e atualizando sistema"   sudo pacman -Syu --noconfirm
            run_step "Instalando git, cmake, clang..."       sudo pacman -S --noconfirm git cmake clang base-devel python micro
            ;;
    esac
}

# --------------------------------------------
# Compilação do Gecko
# --------------------------------------------
build_gecko() {
    local project_root="$1"
    local build_script="$project_root/scripts/build.sh"

    if [[ ! -f "$build_script" ]]; then
        build_script="$project_root/build.sh"
    fi
    if [[ ! -f "$build_script" ]]; then
        echo -e "${RED}Script de build não encontrado.${NC}" >&2
        exit 1
    fi

    echo -e "\n${BOLD}🔧 Compilando Gecko (raiz: $project_root)...${NC}"

    chmod +x "$build_script"

    (
        cd "$project_root"
        run_step "Executando build.sh" "$build_script"
    )
}

# --------------------------------------------
# Instalação dos binários
# --------------------------------------------
install_binaries() {
    local project_root="$1"
    local binaries=("gecko" "gpm")

    for binary_name in "${binaries[@]}"; do
        local candidates=("$project_root/build/$binary_name" "$project_root/$binary_name")
        local src=""

        for cand in "${candidates[@]}"; do
            if [[ -f "$cand" && -x "$cand" ]]; then
                src="$cand"
                break
            fi
        done

        if [[ -z "$src" ]]; then
            echo -e "${RED}Binário '$binary_name' não encontrado após compilação.${NC}" >&2
            exit 1
        fi

        echo -e "\n${BOLD}🚀 Instalando $binary_name em /usr/local/bin...${NC}"
        run_step "Copiando binário $binary_name" sudo cp "$src" "/usr/local/bin/$binary_name"
        sudo chmod +x "/usr/local/bin/$binary_name"
    done
}

# --------------------------------------------
# Execução principal
# --------------------------------------------
main() {
    clear
    echo -e "  ${GREEN}${BOLD}🦎  G E C K O   I N S T A L L E R${NC}"
    echo -e "  ${GREEN}-----------------------------------${NC}\n"

    detect_os >/dev/null

    local PM
    PM=$(detect_package_manager)

    local PROJECT_ROOT
    PROJECT_ROOT=$(find_project_root)

    install_dependencies "$PM"
    build_gecko "$PROJECT_ROOT"
    install_binaries "$PROJECT_ROOT"

    if command -v gecko &>/dev/null && command -v gpm &>/dev/null; then
        echo -e "\n${GREEN}${BOLD}🎉 Pronto! Gecko e GPM instalados com sucesso.${NC}"
        echo -e "Use: ${BOLD}gecko <script>${NC} ou ${BOLD}gpm <comando>${NC}\n"
    else
        echo -e "\n${RED}Instalação concluída, mas os comandos não estão no PATH.${NC}" >&2
        exit 1
    fi
}

# Inicia o script
main "$@"