#!/bin/bash

# =========================================================
# IGecko Build Script
# =========================================================

set -e

BUILD_DIR="build"
THREADS=$(nproc 2>/dev/null || echo 4)

# Create build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory..."
    mkdir -p "$BUILD_DIR"
fi

echo "Configuring project..."
cmake -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release

echo "Building with $THREADS threads..."
cmake --build "$BUILD_DIR" --parallel "$THREADS"

echo "Build completed successfully!"
echo "Binaries available in $BUILD_DIR/bin/"
