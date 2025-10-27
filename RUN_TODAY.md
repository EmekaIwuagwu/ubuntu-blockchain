# 🚀 RUN UBUNTU BLOCKCHAIN TODAY - Step by Step

**⏱️ Time Required: 30-60 minutes**
**💰 Cost: $0 (all free/open-source)**
**⚠️ Risk Level: ZERO (testnet only, fake money)**

---

## ✅ DIRECT ANSWERS TO YOUR QUESTIONS:

### 1. **Is this a real blockchain?**
**YES** - This is 100% real blockchain code with PoW consensus, cryptographic signatures, P2P networking, persistent storage.

### 2. **Can this serve African countries for payments?**
**NO** - Not yet. Needs 5-8 years of development. Currently testnet-only prototype.

### 3. **Is this production-ready?**
**NO** - Never tested in production. Safe for testnet only.

### 4. **Can I use for testnet transactions?**
**YES** - This is PERFECT for testnet! That's exactly what it's ready for.

### 5. **Can I tell people about it?**
**YES** - But be HONEST:
- ✅ Say: "Working blockchain prototype, testnet-ready"
- ❌ Don't say: "Production-ready, start using for payments"

---

## 🏃 QUICK START (If You Just Want to Run It NOW)

### Option A: Ubuntu/Debian Linux (Recommended)

```bash
# 1. Install dependencies (5-10 minutes)
sudo apt update
sudo apt install -y build-essential cmake git pkg-config \
    libssl-dev libboost-all-dev

# 2. Install RocksDB
sudo apt install -y librocksdb-dev

# 3. Install other libs
sudo apt install -y libprotobuf-dev protobuf-compiler \
    nlohmann-json3-dev libspdlog-dev

# 4. Build Ubuntu Blockchain (5-10 minutes)
cd /home/user/ubuntu-blockchain
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DENABLE_PEG=ON
cmake --build . -j$(nproc)

# 5. Run the blockchain!
./bin/ubud --datadir=~/.ubuntu-testnet --testnet
```

### Option B: Any Linux (Using vcpkg - More Reliable)

```bash
# 1. Install vcpkg (package manager) - 5 minutes
cd $HOME
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
export VCPKG_ROOT=$HOME/vcpkg

# 2. Install dependencies via vcpkg (15-30 minutes)
$HOME/vcpkg/vcpkg install openssl spdlog nlohmann-json \
    boost-system boost-thread rocksdb protobuf gtest

# 3. Build Ubuntu Blockchain (5-10 minutes)
cd /home/user/ubuntu-blockchain
./build.sh

# 4. Run!
./build/bin/ubud --datadir=~/.ubuntu-testnet --testnet
```

---

## 📋 COMPLETE STEP-BY-STEP GUIDE

### Step 1: Check Your System

```bash
# Check you have Ubuntu/Debian
cat /etc/os-release

# Check you have internet
ping -c 3 google.com

# Check you have sudo access
sudo echo "I have sudo!"
```

**If any of these fail, stop and fix first.**

### Step 2: Install Build Tools

```bash
sudo apt update
sudo apt install -y build-essential cmake git

# Verify installation
gcc --version    # Should show GCC 9+ or newer
cmake --version  # Should show CMake 3.10+ or newer
git --version    # Should show Git 2.x
```

### Step 3: Choose Your Installation Method

**Method 1: System Packages (Faster, May Have Version Issues)**
```bash
sudo apt install -y \
    libssl-dev \
    libboost-all-dev \
    librocksdb-dev \
    libprotobuf-dev \
    protobuf-compiler \
    nlohmann-json3-dev \
    libspdlog-dev \
    libgtest-dev

# Note: This may fail if packages aren't available in your repos
```

**Method 2: vcpkg (Slower, More Reliable) - RECOMMENDED**
```bash
# Install vcpkg
cd $HOME
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh

# This takes 5 minutes

# Install all dependencies
./vcpkg install openssl spdlog nlohmann-json \
    boost-system boost-thread rocksdb protobuf

# This takes 15-30 minutes - GO GET COFFEE!

# Set environment variable
echo "export VCPKG_ROOT=$HOME/vcpkg" >> ~/.bashrc
source ~/.bashrc
```

### Step 4: Build Ubuntu Blockchain

```bash
cd /home/user/ubuntu-blockchain

# If using vcpkg
./build.sh

# If using system packages
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -DENABLE_PEG=ON
cmake --build . -j$(nproc)
cd ..
```

**Build time: 5-15 minutes**

You'll see lots of compilation messages. This is normal!

**Success looks like:**
```
[100%] Building CXX object ...
[100%] Linking CXX executable ubud
[100%] Built target ubud
✅ Build successful!
```

**Failure looks like:**
```
CMake Error: Could not find OpenSSL
❌ Build failed
```

If build fails, install missing dependencies and try again.

### Step 5: Verify Build

