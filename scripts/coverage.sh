#!/usr/bin/env bash

# =========================================================
# IGecko Coverage Script
# =========================================================

set -e

BUILD_DIR="build_coverage"

echo "Configuring for coverage..."
cmake -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="--coverage" \
  -DIGECKO_BUILD_TESTS=ON

echo "Building..."
cmake --build "$BUILD_DIR" --parallel

echo "Running tests..."
ctest --test-dir "$BUILD_DIR" --output-on-failure

echo "Generating HTML report..."
if command -v gcovr >/dev/null 2>&1; then
    gcovr -r . --html --html-details -o coverage.html --exclude 'build/.*' --exclude 'tests/.*'
    echo "Report generated: coverage.html"
else
    echo "gcovr not found. Please install it to generate HTML reports."
fi
