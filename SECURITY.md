# Security Policy

## Table of Contents

- [Reporting Security Vulnerabilities](#reporting-security-vulnerabilities)
- [Security Best Practices](#security-best-practices)
- [Network Security](#network-security)
- [Wallet Security](#wallet-security)
- [RPC Security](#rpc-security)
- [Node Hardening](#node-hardening)
- [Cryptographic Security](#cryptographic-security)
- [Operational Security](#operational-security)
- [Security Audits](#security-audits)

---

## Reporting Security Vulnerabilities

**CRITICAL**: Do NOT report security vulnerabilities through public GitHub issues.

### Responsible Disclosure

If you discover a security vulnerability, please follow these steps:

1. **Email**: Send details to security@ubuntu-blockchain.com
2. **Encrypt**: Use our PGP key (available at https://ubuntu-blockchain.com/security-pgp.txt)
3. **Include**:
   - Description of the vulnerability
   - Steps to reproduce
   - Potential impact
   - Suggested fix (if available)
4. **Timeline**: We aim to respond within 48 hours

### What to Expect

- **Acknowledgment**: We'll confirm receipt within 48 hours
- **Assessment**: We'll assess the severity and impact
- **Fix**: We'll develop and test a patch
- **Disclosure**: We'll coordinate public disclosure
- **Credit**: We'll acknowledge your contribution (if desired)

### Bug Bounty Program

We offer rewards for responsibly disclosed vulnerabilities:

| Severity | Reward Range |
|----------|--------------|
| Critical | $5,000 - $10,000 UBU |
| High | $2,000 - $5,000 UBU |
| Medium | $500 - $2,000 UBU |
| Low | $100 - $500 UBU |

---

## Security Best Practices

### General Principles

1. **Defense in Depth**: Multiple layers of security
2. **Least Privilege**: Minimal necessary permissions
3. **Fail Securely**: Errors should not compromise security
4. **Keep Software Updated**: Apply security patches promptly
5. **Regular Backups**: Maintain encrypted backups
6. **Monitoring**: Enable logging and monitoring

---

## Network Security

### Firewall Configuration

#### Recommended Firewall Rules

```bash
# Allow P2P connections (port 8333)
sudo ufw allow 8333/tcp comment 'Ubuntu Blockchain P2P'

# Allow RPC only from localhost (default)
# DO NOT expose RPC port 8332 to the internet

# For testnet
sudo ufw allow 18333/tcp comment 'Ubuntu Blockchain Testnet P2P'

# Enable firewall
sudo ufw enable
```

#### Production Firewall Rules

```bash
# Allow P2P from specific IPs only
sudo ufw allow from 192.168.1.0/24 to any port 8333 proto tcp

# Deny all other incoming by default
sudo ufw default deny incoming
sudo ufw default allow outgoing
```

### Network Isolation

```ini
# ubuntu.conf

# Bind RPC to localhost only (IMPORTANT!)
rpcbind=127.0.0.1

# Bind P2P to specific interface
bind=192.168.1.100

# Whitelist specific peers only
addnode=trusted-peer1.example.com
addnode=trusted-peer2.example.com

# Limit number of connections
maxconnections=125
```

### DDoS Protection

```ini
# Rate limiting
bantime=86400
banscore=100

# Connection limits per IP
maxconnections=125
maxoutbound=8

# Enable peer filtering
peerbloomfilters=0
```

### TLS/SSL Configuration

```ini
# Enable RPC over TLS (production)
rpcssl=1
rpcsslcertificatechainfile=/etc/ssl/certs/ubud.crt
rpcsslprivatekeyfile=/etc/ssl/private/ubud.key
rpcsslciphers=TLSv1.2:TLSv1.3
```

---

## Wallet Security

### Mnemonic Seed Protection

**CRITICAL**: Your 24-word mnemonic seed is the master key to all funds.

#### Storage Best Practices

1. **Write it Down**: Use pen and paper, never digital
2. **Multiple Copies**: Store in 2-3 secure locations
3. **Secure Locations**:
   - Bank safe deposit box
   - Home safe (fireproof, waterproof)
   - Trusted family member
4. **Never Share**: NEVER share your seed with anyone
5. **Avoid Digital**: No photos, screenshots, or cloud storage
6. **Verify**: Check spelling and order carefully

#### Advanced: Shamir Secret Sharing (Future Enhancement)

```bash
# Split seed into 3 shares, require 2 to recover
ubu-cli shamirsplit --threshold 2 --shares 3

# Distribute shares to different locations
```

### Wallet Encryption

```bash
# Encrypt wallet with strong passphrase
./ubu-cli encryptwallet "STRONG-PASSPHRASE-HERE"

# Restart daemon after encryption
sudo systemctl restart ubud

# Unlock for transactions (300 seconds)
./ubu-cli walletpassphrase "STRONG-PASSPHRASE-HERE" 300

# Lock wallet immediately
./ubu-cli walletlock
```

#### Strong Passphrase Guidelines

- **Length**: Minimum 20 characters
- **Complexity**: Mix of uppercase, lowercase, numbers, symbols
- **Uniqueness**: Don't reuse passwords
- **Randomness**: Use password manager or dice ware

**Example Strong Passphrase**:
```
correct-horse-battery-staple-7$mountain-whisper
```

### Wallet Backup

```bash
# Backup wallet
./ubu-cli backupwallet "/secure/location/wallet-backup-$(date +%Y%m%d).dat"

# Encrypt backup (highly recommended)
gpg --symmetric --cipher-algo AES256 wallet-backup-20231101.dat

# Verify backup
gpg --decrypt wallet-backup-20231101.dat.gpg > /tmp/test-restore.dat
./ubu-cli verifybackup /tmp/test-restore.dat
rm /tmp/test-restore.dat
```

### Cold Storage (Offline Wallet)

For maximum security, keep funds in cold storage:

1. **Air-Gapped Machine**: Never connected to internet
2. **Generate Address Offline**:
   ```bash
   # On offline machine
   ./ubu-cli getnewaddress "cold-storage"
   ```
3. **Sign Transactions Offline**:
   ```bash
   # Online: Create unsigned transaction
   ./ubu-cli createrawtransaction '[...]' '{...}'

   # Offline: Sign transaction
   ./ubu-cli signrawtransaction "<hex>"

   # Online: Broadcast signed transaction
   ./ubu-cli sendrawtransaction "<signed-hex>"
   ```

### Watch-Only Wallets

Monitor addresses without private keys:

```bash
# Import address as watch-only
./ubu-cli importaddress "U1address" "label" false

# Check balance
./ubu-cli getbalance "*" 1 true
```

---

## RPC Security

### Authentication

**NEVER** run RPC without authentication in production.

```ini
# ubuntu.conf

# Strong credentials
rpcuser=ubuntu_$(openssl rand -hex 16)
rpcpassword=$(openssl rand -base64 32)

# Bind to localhost only
rpcbind=127.0.0.1

# Limit RPC methods (if needed)
# rpcallowmethod=getblockchaininfo
# rpcallowmethod=getbalance
```

### IP Whitelisting

```ini
# Allow RPC only from specific IPs
rpcallowip=127.0.0.1
rpcallowip=192.168.1.0/24

# Deny all others (implicit)
```

### RPC Over SSH Tunnel

For remote RPC access, use SSH tunneling:

```bash
# On local machine
ssh -L 8332:localhost:8332 user@remote-node

# Now use RPC locally
./ubu-cli -rpcconnect=127.0.0.1 getblockchaininfo
```

### Rate Limiting

```ini
# Limit RPC requests
rpcthreads=4
rpcworkqueue=16

# Client timeout
rpcclienttimeout=30
```

### RPC Authentication Tokens

```bash
# Generate auth token
./share/rpcauth/rpcauth.py ubuntu
# Use output in ubuntu.conf:
# rpcauth=ubuntu:7e24a...
```

---

## Node Hardening

### Operating System Hardening

#### User Isolation

```bash
# Create dedicated user
sudo useradd -r -m -s /bin/bash ubuntu-blockchain

# Limit permissions
sudo chmod 700 /home/ubuntu-blockchain
sudo chown -R ubuntu-blockchain:ubuntu-blockchain /var/lib/ubuntu-blockchain
```

#### systemd Security Features

```ini
# /etc/systemd/system/ubud.service

[Service]
# Run as dedicated user
User=ubuntu-blockchain
Group=ubuntu-blockchain

# Security hardening
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=full
ProtectHome=true
ReadWritePaths=/var/lib/ubuntu-blockchain
ProtectKernelTunables=true
ProtectControlGroups=true
RestrictRealtime=true
RestrictNamespaces=true
LockPersonality=true
MemoryDenyWriteExecute=true

# Resource limits
LimitNOFILE=65536
LimitNPROC=512
```

#### File System Permissions

```bash
# Restrict data directory
sudo chmod 700 /var/lib/ubuntu-blockchain

# Restrict config file
sudo chmod 600 /etc/ubuntu-blockchain/ubuntu.conf

# Verify permissions
sudo find /var/lib/ubuntu-blockchain -type d -exec chmod 700 {} \;
sudo find /var/lib/ubuntu-blockchain -type f -exec chmod 600 {} \;
```

### Dependency Security

```bash
# Keep dependencies updated
sudo apt update && sudo apt upgrade

# Enable automatic security updates (Ubuntu)
sudo apt install unattended-upgrades
sudo dpkg-reconfigure -plow unattended-upgrades

# Audit dependencies
pip install safety
safety check
```

### Logging and Monitoring

```ini
# ubuntu.conf

# Enable detailed logging
debug=net
debug=mempool
debug=rpc

# Log file location
debuglogfile=/var/log/ubuntu-blockchain/debug.log

# Enable metrics
metrics=1
metricsport=9090
```

#### Monitor Logs

```bash
# Watch logs in real-time
tail -f /var/log/ubuntu-blockchain/debug.log

# Search for errors
grep ERROR /var/log/ubuntu-blockchain/debug.log

# Monitor with fail2ban
sudo apt install fail2ban
# Configure fail2ban for RPC brute force attempts
```

---

## Cryptographic Security

### Key Generation

- **Entropy**: Use /dev/urandom (Linux) or CryptGenRandom (Windows)
- **CSPRNG**: Cryptographically secure pseudo-random number generator
- **BIP-39**: 24-word mnemonic for maximum security (256-bit entropy)

### Signature Verification

Always verify signatures before accepting transactions:

```cpp
// Verify ECDSA signature
ECDSASignature sig = ECDSASignature::fromDER(sigDER);
if (!sig.verify(pubKey, messageHash)) {
    // Reject invalid signature
    throw std::runtime_error("Invalid signature");
}
```

### Hash Functions

- **SHA-256**: Primary hash function (double-SHA256 for block hashes)
- **RIPEMD-160**: Address generation
- **No MD5/SHA-1**: Never use deprecated hash functions

### Constant-Time Comparisons

```cpp
// Use constant-time comparison for sensitive data
bool isEqual = CRYPTO_memcmp(a.data(), b.data(), 32) == 0;

// NOT: a == b (vulnerable to timing attacks)
```

---

## Operational Security

### Multi-Signature (Future Enhancement)

For high-value accounts, use multi-signature:

```bash
# Create 2-of-3 multisig address
./ubu-cli createmultisig 2 '["pubkey1","pubkey2","pubkey3"]'
```

### Regular Security Audits

#### Self-Audit Checklist

- [ ] RPC not exposed to internet
- [ ] Wallet encrypted with strong passphrase
- [ ] Mnemonic seed backed up securely
- [ ] Firewall rules configured
- [ ] Software up to date
- [ ] Logs monitored regularly
- [ ] Backups tested
- [ ] systemd hardening enabled
- [ ] File permissions restrictive
- [ ] Strong RPC credentials

#### Professional Audits

- **Code Audits**: Third-party security review
- **Penetration Testing**: Simulated attacks
- **Dependency Scanning**: Automated vulnerability detection

### Incident Response Plan

1. **Detection**: Monitor for unusual activity
2. **Containment**: Isolate affected systems
3. **Investigation**: Determine scope and impact
4. **Recovery**: Restore from secure backups
5. **Post-Mortem**: Document and improve

---

## Security Audits

### Automated Security Scanning

```bash
# Static analysis
cppcheck --enable=all src/

# Dependency vulnerabilities
npm audit  # for Node.js dependencies
pip-audit  # for Python dependencies

# Docker image scanning
docker scan ubuntu-blockchain:latest
```

### Code Review Guidelines

- All code must be reviewed by 2+ developers
- Focus on:
  - Input validation
  - Buffer overflows
  - Integer overflows
  - Race conditions
  - Cryptographic misuse
  - Authentication bypasses

### Third-Party Security Audits

We welcome security audits from:

- Academic institutions
- Security research firms
- Independent security researchers

Contact: security@ubuntu-blockchain.com

---

## Security Updates

### Update Policy

- **Critical**: Patched within 24 hours
- **High**: Patched within 1 week
- **Medium**: Patched within 1 month
- **Low**: Patched in next regular release

### Subscribing to Security Announcements

- **Mailing List**: security-announce@ubuntu-blockchain.com
- **RSS Feed**: https://ubuntu-blockchain.com/security/feed
- **GitHub Security Advisories**: Watch repository

---

## Compliance

### Regulatory Considerations

Depending on your jurisdiction, you may need to comply with:

- **AML/KYC**: Anti-Money Laundering / Know Your Customer
- **GDPR**: General Data Protection Regulation (EU)
- **SOC 2**: Service Organization Control 2
- **PCI DSS**: Payment Card Industry Data Security Standard

Consult legal counsel for compliance requirements.

---

## Security Resources

### Official Resources

- **Website**: https://ubuntu-blockchain.com/security
- **Security Guide**: https://docs.ubuntu-blockchain.com/security
- **CVE List**: https://ubuntu-blockchain.com/security/cve

### External Resources

- [OWASP Cryptographic Storage Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Cryptographic_Storage_Cheat_Sheet.html)
- [CIS Benchmarks](https://www.cisecurity.org/cis-benchmarks/)
- [NIST Cybersecurity Framework](https://www.nist.gov/cyberframework)

---

## Acknowledgments

We thank the security research community for their contributions to blockchain security. Responsible disclosures help make Ubuntu Blockchain safer for everyone.

### Hall of Fame

- [Your Name Here] - Submit responsible disclosure to be listed!

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2023-11-01 | Initial security policy |

---

**Remember**: Security is a process, not a product. Stay vigilant, keep learning, and always question assumptions.

For questions or concerns, contact: security@ubuntu-blockchain.com
