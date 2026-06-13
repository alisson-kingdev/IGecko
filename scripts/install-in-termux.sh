#!/bin/bash

# =========================================================
# 🦎 IGecko Termux Installer (With Real-time Diagnostics)
# =========================================================

set -e

# ---------------------------
# Colors and Styling
# ---------------------------
setup_ui() {
    if ! command -v toilet >/dev/null 2>&1 || ! command -v figlet >/dev/null 2>&1 || ! command -v tput >/dev/null 2>&1; then
        pkg install -y toilet figlet ncurses-utils > /dev/null 2>&1 || true
    fi

    if command -v tput >/dev/null 2>&1; then
        RED=$(tput setaf 1)
        GREEN=$(tput setaf 2)
        YELLOW=$(tput setaf 3)
        BLUE=$(tput setaf 4)
        MAGENTA=$(tput setaf 5)
        CYAN=$(tput setaf 6)
        BOLD=$(tput bold)
        RESET=$(tput sgr0)
    else
        RED='\033[1;31m'
        GREEN='\033[1;32m'
        YELLOW='\033[1;33m'
        BLUE='\033[1;34m'
        MAGENTA='\033[1;35m'
        CYAN='\033[1;36m'
        BOLD='\033[1m'
        RESET='\033[0m'
    fi
}

# ---------------------------
# Error Diagnosis Engine
# ---------------------------
handle_failure() {
    local message=$1
    local log_file=$2
    echo -e "${RED}${BOLD}[ FAIL ]${RESET}"
    echo -e "\n${RED}========================================================="
    echo -e "💥 CRITICAL ERROR DIAGNOSTICS"
    echo -e "=========================================================${RESET}"
    echo -e "${YELLOW}Step Failed:${RESET} $message"
    echo -e "${YELLOW}Last 10 lines of system output:${RESET}"
    echo -e "---------------------------------------------------------"
    if [ -f "$log_file" ]; then
        tail -n 10 "$log_file" | sed 's/^/  /'
    else
        echo "  No detailed log stream captured."
    fi
    echo -e "---------------------------------------------------------"
    echo -e "${CYAN}💡 Troubleshooting Tips:${RESET}"
    echo -e "  1. Run ${BOLD}pkg update && pkg upgrade -y${RESET} manually to fix broken Termux mirrors."
    echo -e "  2. Ensure you have an active internet connection."
    echo -e "  3. Check storage space using ${BOLD}df -h${RESET}."
    echo -e "${RED}=========================================================${RESET}"
    rm -f "$log_file"
    exit 1
}

# ---------------------------
# Progress Animation Engines
# ---------------------------

# Engine 1: Parses real-time apt output for native system package installation
run_apt_step() {
    local message=$1
    shift
    local log_file=$(mktemp)
    
    printf "  ${CYAN}•${RESET} ${BOLD}%-45s${RESET}" "$message"
    
    # Use apt-get status-fd descriptor to catch real progress percentages safely
    apt-get -o APT::Status-Fd=3 "$@" > "$log_file" 2>&1 3>&1 | while read -r line; do
        if [[ "$line" =~ ^([^:]+):([^:]+):([0-9\.]+):(.*)$ ]]; then
            local percent=${BASH_REMATCH[3]}
            # Format to drop decimals for clean UI
            percent=${percent%.*} 
            printf "\r  ${CYAN}•${RESET} ${BOLD}%-45s${RESET} [ ${YELLOW}%3d%%${RESET} ]" "$message" "$percent"
        fi
    done

    # Check actual final status code
    if [ ${PIPESTATUS[0]} -eq 0 ]; then
        # Overwrite trailing % metrics smoothly using explicit width padding
        printf "\r  ${CYAN}•${RESET} ${BOLD}%-45s${RESET} %-9s\n" "$message" "${GREEN}${BOLD}[ OK ]${RESET}"
        rm -f "$log_file"
    else
        handle_failure "$message" "$log_file"
    fi
}

# Engine 2: Simulates sequential smooth progress updates for compiled tasks (builds/moves)
run_animated_step() {
    local message=$1
    shift
    local log_file=$(mktemp)
    
    printf "  ${CYAN}•${RESET} ${BOLD}%-45s${RESET} [ ${YELLOW}  0%%${RESET} ]" "$message"
    
    # Launch work in background
    "$@" > "$log_file" 2>&1 &
    local pid=$!
    
    local progress=0
    while kill -0 $pid 2>/dev/null; do
        sleep 0.2
        # Smooth scaling animation up to 98% until it completely finishes
        if [ $progress -lt 98 ]; then
            ((progress+=2))
        fi
        printf "\r  ${CYAN}•${RESET} ${BOLD}%-45s${RESET} [ ${YELLOW}%3d%%${RESET} ]" "$message" "$progress"
    done
    
    wait $pid
    if [ $? -eq 0 ]; then
        # Explicit width string override padding to wipe out old '[ 100% ]' footprints
        printf "\r  ${CYAN}•${RESET} ${BOLD}%-45s${RESET} %-9s\n" "$message" "${GREEN}${BOLD}[ OK ]${RESET}"
        rm -f "$log_file"
    else
        printf "\r  ${CYAN}•${RESET} ${BOLD}%-45s${RESET}" "$message"
        handle_failure "$message" "$log_file"
    fi
}

# ---------------------------
# Main Execution Workflow
# ---------------------------
setup_ui

clear
if command -v toilet >/dev/null 2>&1; then
    echo -e "${MAGENTA}"
    toilet -f small "IGecko"
    echo -e "${RESET}"
elif command -v figlet >/dev/null 2>&1; then
    echo -e "${MAGENTA}"
    figlet "IGecko"
    echo -e "${RESET}"
else
    echo -e "${MAGENTA}${BOLD}🦎 IGecko Language${RESET}\n"
fi

echo -e "${BLUE}${BOLD}Termux Optimization & Installation${RESET}"
echo -e "---------------------------------------------------------"

# APT steps use native descriptor tracking
run_apt_step "Updating environment" install -y --allow-change-held-packages
run_apt_step "Installing system dependencies" install -y git cmake clang ninja build-essential python micro

# Local files verification
if [ ! -f "./scripts/build.sh" ]; then
    echo -e "\n${RED}Error: Build script missing at ./scripts/build.sh${RESET}"
    exit 1
fi

# Native tasks use smooth increment tracking
run_animated_step "Building binaries" bash ./scripts/build.sh

BIN_DIR="${PREFIX:-/data/data/com.termux/files/usr}/bin"
run_animated_step "Installing binaries globally" bash -c "cp build/bin/gecko $BIN_DIR/gecko && cp build/bin/gpm $BIN_DIR/gpm && chmod +x $BIN_DIR/gecko $BIN_DIR/gpm"

echo -e "---------------------------------------------------------"
echo -e "${GREEN}${BOLD}✔ Installation completed successfully!${RESET}"
echo -e "${YELLOW}Usage:${RESET} gecko <file.gk> | gpm <command>"