```bash
# Check executables exist
ls -lh build/bin/

# You should see:
# ubud (blockchain daemon)
# ubu-cli (command line interface)
```

If you see these files, **BUILD SUCCESSFUL!** 🎉

### Step 6: Create Testnet Configuration

```bash
# Create testnet directory
mkdir -p ~/.ubuntu-testnet

# Create config file
cat > ~/.ubuntu-testnet/ubuntu.conf << 'EOF'
# Ubuntu Blockchain Testnet Configuration
testnet=1
datadir=~/.ubuntu-testnet

# Network
port=18333
maxconnections=50

# RPC Server
server=1
rpcport=18332
rpcbind=127.0.0.1
rpcuser=testuser
rpcpassword=testpass123

# Mining (for testnet)
mining=1
miningthreads=2

# Logging
loglevel=info
logtofile=1
debuglogfile=~/.ubuntu-testnet/debug.log
EOF
```

### Step 7: Run Your First Node!

```bash
# Start the blockchain daemon
./build/bin/ubud --datadir=~/.ubuntu-testnet --testnet

# You should see:
# Ubuntu Blockchain Node Starting...
# Loading blockchain from disk...
# P2P server listening on port 18333...
# RPC server listening on port 18332...
```

**🎉 CONGRATULATIONS! Your blockchain is running!**

Leave this terminal open. Open a NEW terminal for next steps.

### Step 8: Create Your First Wallet

In a **NEW terminal**:

```bash
cd /home/user/ubuntu-blockchain

# Generate a new address
./build/bin/ubu-cli --datadir=~/.ubuntu-testnet getnewaddress "my-wallet"

# Output: U1abcd1234... (your first address!)

# Check wallet info
./build/bin/ubu-cli --datadir=~/.ubuntu-testnet getwalletinfo

# Check balance (should be 0.00000000)
./build/bin/ubu-cli --datadir=~/.ubuntu-testnet getbalance
```

### Step 9: Mine Some Test Blocks

```bash
# Mine 101 blocks (needed to make coinbase spendable)
./build/bin/ubu-cli --datadir=~/.ubuntu-testnet generate 101

# This will take a few minutes
# You'll see block hashes being generated

# Check your balance now
./build/bin/ubu-cli --datadir=~/.ubuntu-testnet getbalance

# You should have 5000+ test UBU!
```

**🎊 YOU JUST MINED YOUR FIRST BLOCKS!**

### Step 10: Send Your First Transaction

```bash
# Generate a second address
./build/bin/ubu-cli --datadir=~/.ubuntu-testnet getnewaddress "second-wallet"

# Copy the address (U1xyz...)

# Send 100 test UBU to second address
./build/bin/ubu-cli --datadir=~/.ubuntu-testnet \
    sendtoaddress "U1xyz..." 100.0

# You'll get a transaction ID (txid)

# Mine a block to confirm the transaction
./build/bin/ubu-cli --datadir=~/.ubuntu-testnet generate 1

# Check transaction
./build/bin/ubu-cli --datadir=~/.ubuntu-testnet listtransactions
```

**🚀 YOU JUST SENT YOUR FIRST TRANSACTION!**

---

## 🎓 What You Just Accomplished

✅ **Built a real blockchain from source code**
✅ **Ran your own blockchain node**
✅ **Created cryptocurrency wallets**
✅ **Mined blocks**
✅ **Sent transactions**
✅ **Became a blockchain developer!**

---

## 🧪 More Things to Try

### Check Blockchain Info

```bash
# Get blockchain statistics
./build/bin/ubu-cli --datadir=~/.ubuntu-testnet getblockchaininfo

# Get current block height
./build/bin/ubu-cli --datadir=~/.ubuntu-testnet getblockcount

# Get best block hash
./build/bin/ubu-cli --datadir=~/.ubuntu-testnet getbestblockhash
```

### Explore Blocks

```bash
# Get block at height 1
./build/bin/ubu-cli --datadir=~/.ubuntu-testnet getblock 1

# Get block by hash
./build/bin/ubu-cli --datadir=~/.ubuntu-testnet getblock <blockhash>
```

### Explore Transactions

```bash
# List your transactions
./build/bin/ubu-cli --datadir=~/.ubuntu-testnet listtransactions

# Get specific transaction
./build/bin/ubu-cli --datadir=~/.ubuntu-testnet getrawtransaction <txid> true
```

### Wallet Operations

```bash
# List all your addresses
./build/bin/ubu-cli --datadir=~/.ubuntu-testnet listaddresses

# Backup wallet
./build/bin/ubu-cli --datadir=~/.ubuntu-testnet backupwallet "/tmp/wallet-backup.dat"

# Encrypt wallet (IMPORTANT!)
./build/bin/ubu-cli --datadir=~/.ubuntu-testnet encryptwallet "MyStrongPassword123!"

# After encryption, you'll need to restart the daemon
```

