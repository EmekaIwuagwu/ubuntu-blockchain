#!/bin/bash
# Ubuntu Blockchain Test Runner

set -e

echo "======================================"
echo "Ubuntu Blockchain - Test Suite"
echo "======================================"
echo

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Build directory
BUILD_DIR="build"

if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory not found. Run 'cmake -B build && cmake --build build' first${NC}"
    exit 1
fi

# Run unit tests
echo "Running Unit Tests..."
if [ -f "$BUILD_DIR/bin/ubu_tests" ]; then
    $BUILD_DIR/bin/ubu_tests
    echo -e "${GREEN}✓ Unit tests passed${NC}"
else
    echo -e "${RED}✗ Unit test binary not found${NC}"
fi

echo
echo "All tests completed!"
