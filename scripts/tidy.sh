#!/usr/bin/env bash

set -euo pipefail

# =========================================================
# IGecko clang-tidy runner
# =========================================================

RESET="\033[0m"
RED="\033[31m"
GREEN="\033[32m"
YELLOW="\033[33m"
BLUE="\033[34m"
CYAN="\033[36m"
BOLD="\033[1m"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ -f "${SCRIPT_DIR}/../CMakeLists.txt" ]]; then
  PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
else
  PROJECT_ROOT="$(pwd)"
fi

BUILD_DIR="${PROJECT_ROOT}/build"

echo -e "${CYAN}${BOLD}"
echo "╔══════════════════════════════════════════════╗"
echo "║            IGecko clang-tidy                ║"
echo "╚══════════════════════════════════════════════╝"
echo -e "${RESET}"

# ---------------------------------------------------------
# Dependencies
# ---------------------------------------------------------

if ! command -v clang-tidy >/dev/null 2>&1; then
  echo -e "${RED}Error: clang-tidy not found${RESET}"
  exit 1
fi

# ---------------------------------------------------------
# Configure build (silent)
# ---------------------------------------------------------

echo -e "${BLUE}Configuring project for analysis...${RESET}"
mkdir -p "${BUILD_DIR}"
cmake -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DIGECKO_ENABLE_CLANG_TIDY=OFF > /dev/null 2>&1

# ---------------------------------------------------------
# Source files
# ---------------------------------------------------------

mapfile -t FILES < <(
  find "${PROJECT_ROOT}/src" \
    -type f \
    \( \
      -name "*.cpp" -o \
      -name "*.cc" -o \
      -name "*.cxx" \
    \) \
    | sort
)

TOTAL=${#FILES[@]}

if [[ "${TOTAL}" -eq 0 ]]; then
  echo -e "${YELLOW}No source files found.${RESET}"
  exit 0
fi

# ---------------------------------------------------------
# Detect libstdc++ include paths (clang-tidy needs them when
# compilation database was generated with GCC)
# ---------------------------------------------------------

EXTRA_ARGS=()
if command -v g++ >/dev/null 2>&1; then
  # libstdc++ include paths from GCC (clang needs explicit -I for these)
  STDCPP_PATHS=$(g++ -x c++ -std=c++20 -E -v /dev/null 2>&1 | grep " /usr/include" | grep "c++" | tr -d ' ')
  while IFS= read -r path; do
    [ -n "$path" ] && EXTRA_ARGS+=(--extra-arg="-I${path}")
  done <<< "$STDCPP_PATHS"
fi

# ---------------------------------------------------------
# Run tidy
# ---------------------------------------------------------

COUNT=0
ERRORS=0

echo -e "${BLUE}Analyzing ${TOTAL} files...${RESET}"
echo "----------------------------------------"

CLANG_TIDY_TIMEOUT=120

for FILE in "${FILES[@]}"; do
  ((COUNT += 1))
  RELATIVE="${FILE#${PROJECT_ROOT}/}"

  printf "${BLUE}[%2d/%2d]${RESET} Analyzing ${CYAN}%-40s${RESET} " "${COUNT}" "${TOTAL}" "${RELATIVE}"

  if timeout "${CLANG_TIDY_TIMEOUT}" clang-tidy "${FILE}" -p "${BUILD_DIR}" --use-color "${EXTRA_ARGS[@]}" > /dev/null 2>&1; then
    echo -e "${GREEN}[OK]${RESET}"
  else
    echo -e "${RED}[WARN/FAIL]${RESET}"
    ((ERRORS++))
    timeout "${CLANG_TIDY_TIMEOUT}" clang-tidy "${FILE}" -p "${BUILD_DIR}" --use-color "${EXTRA_ARGS[@]}" 2>&1 | tail -20 || true
  fi
done

echo "----------------------------------------"
if [ $ERRORS -eq 0 ]; then
  echo -e "${GREEN}${BOLD}✔ clang-tidy completed successfully with no major issues.${RESET}"
else
  echo -e "${YELLOW}${BOLD}⚠ clang-tidy completed with ${ERRORS} files having warnings/errors.${RESET}"
fi