---

## 🐛 Troubleshooting

### Build Fails with "Cannot find OpenSSL"

```bash
# Install OpenSSL development files
sudo apt install -y libssl-dev

# Try build again
./build.sh
```

### Build Fails with "Cannot find Boost"

```bash
# Install Boost development files
sudo apt install -y libboost-all-dev

# Try build again
./build.sh
```

### Daemon Won't Start

```bash
# Check if port is already in use
sudo netstat -tulpn | grep 18332

# Check logs
tail -f ~/.ubuntu-testnet/debug.log

# Try with more verbose logging
./build/bin/ubud --datadir=~/.ubuntu-testnet --testnet --loglevel=debug
```

### CLI Commands Don't Work

```bash
# Make sure daemon is running first
ps aux | grep ubud

# Check RPC connection
./build/bin/ubu-cli --datadir=~/.ubuntu-testnet getblockchaininfo

# If error "Connection refused", daemon isn't running
```

### "Out of Memory" During Build

```bash
# Build with fewer parallel jobs
cd build
cmake --build . -j2  # Instead of -j$(nproc)
```

---

## 📊 Performance Expectations (Testnet)

**On Modern Hardware (4+ cores, 8GB+ RAM):**
- Build time: 5-10 minutes
- Block mining: 10-30 seconds per block
- Transaction processing: <1 second
- Wallet operations: Instant
- Memory usage: 100-300 MB
- Disk usage: <1 GB

**On Older Hardware (2 cores, 4GB RAM):**
- Build time: 15-30 minutes
- Block mining: 30-60 seconds per block
- May experience occasional slowdowns

---

## ⚠️ IMPORTANT REMINDERS

### This is TESTNET ONLY
- ❌ Test UBU has NO real value
- ❌ Don't expect to sell or trade it
- ❌ Don't use for real transactions
- ✅ Use for learning and experimentation only

### Safety Guidelines
- ✅ Run on testnet only
- ✅ Never give out real passwords/keys
- ✅ Keep daemon on localhost (127.0.0.1)
- ✅ Don't expose RPC to internet
- ✅ This is for testing only!

### What This IS
- ✅ Real blockchain you can learn from
- ✅ Safe environment to experiment
- ✅ Great for understanding blockchain technology
- ✅ Perfect for developer training

### What This IS NOT
- ❌ Production-ready system
- ❌ Ready for real money
- ❌ Suitable for public deployment
- ❌ Financially valuable (testnet)

---

## 📢 Sharing Your Experience

### ✅ What You CAN Say

**Good:**
> "I just built and ran Ubuntu Blockchain testnet! 🌍 It's a working blockchain prototype made in Africa. Currently testnet-only but exciting to see African blockchain development. #UbuntuBlockchain #MadeInAfrica"

> "Testing Ubuntu Blockchain - Africa's indigenous blockchain project. Mined my first blocks! This is testnet only but great learning experience. Looking forward to seeing this evolve over next few years."

### ❌ What You Shouldn't Say

**Bad:**
> "Ubuntu Blockchain is live! Start using it for payments!" ❌
> "Better than Bitcoin, ready for production!" ❌
> "Invest now!" ❌
> "African countries are adopting this!" ❌

**Why:** False claims damage credibility and could be illegal.

---

## 🎯 Next Steps

### Today (You Can Do This NOW)
- ✅ Share your testnet experience honestly
- ✅ Invite developer friends to try it
- ✅ Join the community discussions
- ✅ Report any bugs you find

### This Week
- Test more features
- Write down feedback
- Experiment with RPC API
- Try building simple applications

### This Month
- Contribute to documentation
- Write test cases
- Suggest improvements
- Help onboard other developers

### This Year
- Help build production-ready features
- Participate in security audits
- Contribute to roadmap
- Build African blockchain community

---

## 📧 Stay Connected

**Report Issues:**
- Email: dev@ubuntublockchain.xyz
- GitHub: https://github.com/EmekaIwuagwu/ubuntu-blockchain

**Ask Questions:**
- Discord: [Coming soon]
- Email: info@ubuntublockchain.xyz

**Contribute:**
- Fork on GitHub
- Submit pull requests
- Write documentation
- Share knowledge

---

## 🙏 Thank You!

You just ran Africa's first indigenous blockchain!

Whether it becomes Africa's reserve cryptocurrency or not, you're part of the movement showing that:

✅ Africans can build world-class technology
✅ Africa doesn't need to wait for others
✅ African solutions to African problems work
✅ We can control our own technological destiny

**Ubuntu: "I am because we are"**

Together, we build Africa's digital future! 🌍

---

🌍 **Built in Africa. Secured by Africans. Scaled for African prosperity.**

⛓️ **Ubuntu Blockchain - Africa's Reserve Cryptocurrency (In Development)**

**Status: TESTNET RUNNING ✅ | Production: 5-8 years away ⏳**
