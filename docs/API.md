# Ubuntu Blockchain API Reference

This document provides comprehensive documentation for all RPC methods available in the Ubuntu Blockchain daemon (ubud).

## Table of Contents

- [Authentication](#authentication)
- [JSON-RPC 2.0 Format](#json-rpc-20-format)
- [Error Codes](#error-codes)
- [Blockchain RPC Methods](#blockchain-rpc-methods)
- [Wallet RPC Methods](#wallet-rpc-methods)
- [Network RPC Methods](#network-rpc-methods)
- [System RPC Methods](#system-rpc-methods)

## Authentication

The RPC server supports HTTP Basic Authentication. Configure credentials in `ubuntu.conf`:

```ini
rpcuser=your_username
rpcpassword=your_secure_password
```

Example authenticated request:
```bash
curl --user your_username:your_secure_password \
  --data-binary '{"jsonrpc":"2.0","id":"1","method":"getblockchaininfo","params":[]}' \
  http://127.0.0.1:8332/
```

## JSON-RPC 2.0 Format

All requests and responses follow the JSON-RPC 2.0 specification.

### Request Format
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "method_name",
  "params": []
}
```

### Success Response
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": { ... }
}
```

### Error Response
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "error": {
    "code": -32601,
    "message": "Method not found"
  }
}
```

## Error Codes

| Code | Message | Description |
|------|---------|-------------|
| -32700 | Parse error | Invalid JSON |
| -32600 | Invalid request | Invalid JSON-RPC request |
| -32601 | Method not found | Method does not exist |
| -32602 | Invalid params | Invalid method parameters |
| -32603 | Internal error | Internal server error |
| -1 | Blockchain error | General blockchain error |
| -2 | Transaction error | Transaction validation failed |
| -3 | Wallet error | Wallet operation failed |
| -4 | Network error | Network operation failed |
| -5 | Database error | Database operation failed |

---

# Blockchain RPC Methods

## getblockchaininfo

Returns comprehensive information about the blockchain state.

### Parameters
None

### Returns
```json
{
  "chain": "main",
  "blocks": 12345,
  "headers": 12345,
  "bestblockhash": "0000000000000000000a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w",
  "difficulty": 15000000000000.5,
  "mediantime": 1698765432,
  "chainwork": "00000000000000000000000000000000000000001234567890abcdef",
  "initialblockdownload": false,
  "pruned": false
}
```

### Example
```bash
./ubu-cli getblockchaininfo
```

---

## getblockcount

Returns the current block height.

### Parameters
None

### Returns
```json
12345
```

### Example
```bash
./ubu-cli getblockcount
```

---

## getblockhash

Returns the block hash for a given height.

### Parameters
1. `height` (number, required) - Block height

### Returns
```json
"0000000000000000000a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w"
```

### Example
```bash
./ubu-cli getblockhash 12345
```

---

## getblock

Returns detailed block information.

### Parameters
1. `hash` (string, required) - Block hash
2. `verbosity` (number, optional, default=1) - 0=hex, 1=json, 2=json with tx details

### Returns (verbosity=1)
```json
{
  "hash": "0000000000000000000a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w",
  "confirmations": 100,
  "height": 12345,
  "version": 1,
  "merkleroot": "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890",
  "time": 1698765432,
  "nonce": 987654321,
  "bits": "1a2b3c4d",
  "difficulty": 15000000000000.5,
  "previousblockhash": "0000000000000000000b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x",
  "nextblockhash": "0000000000000000000c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x4y",
  "tx": [
    "tx_hash_1",
    "tx_hash_2"
  ],
  "size": 285,
  "weight": 1140
}
```

### Example
```bash
./ubu-cli getblock "0000000000000000000a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w"
./ubu-cli getblock "0000000000000000000a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w" 2
```

---

## getblockheader

Returns block header information.

### Parameters
1. `hash` (string, required) - Block hash
2. `verbose` (boolean, optional, default=true) - true for JSON, false for hex

### Returns
```json
{
  "hash": "0000000000000000000a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w",
  "confirmations": 100,
  "height": 12345,
  "version": 1,
  "merkleroot": "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890",
  "time": 1698765432,
  "nonce": 987654321,
  "bits": "1a2b3c4d",
  "difficulty": 15000000000000.5,
  "previousblockhash": "0000000000000000000b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x"
}
```

### Example
```bash
./ubu-cli getblockheader "0000000000000000000a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w"
```

---

## getchaintips

Returns information about all known chain tips.

### Parameters
None

### Returns
```json
[
  {
    "height": 12345,
    "hash": "0000000000000000000a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w",
    "branchlen": 0,
    "status": "active"
  },
  {
    "height": 12340,
    "hash": "0000000000000000000b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x",
    "branchlen": 5,
    "status": "valid-fork"
  }
]
```

### Example
```bash
./ubu-cli getchaintips
```

---

## getdifficulty

Returns the current proof-of-work difficulty.

### Parameters
None

### Returns
```json
15000000000000.5
```

### Example
```bash
./ubu-cli getdifficulty
```

---

## getbestblockhash

Returns the hash of the best (tip) block.

### Parameters
None

### Returns
```json
"0000000000000000000a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w"
```

### Example
```bash
./ubu-cli getbestblockhash
```

---

## gettxout

Returns details about an unspent transaction output (UTXO).

### Parameters
1. `txid` (string, required) - Transaction ID
2. `vout` (number, required) - Output index
3. `includemempool` (boolean, optional, default=true) - Include mempool transactions

### Returns
```json
{
  "bestblock": "0000000000000000000a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w",
  "confirmations": 100,
  "value": 50.00000000,
  "scriptPubKey": {
    "asm": "OP_DUP OP_HASH160 abcdef1234567890 OP_EQUALVERIFY OP_CHECKSIG",
    "hex": "76a914abcdef123456789088ac",
    "type": "pubkeyhash",
    "address": "U1qp8r4xktdg8vdawme73kfr5eg0h9j4u2xgz3la0"
  },
  "coinbase": false
}
```

### Example
```bash
./ubu-cli gettxout "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890" 0
```

---

## gettxoutsetinfo

Returns statistics about the UTXO set.

### Parameters
None

### Returns
```json
{
  "height": 12345,
  "bestblock": "0000000000000000000a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w",
  "transactions": 54321,
  "txouts": 123456,
  "total_amount": 1000000000.00000000,
  "disk_size": 5678901234
}
```

### Example
```bash
./ubu-cli gettxoutsetinfo
```

---

## getrawtransaction

Returns raw transaction data.

### Parameters
1. `txid` (string, required) - Transaction ID
2. `verbose` (boolean, optional, default=false) - true for JSON, false for hex

### Returns (verbose=true)
```json
{
  "txid": "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890",
  "hash": "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890",
  "version": 1,
  "size": 225,
  "locktime": 0,
  "vin": [
    {
      "txid": "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef",
      "vout": 0,
      "scriptSig": {
        "asm": "304502...",
        "hex": "48304502..."
      },
      "sequence": 4294967295
    }
  ],
  "vout": [
    {
      "value": 50.00000000,
      "n": 0,
      "scriptPubKey": {
        "asm": "OP_DUP OP_HASH160 abcdef1234567890 OP_EQUALVERIFY OP_CHECKSIG",
        "hex": "76a914abcdef123456789088ac",
        "type": "pubkeyhash",
        "address": "U1qp8r4xktdg8vdawme73kfr5eg0h9j4u2xgz3la0"
      }
    }
  ],
  "blockhash": "0000000000000000000a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w",
  "confirmations": 100,
  "time": 1698765432,
  "blocktime": 1698765432
}
```

### Example
```bash
./ubu-cli getrawtransaction "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890" true
```

---

## sendrawtransaction

Submits a raw transaction to the network.

### Parameters
1. `hexstring` (string, required) - Raw transaction hex
2. `maxfeerate` (number, optional, default=0.10) - Maximum fee rate

### Returns
```json
"abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"
```

### Example
```bash
./ubu-cli sendrawtransaction "01000000..."
```

---

## getmempoolinfo

Returns mempool statistics.

### Parameters
None

### Returns
```json
{
  "size": 4321,
  "bytes": 987654,
  "usage": 5000000,
  "maxmempool": 300000000,
  "mempoolminfee": 0.00001000
}
```

### Example
```bash
./ubu-cli getmempoolinfo
```

---

## getrawmempool

Returns all transaction IDs in the mempool.

### Parameters
1. `verbose` (boolean, optional, default=false) - true for detailed info

### Returns (verbose=false)
```json
[
  "txid1",
  "txid2",
  "txid3"
]
```

### Returns (verbose=true)
```json
{
  "txid1": {
    "size": 225,
    "fee": 0.00001000,
    "time": 1698765432,
    "height": 12345,
    "descendantcount": 1,
    "descendantsize": 225,
    "descendantfees": 1000
  }
}
```

### Example
```bash
./ubu-cli getrawmempool
./ubu-cli getrawmempool true
```

---

## estimatefee

Estimates the fee rate for transaction confirmation.

### Parameters
1. `nblocks` (number, required) - Number of blocks for confirmation target

### Returns
```json
0.00001500
```

### Example
```bash
./ubu-cli estimatefee 6
```

---

## validateaddress

Validates a blockchain address.

### Parameters
1. `address` (string, required) - Address to validate

### Returns
```json
{
  "isvalid": true,
  "address": "U1qp8r4xktdg8vdawme73kfr5eg0h9j4u2xgz3la0",
  "scriptPubKey": "76a914abcdef123456789088ac",
  "isscript": false
}
```

### Example
```bash
./ubu-cli validateaddress "U1qp8r4xktdg8vdawme73kfr5eg0h9j4u2xgz3la0"
```

---

## verifychain

Verifies the blockchain database.

### Parameters
1. `checklevel` (number, optional, default=3) - Verification level (0-4)
2. `nblocks` (number, optional, default=6) - Number of blocks to check

### Returns
```json
true
```

### Example
```bash
./ubu-cli verifychain 3 1000
```

---

## getmininginfo

Returns mining-related information.

### Parameters
None

### Returns
```json
{
  "blocks": 12345,
  "currentblocksize": 1000,
  "currentblocktx": 10,
  "difficulty": 15000000000000.5,
  "networkhashps": 123456789012345.0,
  "pooledtx": 4321,
  "chain": "main"
}
```

### Example
```bash
./ubu-cli getmininginfo
```

---

# Wallet RPC Methods

## getnewaddress

Generates a new receiving address.

### Parameters
1. `label` (string, optional, default="") - Address label
2. `address_type` (string, optional, default="bech32") - "bech32" or "legacy"

### Returns
```json
"U1qp8r4xktdg8vdawme73kfr5eg0h9j4u2xgz3la0"
```

### Example
```bash
./ubu-cli getnewaddress
./ubu-cli getnewaddress "my-label"
./ubu-cli getnewaddress "legacy-addr" "legacy"
```

---

## getbalance

Returns the wallet balance.

### Parameters
1. `minconf` (number, optional, default=1) - Minimum confirmations
2. `include_watchonly` (boolean, optional, default=false) - Include watch-only addresses

### Returns
```json
{
  "confirmed": 1000.00000000,
  "unconfirmed": 50.00000000,
  "immature": 100.00000000,
  "total": 1150.00000000
}
```

### Example
```bash
./ubu-cli getbalance
./ubu-cli getbalance 6
```

---

## sendtoaddress

Sends UBU to an address.

### Parameters
1. `address` (string, required) - Destination address
2. `amount` (number, required) - Amount in UBU
3. `comment` (string, optional) - Transaction comment
4. `comment_to` (string, optional) - Recipient comment
5. `subtractfeefromamount` (boolean, optional, default=false) - Subtract fee from amount

### Returns
```json
"abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"
```

### Example
```bash
./ubu-cli sendtoaddress "U1qp8r4xktdg8vdawme73kfr5eg0h9j4u2xgz3la0" 100.0
./ubu-cli sendtoaddress "U1qp8r4xktdg8vdawme73kfr5eg0h9j4u2xgz3la0" 100.0 "payment" "recipient"
```

---

## sendmany

Sends UBU to multiple addresses.

### Parameters
1. `amounts` (object, required) - Address/amount pairs
2. `minconf` (number, optional, default=1) - Minimum confirmations
3. `comment` (string, optional) - Transaction comment
4. `subtractfeefrom` (array, optional) - Addresses to subtract fee from

### Request Example
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "sendmany",
  "params": [
    {
      "U1addr1": 10.0,
      "U1addr2": 20.0,
      "U1addr3": 30.0
    },
    6,
    "batch payment"
  ]
}
```

### Returns
```json
"abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"
```

### Example
```bash
./ubu-cli sendmany '{"U1addr1":10.0,"U1addr2":20.0}'
```

---

## listtransactions

Lists wallet transactions.

### Parameters
1. `label` (string, optional, default="*") - Filter by label
2. `count` (number, optional, default=10) - Number of transactions
3. `skip` (number, optional, default=0) - Skip first N transactions
4. `include_watchonly` (boolean, optional, default=false) - Include watch-only

### Returns
```json
[
  {
    "address": "U1qp8r4xktdg8vdawme73kfr5eg0h9j4u2xgz3la0",
    "category": "receive",
    "amount": 50.00000000,
    "confirmations": 100,
    "blockhash": "0000000000000000000a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w",
    "blockheight": 12345,
    "blocktime": 1698765432,
    "txid": "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890",
    "time": 1698765430,
    "label": "my-label"
  }
]
```

### Example
```bash
./ubu-cli listtransactions
./ubu-cli listtransactions "*" 20 0
```

---

## listunspent

Lists unspent transaction outputs.

### Parameters
1. `minconf` (number, optional, default=1) - Minimum confirmations
2. `maxconf` (number, optional, default=9999999) - Maximum confirmations
3. `addresses` (array, optional) - Filter by addresses

### Returns
```json
[
  {
    "txid": "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890",
    "vout": 0,
    "address": "U1qp8r4xktdg8vdawme73kfr5eg0h9j4u2xgz3la0",
    "label": "my-label",
    "scriptPubKey": "76a914abcdef123456789088ac",
    "amount": 50.00000000,
    "confirmations": 100,
    "spendable": true,
    "solvable": true,
    "safe": true
  }
]
```

### Example
```bash
./ubu-cli listunspent
./ubu-cli listunspent 6 9999999
./ubu-cli listunspent 1 9999999 '["U1addr1","U1addr2"]'
```

---

## listaddresses

Lists all wallet addresses.

### Parameters
1. `include_change` (boolean, optional, default=false) - Include change addresses

### Returns
```json
[
  {
    "address": "U1qp8r4xktdg8vdawme73kfr5eg0h9j4u2xgz3la0",
    "label": "my-label",
    "type": "receive",
    "index": 0,
    "used": true
  },
  {
    "address": "U1qr9s5yludh9we0bnf84lgr6fh1i0k5v3yhz4mb1",
    "label": "",
    "type": "receive",
    "index": 1,
    "used": false
  }
]
```

### Example
```bash
./ubu-cli listaddresses
./ubu-cli listaddresses true
```

---

## getaddressinfo

Returns information about an address.

### Parameters
1. `address` (string, required) - Address to query

### Returns
```json
{
  "address": "U1qp8r4xktdg8vdawme73kfr5eg0h9j4u2xgz3la0",
  "ismine": true,
  "iswatchonly": false,
  "isscript": false,
  "scriptPubKey": "76a914abcdef123456789088ac",
  "label": "my-label",
  "hdkeypath": "m/44'/9999'/0'/0/0",
  "pubkey": "03abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"
}
```

### Example
```bash
./ubu-cli getaddressinfo "U1qp8r4xktdg8vdawme73kfr5eg0h9j4u2xgz3la0"
```

---

## backupwallet

Backs up the wallet to a file.

### Parameters
1. `destination` (string, required) - Backup file path

### Returns
```json
{
  "success": true,
  "path": "/path/to/backup.dat"
}
```

### Example
```bash
./ubu-cli backupwallet "/backup/wallet-20231101.dat"
```

---

## encryptwallet

Encrypts the wallet with a passphrase.

### Parameters
1. `passphrase` (string, required) - Encryption passphrase

### Returns
```json
{
  "success": true,
  "message": "Wallet encrypted. Restart daemon to complete encryption."
}
```

### Example
```bash
./ubu-cli encryptwallet "my-secure-passphrase"
```

**Warning:** After encryption, the daemon must be restarted.

---

## walletpassphrase

Unlocks the encrypted wallet.

### Parameters
1. `passphrase` (string, required) - Wallet passphrase
2. `timeout` (number, required) - Unlock timeout in seconds

### Returns
```json
{
  "success": true
}
```

### Example
```bash
./ubu-cli walletpassphrase "my-secure-passphrase" 300
```

---

## walletlock

Locks the encrypted wallet.

### Parameters
None

### Returns
```json
{
  "success": true
}
```

### Example
```bash
./ubu-cli walletlock
```

---

## dumpprivkey

Exports the private key for an address.

### Parameters
1. `address` (string, required) - Address to export

### Returns
```json
"L1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUV"
```

### Example
```bash
./ubu-cli dumpprivkey "U1qp8r4xktdg8vdawme73kfr5eg0h9j4u2xgz3la0"
```

**Warning:** Keep private keys secure. Never share them.

---

## importprivkey

Imports a private key.

### Parameters
1. `privkey` (string, required) - Private key in WIF format
2. `label` (string, optional, default="") - Address label
3. `rescan` (boolean, optional, default=true) - Rescan blockchain

### Returns
```json
{
  "success": true,
  "address": "U1qp8r4xktdg8vdawme73kfr5eg0h9j4u2xgz3la0"
}
```

### Example
```bash
./ubu-cli importprivkey "L1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUV"
./ubu-cli importprivkey "L1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUV" "imported" false
```

---

# Network RPC Methods

## getpeerinfo

Returns information about connected peers.

### Parameters
None

### Returns
```json
[
  {
    "id": 1,
    "addr": "192.168.1.100:8333",
    "version": 70015,
    "subver": "/UbuntuBlockchain:1.0.0/",
    "inbound": false,
    "startingheight": 12340,
    "synced_headers": 12345,
    "synced_blocks": 12345,
    "bytessent": 1234567,
    "bytesrecv": 7654321,
    "conntime": 1698765432,
    "pingtime": 0.025
  }
]
```

### Example
```bash
./ubu-cli getpeerinfo
```

---

## getnetworkinfo

Returns network status information.

### Parameters
None

### Returns
```json
{
  "version": 100000,
  "subversion": "/UbuntuBlockchain:1.0.0/",
  "protocolversion": 70015,
  "localservices": "0000000000000409",
  "timeoffset": 0,
  "connections": 8,
  "networks": [
    {
      "name": "ipv4",
      "limited": false,
      "reachable": true
    }
  ],
  "relayfee": 0.00001000
}
```

### Example
```bash
./ubu-cli getnetworkinfo
```

---

## getconnectioncount

Returns the number of connections to other nodes.

### Parameters
None

### Returns
```json
8
```

### Example
```bash
./ubu-cli getconnectioncount
```

---

## addnode

Adds or removes a peer.

### Parameters
1. `node` (string, required) - Node address (IP:port)
2. `command` (string, required) - "add", "remove", or "onetry"

### Returns
```json
{
  "success": true
}
```

### Example
```bash
./ubu-cli addnode "192.168.1.100:8333" "add"
./ubu-cli addnode "192.168.1.100:8333" "onetry"
```

---

# System RPC Methods

## stop

Stops the daemon gracefully.

### Parameters
None

### Returns
```json
"Shutting down..."
```

### Example
```bash
./ubu-cli stop
```

---

## uptime

Returns the daemon uptime in seconds.

### Parameters
None

### Returns
```json
86400
```

### Example
```bash
./ubu-cli uptime
```

---

## help

Returns help information for RPC methods.

### Parameters
1. `command` (string, optional) - Specific command to get help for

### Returns
```json
"getblockchaininfo\nReturns comprehensive information about the blockchain state.\n..."
```

### Example
```bash
./ubu-cli help
./ubu-cli help getblock
```

---

# Complete Usage Examples

## Send Transaction Workflow

```bash
# 1. Get new receiving address
ADDR=$(./ubu-cli getnewaddress "payment-001")

# 2. Check balance
./ubu-cli getbalance

# 3. Send transaction
TXID=$(./ubu-cli sendtoaddress "$ADDR" 100.0 "payment for services")

# 4. Get transaction details
./ubu-cli getrawtransaction "$TXID" true

# 5. List recent transactions
./ubu-cli listtransactions "*" 10
```

## Query Blockchain Status

```bash
# Get overall blockchain info
./ubu-cli getblockchaininfo

# Get current height
HEIGHT=$(./ubu-cli getblockcount)

# Get block hash at height
HASH=$(./ubu-cli getblockhash $HEIGHT)

# Get full block details
./ubu-cli getblock "$HASH" 2

# Get mempool status
./ubu-cli getmempoolinfo
```

## Wallet Management

```bash
# Generate multiple addresses
for i in {1..5}; do
  ./ubu-cli getnewaddress "addr-$i"
done

# List all addresses
./ubu-cli listaddresses

# Check specific address
./ubu-cli getaddressinfo "U1qp8r4xktdg8vdawme73kfr5eg0h9j4u2xgz3la0"

# Backup wallet
./ubu-cli backupwallet "/backup/wallet-$(date +%Y%m%d).dat"
```

## Network Monitoring

```bash
# Check connected peers
./ubu-cli getpeerinfo

# Get network stats
./ubu-cli getnetworkinfo

# Add seed node
./ubu-cli addnode "seed.ubuntublockchain.xyz:8333" "add"
```

---

# Programming Examples

## Python Example

```python
import requests
import json

class UbuRpcClient:
    def __init__(self, url="http://127.0.0.1:8332", user="ubuntu", password="changeme"):
        self.url = url
        self.auth = (user, password)
        self.id_counter = 0

    def call(self, method, params=[]):
        self.id_counter += 1
        payload = {
            "jsonrpc": "2.0",
            "id": self.id_counter,
            "method": method,
            "params": params
        }
        response = requests.post(self.url, json=payload, auth=self.auth)
        result = response.json()

        if "error" in result and result["error"]:
            raise Exception(f"RPC Error: {result['error']}")

        return result["result"]

# Usage
client = UbuRpcClient()

# Get blockchain info
info = client.call("getblockchaininfo")
print(f"Current height: {info['blocks']}")

# Generate address
address = client.call("getnewaddress", ["my-label"])
print(f"New address: {address}")

# Send transaction
txid = client.call("sendtoaddress", [address, 10.0])
print(f"Transaction ID: {txid}")
```

## Node.js Example

```javascript
const axios = require('axios');

class UbuRpcClient {
  constructor(url = 'http://127.0.0.1:8332', user = 'ubuntu', password = 'changeme') {
    this.url = url;
    this.auth = { username: user, password: password };
    this.idCounter = 0;
  }

  async call(method, params = []) {
    this.idCounter++;
    const payload = {
      jsonrpc: '2.0',
      id: this.idCounter,
      method: method,
      params: params
    };

    const response = await axios.post(this.url, payload, { auth: this.auth });

    if (response.data.error) {
      throw new Error(`RPC Error: ${JSON.stringify(response.data.error)}`);
    }

    return response.data.result;
  }
}

// Usage
(async () => {
  const client = new UbuRpcClient();

  // Get blockchain info
  const info = await client.call('getblockchaininfo');
  console.log(`Current height: ${info.blocks}`);

  // Generate address
  const address = await client.call('getnewaddress', ['my-label']);
  console.log(`New address: ${address}`);

  // Send transaction
  const txid = await client.call('sendtoaddress', [address, 10.0]);
  console.log(`Transaction ID: ${txid}`);
})();
```

---

# Rate Limiting and Best Practices

## Rate Limiting
- Default: No rate limiting
- Configure in `ubuntu.conf`: `rpcthreads=4`
- Recommended: Use connection pooling for high-volume applications

## Best Practices

1. **Always use HTTPS in production**
   ```ini
   rpcssl=1
   rpcsslcertificatechainfile=/path/to/cert.pem
   rpcsslprivatekeyfile=/path/to/key.pem
   ```

2. **Use strong authentication**
   ```bash
   # Generate strong password
   openssl rand -base64 32
   ```

3. **Limit RPC access**
   ```ini
   rpcallowip=127.0.0.1
   rpcallowip=192.168.1.0/24
   ```

4. **Handle errors gracefully**
   ```python
   try:
       result = client.call("getblock", [block_hash])
   except Exception as e:
       logger.error(f"RPC call failed: {e}")
   ```

5. **Use batch requests for multiple calls**
   ```json
   [
     {"jsonrpc":"2.0","id":1,"method":"getblockcount","params":[]},
     {"jsonrpc":"2.0","id":2,"method":"getbestblockhash","params":[]},
     {"jsonrpc":"2.0","id":3,"method":"getdifficulty","params":[]}
   ]
   ```

---

# Troubleshooting

## Connection Refused
```bash
# Check if daemon is running
ps aux | grep ubud

# Check RPC port
netstat -an | grep 8332

# Verify configuration
cat ~/.ubuntu-blockchain/ubuntu.conf
```

## Authentication Failed
```bash
# Verify credentials
./ubu-cli -rpcuser=ubuntu -rpcpassword=changeme getblockchaininfo

# Check config file
grep rpc ~/.ubuntu-blockchain/ubuntu.conf
```

## Method Not Found
```bash
# List all available methods
./ubu-cli help

# Check method name spelling
./ubu-cli help getblockchaininfo
```

---

For more information, visit: https://docs.ubuntublockchain.xyz
