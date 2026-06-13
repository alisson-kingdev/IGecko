#!/usr/bin/env bash

set -euo pipefail

# =========================================================
# IGecko Formatter
# =========================================================

RESET="\033[0m"
BOLD="\033[1m"
RED="\033[31m"
GREEN="\033[32m"
YELLOW="\033[33m"
BLUE="\033[34m"
MAGENTA="\033[35m"
CYAN="\033[36m"
WHITE="\033[37m"

CHECK_MODE=false
if [[ "${1:-}" == "--check" ]]; then
  CHECK_MODE=true
fi

spinner() {
  local pid=$1
  local delay=0.08
  local spinstr='⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏'
  while ps -p "${pid}" > /dev/null 2>&1; do
    local temp=${spinstr#?}
    printf "\r${CYAN}${BOLD}[%c]${RESET} %s..." "${spinstr}" "$([ "$CHECK_MODE" = true ] && echo "Checking" || echo "Formatting")"
    local spinstr=${temp}${spinstr%"$temp"}
    sleep "${delay}"
  done
  printf "\r"
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

if ! command -v clang-format >/dev/null 2>&1; then
  echo -e "${RED}${BOLD}Error:${RESET} clang-format not found."
  exit 1
fi

mapfile -t FILES < <(find "${PROJECT_ROOT}/src" "${PROJECT_ROOT}/examples" -type f \( -name "*.cpp" -o -name "*.h" \) | sort)

TOTAL=${#FILES[@]}
START_TIME=$(date +%s)
COUNT=0
ISSUES=0

for FILE in "${FILES[@]}"; do
  ((COUNT += 1))
  RELATIVE="${FILE#${PROJECT_ROOT}/}"

  if [ "$CHECK_MODE" = true ]; then
    if ! clang-format --dry-run --Werror "$FILE" >/dev/null 2>&1; then
      echo -e "${RED}${BOLD}[%3d/%3d]${RESET} ${WHITE}Needs Formatting${RESET} ${CYAN}%s${RESET}" "${COUNT}" "${TOTAL}" "${RELATIVE}"
      ((ISSUES++))
    fi
  else
    (clang-format -i "${FILE}") &
    PID=$!
    spinner "${PID}"
    wait "${PID}"
    printf "${GREEN}${BOLD}[%3d/%3d]${RESET} ${WHITE}Formatted${RESET} ${CYAN}%s${RESET}\n" "${COUNT}" "${TOTAL}" "${RELATIVE}"
  fi
done

if [ "$CHECK_MODE" = true ]; then
  if [ "$ISSUES" -gt 0 ]; then
    echo -e "\n${RED}${BOLD}✘ Formatting issues detected in ${ISSUES} files.${RESET}"
    exit 1
  else
    echo -e "\n${GREEN}${BOLD}✔ Formatting is correct.${RESET}"
  fi
fi
