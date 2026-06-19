# CP (Custom Protocol) 🤖

CP is a high-performance, lightweight communication protocol built from scratch, specifically engineered for **AI-to-AI agent interaction**. It provides structured data exchange with native support for agent identification, role-based messaging, and built-in data integrity verification.

---

## Table of Contents

- [Quick Overview](#quick-overview)
- [Key Features](#key-features)
- [Prerequisites](#prerequisites)
- [Installation Guide](#installation-guide)
- [Configuration](#configuration)
- [Running Your First Example](#running-your-first-example)
- [How It Works](#how-it-works)
- [Troubleshooting](#troubleshooting)
- [Next Steps](#next-steps)

---

##  Quick Overview

Think of CP as a **walkie-talkie system for AI agents**:
- **Sender (Initiator)**  - One agent that starts conversations and sends commands
- **Receiver (Responder)**  - Another agent that listens and responds to commands
- **Trigger Words**  - Special keywords that tell receivers what action to take (e.g., "MOVE", "STOP", "STATUS")
- **Checksums** - Built-in verification to ensure messages arrive correctly

---

##  Key Features

| Feature | Description |
|---------|-------------|
| **Custom Binary Protocol** | Optimized for ultra-low-latency, reliable inter-process or network communication |
| **Trigger Word Support** | Every packet includes a fixed-size `trigger` field (up to 16 characters), enabling agents to filter or route messages without parsing entire payloads |
| **Agent-Oriented Design** | Structured dialogues with role-based messaging (initiator/responder) and turn-based sequencing |
| **Data Integrity Verification** | Built-in CRC32 checksums ensure every transmitted packet arrives uncorrupted |
| **Encryption Ready** | Built on OpenSSL for secure, encrypted agent communication |
| **Heartbeat Monitoring** | Automatic liveness detection to ensure both agents stay connected |

---

##  Prerequisites

### System Requirements
- **Operating System:** Linux, macOS, or Windows (with WSL2)
- **RAM:** At least 512 MB free
- **Disk Space:** ~200 MB for build artifacts

### Software Requirements

#### 1. **C++ Compiler** (Pick ONE based on your OS)

**macOS (using Homebrew):**
```bash
brew install gcc cmake openssl
```

**Ubuntu/Debian Linux:**
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libssl-dev
```

**CentOS/RHEL:**
```bash
sudo yum groupinstall -y "Development Tools"
sudo yum install -y cmake openssl-devel
```

**Windows (WSL2):**
```bash
# Inside WSL2 terminal (Ubuntu)
sudo apt-get update
sudo apt-get install -y build-essential cmake libssl-dev
```

#### 2. **Verify Installation**

Run these commands to confirm everything is installed:

```bash
# Check C++ compiler
g++ --version
# Should show version >= 7.0

# Check CMake
cmake --version
# Should show version >= 3.10

# Check OpenSSL
openssl version
# Should show OpenSSL installed
```

---

##  Installation Guide

### Step 1: Clone the Repository

```bash
git clone https://github.com/furinaops/CP.git
cd CP
```

### Step 2: Create Build Directory

```bash
mkdir -p build
cd build
```

### Step 3: Configure with CMake

```bash
cmake ..
```

**Expected Output:**
```
-- The C compiler identification is GNU X.X.X
-- The CXX compiler identification is GNU X.X.X
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Found OpenSSL: /usr/lib/libcrypto.so
-- Configuring done
-- Generating done
```

### Step 4: Build the Project

```bash
make
```

**This will create:**
- `build/sender/cp_sender` - The sender executable
- `build/receiver/cp_receiver` - The receiver executable

**Expected Build Output:**
```
Scanning dependencies of target cp_shared
[ XX%] Building CXX object shared/CMakeFiles/cp_shared.src.dir/...
[ XX%] Linking CXX shared library ../lib/libcp_shared.so
[100%] Built target cp_sender
[100%] Built target cp_receiver
```

 **Success!** You now have compiled binaries ready to run.

---

##  Configuration

The system is configured via the `factory.cpproj` file. Here's what each section means:

```yaml
server:
  host: "0.0.0.0"              # Network interface (0.0.0.0 = all interfaces)
  port: 9000                   # Port for communication

session:
  version: 1                   # Protocol version
  echo_expiry_ms: 10000        # Timeout for waiting on response (10 seconds)
  echo_warn_ms: 7000           # Log warning if response takes longer (7 seconds)
  heartbeat_interval_ms: 500   # Health check every 500ms
  liveness_timeout_ms: 30000   # Consider connection dead after 30 seconds

agents:                        # Define your agents here
  - id: 0x01                   # Unique hex ID for this agent
    name: "robot_arm"          # Human-readable name
    role: initiator            # This agent starts conversations (SENDER)
    triggers: []               # Commands it can send
  
  - id: 0x02                   # Second agent
    name: "conveyor"           # Another human-readable name
    role: responder            # This agent responds (RECEIVER)
    triggers:                  # Commands it understands
      - MOVE                   # Can receive MOVE commands
      - STOP                   # Can receive STOP commands
      - SPEED                  # Can receive SPEED commands
      - STATUS                 # Can receive STATUS commands
      - EXECUTE                # Can receive EXECUTE commands
      - RESET                  # Can receive RESET commands
```

###  Customizing Configuration

To use your own configuration:

1. **Edit `factory.cpproj`** in the project root:
   ```bash
   nano factory.cpproj  # or use your favorite editor
   ```

2. **Change the server host/port** if needed:
   ```yaml
   server:
     host: "127.0.0.1"    # Change to localhost if running on same machine
     port: 5000           # Use any available port
   ```

3. **Add your own agents**:
   ```yaml
   agents:
     - id: 0x03
       name: "my_agent"
       role: initiator
       triggers: [PING, QUERY]
   ```

4. **Save and rebuild** (if you changed code):
   ```bash
   cd build && make
   ```

---

## 🎮 Running Your First Example

### Terminal 1: Start the Receiver (Listener)

```bash
# From project root
./receiver.py
```

**Expected Output:**
```
==================================================
      CP PROTOCOL SYSTEM - RECEIVER MODULE
==================================================

[*] Starting CP Receiver...
[*] Listening on 0.0.0.0:9000
[*] Waiting for connections...
```

The receiver is now **waiting for incoming messages**. Leave this terminal open.

### Terminal 2: Start the Sender (Initiator)

**Open a NEW terminal** in the project root:

```bash
# From project root
./sender.py
```

**Expected Output:**
```
==================================================
      CP PROTOCOL SYSTEM - SENDER MODULE
==================================================
Server Host: 0.0.0.0
Server Port: 9000
==================================================

[*] Connecting to receiver...
[*] Connected! Type your trigger word and message:
```

### Terminal 3: Send a Message (Optional - or use Terminal 2)

**In the sender terminal**, type a trigger word and message:

```
MOVE forward 10cm
```

**What happens:**
1. ✉️ Sender encodes "MOVE" as trigger, "forward 10cm" as payload
2. 🔒 CRC32 checksum is calculated and attached
3. 📡 Packet is sent over the network/socket
4. 📥 Receiver listens and detects the "MOVE" trigger
5. ✅ Receiver verifies checksum (data integrity check)
6. 💬 Receiver echoes back: `[MOVE] Received: forward 10cm`

### Stopping the Applications

Press **Ctrl+C** in either terminal to stop the sender or receiver gracefully.

```
^C
[!] Sender stopped by user.
```

---

## 🔍 How It Works

### The Communication Flow

```
SENDER                          RECEIVER
  │                                │
  │  1. User types: "MOVE abc"    │
  │  2. Encode message             │
  │     └─ Trigger: "MOVE"        │
  │     └─ Payload: "abc"         │
  │     └─ Calculate CRC32        │
  ├──────────────────────────────>│ 3. Receive packet
  │                              │ 4. Verify CRC32
  │                              │ 5. Parse trigger: "MOVE"
  │                              │ 6. Execute action
  │<──────────────────────────────┤ 7. Send response
  │ 8. Verify response checksum   │
  │ 9. Display to user            │
```

### Packet Structure (Simplified)

```
┌─────────────────────────────────────────┐
│ TRIGGER (16 bytes)  │ "MOVE"            │
├─────────────────────────────────────────┤
│ PAYLOAD (variable)  │ Message data      │
├─────────────────────────────────────────┤
│ CRC32 (4 bytes)     │ Checksum          │
└─────────────────────────────────────────┘
```

---

##  Troubleshooting

### Problem: "Binary not found at build/sender/cp_sender"

**Solution:** You haven't built the project yet.
```bash
cd build
cmake ..
make
cd ..
```

### Problem: "Address already in use" or "Port 9000 in use"

**Solution 1:** Change the port in `factory.cpproj`:
```yaml
server:
  port: 5000  # Use a different port
```

**Solution 2:** Kill the existing process using that port:
```bash
# macOS/Linux
lsof -i :9000
kill -9 <PID>

# Windows (PowerShell)
Get-Process -Id (Get-NetTCPConnection -LocalPort 9000).OwningProcess | Stop-Process
```

### Problem: "cmake: command not found"

**Solution:** Install CMake
```bash
# macOS
brew install cmake

# Ubuntu/Debian
sudo apt-get install cmake

# CentOS/RHEL
sudo yum install cmake
```

### Problem: "openssl not found" or OpenSSL linking errors

**Solution:** Install OpenSSL development libraries
```bash
# macOS
brew install openssl

# Ubuntu/Debian
sudo apt-get install libssl-dev

# CentOS/RHEL
sudo yum install openssl-devel
```

### Problem: Build fails with "C++17 not supported"

**Solution:** Use a newer compiler
```bash
# macOS - upgrade GCC
brew install gcc

# Linux - enable C++17 explicitly in CMakeLists.txt
# (Already enabled in our project, so update your compiler)
```

### Problem: "Connection refused" when running sender

**Solution:** Make sure the receiver is running in another terminal first!

```bash
# Terminal 1: Start receiver
./receiver.py

# Terminal 2: Then start sender
./sender.py
```


---

## 📖 Documentation

- **Protocol Specification:** See `shared/` directory
- **Sender Implementation:** See `sender/` directory
- **Receiver Implementation:** See `receiver/` directory
- **Configuration:** See `factory.cpproj`

---

##  Getting Help

If you encounter issues:

1. **Check the Troubleshooting section** above
2. **Run with verbose logging** (if available):
   ```bash
   CP_DEBUG=1 ./sender.py
   ```
3. **Check system requirements** match your OS
4. **Verify ports aren't blocked** by firewall:
   ```bash
   sudo ufw allow 9000  # Linux
   ```

---

##  License

This project is open source. See LICENSE file for details.

---
