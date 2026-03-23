#!/bin/bash
# Auto-build script - run this after vcpkg finishes

set -e  # Exit on error

echo "================================================"
echo "HAMSTRING C++ Auto-Build Script"
echo "================================================"
echo ""

# Check if vcpkg finished
if [ ! -d "$HOME/vcpkg/installed/arm64-osx" ]; then
    echo "Error: vcpkg dependencies not installed yet"
    echo "Please wait for 'vcpkg install' to complete first"
    exit 1
fi

echo "✓ vcpkg dependencies installed"
echo ""

# Configure CMake
echo "Step 1: Configuring CMake..."
cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo ""
echo "✓ CMake configured"
echo ""

# Build
echo "Step 2: Building project..."
cmake --build build -j$(sysctl -n hw.ncpu)

echo ""
echo "✓ Build complete!"
echo ""

# Show what was built
echo "Built executables:"
echo "  - build/examples/demo"
echo "  - build/src/logserver/logserver"
echo ""

# Optionally run tests
read -p "Run tests? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    cd build && ctest --output-on-failure
    cd ..
fi

echo ""
echo "================================================"
echo "Build complete! You can now run:"
echo "  ./build/examples/demo ../config.yaml"
echo "  ./build/src/logserver/logserver ../config.yaml"
echo "================================================"
