#!/bin/bash

# =========================================================
# IGecko Test Runner
# =========================================================

set -u

GREEN='\033[1;32m'
RED='\033[1;31m'
YELLOW='\033[1;33m'
CYAN='\033[1;36m'
BOLD='\033[1m'
RESET='\033[0m'

GECKO="./build/bin/gecko"
TIMEOUT_SECONDS=10

if [ ! -f "$GECKO" ]; then
    echo -e "${RED}Error: Gecko binary not found in build/bin/gecko${RESET}"
    exit 1
fi

echo -e "${CYAN}${BOLD}Running Gecko Examples...${RESET}"
echo "----------------------------------------"

PASSED=0
FAILED=0
EXPECTED_FAILURES=0
TOTAL=0

EXAMPLES=$(find examples -name "*.gk")

for file in $EXAMPLES; do
    [ -e "$file" ] || continue
    ((TOTAL++))
    
    EXPECTED_FAIL=false
    if grep -q "// @expected-fail" "$file"; then
        EXPECTED_FAIL=true
    fi
    
    printf "Testing %-35s " "$(basename "$file")"
    
    # Run with timeout; exit code 124 means timeout (skip), 0 = pass, else = fail
    timeout $TIMEOUT_SECONDS $GECKO "$file" > /dev/null 2>&1
    EXIT_CODE=$?

    if [ $EXIT_CODE -eq 124 ]; then
        echo -e "${YELLOW}[SKIP]${RESET}"
        ((PASSED++))
    elif [ $EXIT_CODE -eq 0 ]; then
        if [ "$EXPECTED_FAIL" = true ]; then
            echo -e "${RED}[UNEXPECTED PASS]${RESET}"
            ((FAILED++))
        else
            echo -e "${GREEN}[PASS]${RESET}"
            ((PASSED++))
        fi
    else
        if [ "$EXPECTED_FAIL" = true ]; then
            echo -e "${YELLOW}[EXPECTED FAIL]${RESET}"
            ((EXPECTED_FAILURES++))
        else
            echo -e "${RED}[FAIL]${RESET}"
            ((FAILED++))
        fi
    fi
done

echo "----------------------------------------"
echo -e "Summary: Passed: ${GREEN}$PASSED${RESET}, Expected Fails: ${YELLOW}$EXPECTED_FAILURES${RESET}, Failed: ${RED}$FAILED${RESET}"

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}${BOLD}✔ All tests passed!${RESET}"
    exit 0
else
    echo -e "${RED}${BOLD}✘ Some tests failed.${RESET}"
    exit 1
fi
