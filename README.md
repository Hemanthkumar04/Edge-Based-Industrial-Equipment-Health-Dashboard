# Industrial Monitoring System (IMS) - Sentinel-RT

## Project Overview

**Sentinel-RT** is a professional-grade, real-time industrial monitoring solution designed for the **QNX Real-Time Operating System (RTOS)** running on a Raspberry Pi 4.

It monitors critical equipment health (Vibration & Sound) and provides a secure, encrypted data stream to remote clients. The system features **Mutual TLS (mTLS)** for "Zero Trust" security, a **Black Box** event recorder, and a live **Graphical Dashboard**.

## Key Features

### ✅ 1. Real-Time Hardware Monitoring
* **Vibration Monitoring:** Detects motor anomalies using SW-420 sensors via high-frequency GPIO polling.
* **Acoustic Monitoring:** Measures noise intensity duty cycles to detect mechanical failure.
* **Deterministic Polling:** Dedicated QNX background threads ensure precise 1ms sampling intervals.

### ✅ 2. Enterprise-Grade Security
* **Mutual TLS (mTLS):** Both Server and Client must present valid X.509 certificates signed by our internal CA.
* **Role-Based Access Control (RBAC):**
    * **Admin:** Full control (calibration, shutdown).
    * **Viewer:** Read-only access to dashboards.
* **Encryption:** All data uses AES-256-GCM encryption over TCP/IP.

### ✅ 3. Advanced Data Handling
* **Live Monitor Mode:** Push-based streaming protocol sends updates every 1 second.
* **Black Box Logger:** Automatically saves `CRITICAL` alerts to a non-volatile `blackbox.log` file on the device (Forensics).
* **Visual Dashboard:** Python-based GUI client providing real-time vibration and sound graphs.

---

## System Architecture
```text
┌─────────────────────────┐       ┌─────────────────────────┐
│     SENSORS (GPIO)      │       │    CLIENT APPLICATIONS  │
│  (Vibration / Sound)    │       │ (Laptop / Control Room) │
└────────────┬────────────┘       └────────────┬────────────┘
             │ Signal                          │ mTLS (SSL/TLS)
             ▼                                 ▼
┌─────────────────────────┐       ┌─────────────────────────┐
│   QNX DRIVER LAYER      │       │     PROTOCOL LAYER      │
│ (drivers/sensor_mgr.c)  │<─────>│  (protocol/protocol.c)  │
│ - 1kHz Polling Thread   │ Data  │ - Command Parsing       │
│ - Signal Accumulation   │       │ - Encryption            │
│ - Health Evaluation     │       │ - Black Box Logging     │
└─────────────────────────┘       └─────────────────────────┘
             │                                 ▲
             └───────────────┐                 │
                             ▼                 │
                  ┌─────────────────────────┐  │
                  │    MAIN SERVER LOOP     │──┘
                  │     (apps/server.c)     │
                  └─────────────────────────┘
```

## Hardware Setup

**Platform:** Raspberry Pi 4 Model B (Running QNX Neutrino 8.0)

| Component | Function | Connection (RPi Pin) | GPIO Number |
|-----------|----------|---------------------|-------------|
| SW-420 | Vibration Sensor | Physical Pin 11 | GPIO 17 |
| LM393 Mic | Sound Sensor | Physical Pin 13 | GPIO 27 |
| LED (Opt) | Status Indicator | Physical Pin 15 | GPIO 22 |

*(Note: The system logic uses Digital Input for both sensors. Sound level is calculated based on the % of time the pin is HIGH vs LOW).*

## Directory Structure
```
.
├── apps/
│   └── server.c           # Main entry point (Connection Handler)
├── clients/
│   ├── ims_client.c       # C-based Text Terminal Client
│   └── dashboard.py       # Python Graphical Dashboard (Matplotlib)
├── common/
│   ├── tls_helper.c       # OpenSSL Context & Certificate Loading
│   └── ...
├── drivers/
│   ├── sensors.c          # Low-level QNX GPIO Mapping
│   └── sensor_manager.c   # Background Polling Thread & Health Logic
├── protocol/
│   ├── protocol.c         # Command Logic (Monitor, Log, Help)
│   └── authorization.c    # Role extraction from Certificates
├── scripts/
│   └── quick_start.sh     # One-click Build & Deploy tool
├── certs/                 # Generated Keys & Certificates
├── blackbox.log           # Event Audit Trail (Generated at runtime)
└── Makefile               # Build System
```

## Installation & Deployment

### 1. Prerequisites
- QNX SDP 8.0 installed on host.
- OpenSSL libraries.
- Python 3 + Matplotlib (for the dashboard).

### 2. Quick Build
Use the automated script to compile code and generate fresh SSL certificates:
```bash
./scripts/quick_start.sh all
```

### 3. Start the Server (On Raspberry Pi)
Must run as root to access GPIO hardware:
```bash
# SSH into QNX Pi
su
cd ims
./ims_server
```

## Client Usage

### Option A: Graphical Dashboard (Python)
Provides real-time charts of motor health.
```bash
# On your Laptop (Linux/Windows)
python3 clients/dashboard.py
```

*Requires `client.crt` and `client.key` in the project root.*

### Option B: Terminal Client (C)
Best for debugging and checking logs.
```bash
./ims_client <RPI_IP_ADDRESS>
```

### Available Commands

| Command | Description |
|:--------|:------------|
| `monitor` | Starts Live Mode. Streams status every 1s. Auto-pushes alerts. |
| `get_health` | Returns current Snapshot (Healthy/Warning/Critical). |
| `get_sensors` | Returns raw values (Vibration Events/sec, Sound Duty Cycle %). |
| `get_log` | Downloads the blackbox.log file content from the server. |
| `whoami` | Shows your Certificate Common Name and Access Role. |
| `list_units` | Lists all registered machinery (e.g., "Sentinel-RT"). |

## Security Details (mTLS)

- **Certificate Authority (CA):** We generated our own private CA (`ca.crt`).
- **Identity:** Every client has a unique certificate (`client.crt`) signed by this CA.
- **Verification:**
  - Server rejects any connection not signed by the CA.
  - Server extracts the Role (Admin/User) from the certificate fields.
- **Logging:** Every Critical Alert is timestamped and saved to disk (`blackbox.log`) for post-incident forensics.

## Future Roadmap (Next Steps)

- **Current Sensing:** Integration of ACS712 Sensor via ADS1115 ADC.
- **Temperature:** Integration of DS18B20 Digital Sensor.
- **Isolation:** Implementation of QNX Adaptive Partitioning (APS) to isolate the Server thread from Client threads.

---

**Author:** Hemanth  
**License:** Academic / Open Source
