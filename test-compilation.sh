#!/bin/bash
set -e

echo "========================================="
echo "Ubuntu Blockchain Compilation Test Suite"
echo "========================================="
echo ""

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}Step 1: Cleaning previous build${NC}"
rm -rf build
mkdir -p build
cd build

echo -e "${GREEN}✓ Build directory created${NC}"
echo ""

echo -e "${YELLOW}Step 2: Configuring with CMake${NC}"
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DBUILD_BENCHMARKS=ON

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ CMake configuration successful${NC}"
else
    echo -e "${RED}✗ CMake configuration failed${NC}"
    exit 1
fi
echo ""

echo -e "${YELLOW}Step 3: Compiling (this may take 5-10 minutes)${NC}"
cmake --build . -j$(nproc) 2>&1 | tee compile.log

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Compilation successful${NC}"
else
    echo -e "${RED}✗ Compilation failed. Check compile.log${NC}"
    exit 1
fi
echo ""

echo -e "${YELLOW}Step 4: Verifying binaries${NC}"
if [ -f "bin/ubud" ] && [ -f "bin/ubu-cli" ]; then
    echo -e "${GREEN}✓ ubud binary created: $(du -h bin/ubud | cut -f1)${NC}"
    echo -e "${GREEN}✓ ubu-cli binary created: $(du -h bin/ubu-cli | cut -f1)${NC}"
else
    echo -e "${RED}✗ Binaries not found${NC}"
    exit 1
fi
echo ""

echo -e "${YELLOW}Step 5: Running unit tests${NC}"
if [ -f "bin/ubu_tests" ]; then
    ./bin/ubu_tests
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ All tests passed${NC}"
    else
        echo -e "${RED}✗ Some tests failed${NC}"
    fi
else
    echo -e "${YELLOW}⚠ Test binary not found (tests may be disabled)${NC}"
fi
echo ""

echo -e "${YELLOW}Step 6: Testing daemon startup${NC}"
timeout 5s ./bin/ubud -regtest -daemon 2>&1 || true
sleep 2

if pgrep -x "ubud" > /dev/null; then
    echo -e "${GREEN}✓ Daemon started successfully${NC}"
    
    echo -e "${YELLOW}Step 7: Testing RPC${NC}"
    ./bin/ubu-cli -regtest getblockchaininfo > /dev/null 2>&1
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ RPC communication working${NC}"
    else
        echo -e "${RED}✗ RPC communication failed${NC}"
    fi
    
    # Stop daemon
    ./bin/ubu-cli -regtest stop > /dev/null 2>&1 || true
    sleep 2
else
    echo -e "${RED}✗ Daemon failed to start${NC}"
fi
echo ""

echo "========================================="
echo -e "${GREEN}COMPILATION TEST COMPLETE!${NC}"
echo "========================================="
echo ""
echo "Summary:"
echo "  Build directory: $(pwd)"
echo "  Binaries:"
echo "    - ubud: $(readlink -f bin/ubud)"
echo "    - ubu-cli: $(readlink -f bin/ubu-cli)"
echo ""
echo "Next steps:"
echo "  1. Deploy to Azure: Follow docs/AZURE_DEPLOYMENT.md"
echo "  2. Run locally: ./bin/ubud -regtest"
echo "  3. Test transactions: See README.md examples"
echo ""
