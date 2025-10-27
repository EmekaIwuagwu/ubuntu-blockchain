#!/bin/bash
# Ubuntu Blockchain - Build Script
# Made in Africa, For Africa

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "🌍 Ubuntu Blockchain - Build Script"
echo "===================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if vcpkg is available
if [ -z "$VCPKG_ROOT" ]; then
    if [ -d "$HOME/vcpkg" ]; then
        export VCPKG_ROOT="$HOME/vcpkg"
    else
        echo -e "${RED}❌ Error: vcpkg not found${NC}"
        echo "Please install vcpkg first:"
        echo "  git clone https://github.com/Microsoft/vcpkg.git \$HOME/vcpkg"
        echo "  cd \$HOME/vcpkg && ./bootstrap-vcpkg.sh"
        exit 1
    fi
fi

VCPKG_TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

if [ ! -f "$VCPKG_TOOLCHAIN" ]; then
    echo -e "${RED}❌ Error: vcpkg toolchain file not found at $VCPKG_TOOLCHAIN${NC}"
    exit 1
fi

echo -e "${GREEN}✓${NC} vcpkg found at: $VCPKG_ROOT"
echo ""

# Build type (default: Release)
BUILD_TYPE="${1:-Release}"

echo "Build Configuration:"
echo "  - Build Type: $BUILD_TYPE"
echo "  - C++ Standard: C++20"
echo "  - Toolchain: vcpkg"
echo ""

# Create build directory
BUILD_DIR="build"
if [ -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}⚠${NC} Build directory exists. Cleaning..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo ""
echo "🔧 Configuring CMake..."
echo ""

cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN" \
    -DBUILD_TESTS=ON \
    -DBUILD_BENCHMARKS=OFF \
    -DENABLE_PEG=ON

echo ""
echo "🔨 Building Ubuntu Blockchain..."
echo ""

# Build with all available cores
CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
echo "Using $CORES parallel jobs"
echo ""

cmake --build . --config "$BUILD_TYPE" -j"$CORES"

echo ""
echo -e "${GREEN}✅ Build successful!${NC}"
echo ""
echo "Executables built:"
if [ -f "bin/ubud" ]; then
    echo -e "  ${GREEN}✓${NC} bin/ubud (blockchain daemon)"
else
    echo -e "  ${RED}✗${NC} bin/ubud (MISSING)"
fi

if [ -f "bin/ubu-cli" ]; then
    echo -e "  ${GREEN}✓${NC} bin/ubu-cli (command-line interface)"
else
    echo -e "  ${RED}✗${NC} bin/ubu-cli (MISSING)"
fi

if [ -f "bin/ubu_tests" ]; then
    echo -e "  ${GREEN}✓${NC} bin/ubu_tests (test suite)"
else
    echo -e "  ${YELLOW}⚠${NC} bin/ubu_tests (optional, not built)"
fi

echo ""
echo "📝 Next steps:"
echo ""
echo "1. Run tests:"
echo "   cd build && ctest"
echo ""
echo "2. Start blockchain daemon (testnet):"
echo "   ./build/bin/ubud --datadir=~/.ubuntu-testnet --testnet"
echo ""
echo "3. Use CLI wallet:"
echo "   ./build/bin/ubu-cli --datadir=~/.ubuntu-testnet help"
echo ""
echo -e "${YELLOW}⚠  WARNING: This is TESTNET software${NC}"
echo -e "${YELLOW}   DO NOT use with real money or production systems${NC}"
echo ""
