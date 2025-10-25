# Ubuntu Blockchain Deployment Guide

This guide covers deploying Ubuntu Blockchain in production environments.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Installation Methods](#installation-methods)
- [Configuration](#configuration)
- [Running the Node](#running-the-node)
- [Monitoring](#monitoring)
- [Backup and Recovery](#backup-and-recovery)
- [Security](#security)
- [Troubleshooting](#troubleshooting)

## Prerequisites

### Hardware Requirements

**Minimum:**
- CPU: 4 cores
- RAM: 8 GB
- Storage: 500 GB SSD
- Network: 100 Mbps

**Recommended:**
- CPU: 8+ cores
- RAM: 16+ GB
- Storage: 1+ TB NVMe SSD
- Network: 1 Gbps

### Software Requirements

- Ubuntu 22.04 LTS or later
- Docker 20.10+ (for Docker deployment)
- systemd (for service deployment)

## Installation Methods

### Method 1: Docker (Recommended)

**Quick Start:**
```bash
# Clone repository
git clone https://github.com/UbuntuBlockchain/ubuntu-blockchain.git
cd ubuntu-blockchain

# Create configuration
cp ubuntu.conf.example ubuntu.conf
nano ubuntu.conf  # Edit configuration

# Start with Docker Compose
docker-compose up -d ubud

# Check status
docker-compose logs -f ubud
```

**Production Deployment:**
```bash
# Build image
docker build -t ubuntu-blockchain:latest .

# Run container
docker run -d \
  --name ubuntu-blockchain-node \
  -p 8333:8333 \
  -p 8332:8332 \
  -v /var/lib/ubuntu-blockchain:/var/lib/ubuntu-blockchain \
  -v $(pwd)/ubuntu.conf:/etc/ubuntu-blockchain/ubuntu.conf:ro \
  --restart unless-stopped \
  ubuntu-blockchain:latest
```

### Method 2: Systemd Service

**Build from Source:**
```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake git \
  libssl-dev libboost-all-dev \
  libprotobuf-dev protobuf-compiler

# Clone and build
git clone https://github.com/UbuntuBlockchain/ubuntu-blockchain.git
cd ubuntu-blockchain
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Install binaries
sudo cp build/bin/ubud /usr/local/bin/
sudo cp build/bin/ubu-cli /usr/local/bin/
```

**Install Service:**
```bash
# Create user
sudo useradd -r -m -d /var/lib/ubuntu-blockchain ubuntu-blockchain

# Create directories
sudo mkdir -p /var/lib/ubuntu-blockchain
sudo mkdir -p /etc/ubuntu-blockchain
sudo chown -R ubuntu-blockchain:ubuntu-blockchain /var/lib/ubuntu-blockchain

# Copy configuration
sudo cp ubuntu.conf.example /etc/ubuntu-blockchain/ubuntu.conf
sudo nano /etc/ubuntu-blockchain/ubuntu.conf

# Install systemd service
sudo cp contrib/systemd/ubud.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable ubud
sudo systemctl start ubud
```

## Configuration

### Essential Settings

Edit `/etc/ubuntu-blockchain/ubuntu.conf`:

```ini
# Network
port=8333
maxconnections=125

# RPC
server=1
rpcport=8332
rpcbind=127.0.0.1
rpcuser=your_username
rpcpassword=STRONG_PASSWORD_HERE

# Data
datadir=/var/lib/ubuntu-blockchain
dbcache=4096  # MB of cache

# Logging
loglevel=info
logtofile=1
```

### Security Configuration

```ini
# Bind RPC to localhost only
rpcbind=127.0.0.1

# Strong authentication
rpcuser=admin
rpcpassword=$(openssl rand -base64 32)

# Firewall - Allow only necessary ports
# UFW example:
# sudo ufw allow 8333/tcp  # P2P
# sudo ufw enable
```

## Running the Node

### Start Node
```bash
# Docker
docker-compose up -d ubud

# Systemd
sudo systemctl start ubud

# Manual
ubud -daemon
```

### Check Status
```bash
# Using CLI
ubu-cli getblockchaininfo

# Check logs (systemd)
sudo journalctl -u ubud -f

# Check logs (Docker)
docker logs -f ubuntu-blockchain-node
```

### Stop Node
```bash
# Graceful shutdown
ubu-cli stop

# Force stop (Docker)
docker-compose down

# Force stop (systemd)
sudo systemctl stop ubud
```

## Monitoring

### Built-in Metrics

Access Prometheus-format metrics:
```bash
curl http://localhost:9090/metrics
```

### Health Checks

```bash
# Check if node is running
ubu-cli getblockchaininfo

# Check peer count
ubu-cli getconnectioncount

# Check mempool
ubu-cli getmempoolinfo

# Check sync status
ubu-cli getblockcount
```

### Prometheus + Grafana

Use the provided docker-compose.yml:
```bash
# Start full monitoring stack
docker-compose up -d

# Access Grafana
# URL: http://localhost:3000
# User: admin
# Password: ubuntu-blockchain
```

## Backup and Recovery

### Backup Strategy

**What to Backup:**
1. Wallet file: `/var/lib/ubuntu-blockchain/wallet.dat`
2. Configuration: `/etc/ubuntu-blockchain/ubuntu.conf`
3. (Optional) Blockchain data for faster recovery

**Backup Commands:**
```bash
# Stop node
ubu-cli stop

# Backup wallet
cp /var/lib/ubuntu-blockchain/wallet.dat ~/wallet-backup-$(date +%Y%m%d).dat

# Backup configuration
cp /etc/ubuntu-blockchain/ubuntu.conf ~/ubuntu-conf-backup.conf

# Optional: Backup entire blockchain
tar -czf ~/blockchain-backup.tar.gz /var/lib/ubuntu-blockchain

# Restart node
systemctl start ubud
```

### Recovery

```bash
# Stop node
systemctl stop ubud

# Restore wallet
cp ~/wallet-backup.dat /var/lib/ubuntu-blockchain/wallet.dat
chown ubuntu-blockchain:ubuntu-blockchain /var/lib/ubuntu-blockchain/wallet.dat

# Restore configuration
cp ~/ubuntu-conf-backup.conf /etc/ubuntu-blockchain/ubuntu.conf

# Restart node
systemctl start ubud

# Rescan blockchain (if needed)
ubud -rescan
```

## Security

### Firewall Configuration

```bash
# UFW (Ubuntu)
sudo ufw default deny incoming
sudo ufw default allow outgoing
sudo ufw allow 8333/tcp  # P2P
sudo ufw allow from 127.0.0.1 to any port 8332  # RPC (localhost only)
sudo ufw enable

# iptables
sudo iptables -A INPUT -p tcp --dport 8333 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 8332 -s 127.0.0.1 -j ACCEPT
```

### SSL/TLS for RPC

Use nginx or similar reverse proxy:
```nginx
server {
    listen 443 ssl;
    server_name node.example.com;

    ssl_certificate /etc/ssl/certs/ubuntu-blockchain.crt;
    ssl_certificate_key /etc/ssl/private/ubuntu-blockchain.key;

    location / {
        proxy_pass http://127.0.0.1:8332;
        proxy_set_header Host $host;
        auth_basic "RPC Access";
        auth_basic_user_file /etc/nginx/.htpasswd;
    }
}
```

### Regular Updates

```bash
# Pull latest code
cd ubuntu-blockchain
git pull origin main

# Rebuild
cmake --build build

# Restart
sudo systemctl restart ubud
```

## Troubleshooting

### Node Won't Start

**Check logs:**
```bash
sudo journalctl -u ubud -n 100
```

**Common issues:**
- Port already in use: Change port in config
- Permission denied: Check file ownership
- Database corrupted: Delete chainstate, resync

### No Peer Connections

```bash
# Check firewall
sudo ufw status

# Check network
ubu-cli getnetworkinfo

# Add seed nodes manually
# Edit ubuntu.conf:
addnode=seed1.ubuntu-blockchain.com:8333
```

### High Memory Usage

```bash
# Reduce cache size in ubuntu.conf
dbcache=2048  # Reduce from 4096

# Restart node
sudo systemctl restart ubud
```

### Slow Sync

```bash
# Check block count
ubu-cli getblockcount

# Check peers
ubu-cli getconnectioncount

# Add more connections
# Edit ubuntu.conf:
maxconnections=200
```

### Database Corruption

```bash
# Stop node
ubu-cli stop

# Backup wallet
cp /var/lib/ubuntu-blockchain/wallet.dat ~/wallet-safe.dat

# Remove chainstate
rm -rf /var/lib/ubuntu-blockchain/chainstate

# Restart (will resync)
systemctl start ubud
```

## Performance Tuning

### System Optimization

```bash
# Increase file descriptors
echo "ubuntu-blockchain soft nofile 65536" | sudo tee -a /etc/security/limits.conf
echo "ubuntu-blockchain hard nofile 65536" | sudo tee -a /etc/security/limits.conf

# Optimize I/O scheduler for SSD
echo "deadline" | sudo tee /sys/block/sda/queue/scheduler
```

### Node Configuration

```ini
# ubuntu.conf optimizations
dbcache=8192        # Increase cache (if RAM available)
maxconnections=200  # More peers
par=8              # Script verification threads
```

## Support

- Documentation: https://ubuntu-blockchain.org/docs
- GitHub Issues: https://github.com/UbuntuBlockchain/ubuntu-blockchain/issues
- Discord: https://discord.gg/ubuntu-blockchain
- Email: support@ubuntu-blockchain.org

---

For additional help, see [API.md](API.md) and [CONTRIBUTING.md](CONTRIBUTING.md).
