# Industrial Monitoring System (IMS) - Sentinel-RT

## Project Overview

**Sentinel-RT** is a professional-grade, real-time industrial monitoring solution designed for the **QNX Real-Time Operating System (RTOS)** running on a Raspberry Pi 4.

It monitors critical equipment health (Vibration, Sound, Temperature, and Current) and provides a secure, encrypted data stream to remote clients. The system features **Mutual TLS (mTLS)** for "Zero Trust" security, a **Black Box** event recorder, and a live **Graphical Dashboard**.

## Key Features

### ✅ 1. Real-Time Hardware Monitoring
* **Vibration Monitoring:** Detects motor anomalies using SW-420 sensors via high-frequency GPIO polling.
* **Acoustic Monitoring:** Measures noise intensity duty cycles to detect mechanical failure.
* **Temperature Monitoring:** DS18B20 digital sensor for precision temperature readings (-55°C to +125°C).
* **Current Monitoring:** ACS712 Hall-effect sensor via ADS1115 16-bit ADC for AC/DC current measurement.
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
* **Visual Dashboard:** Python-based GUI client providing real-time vibration, sound, temperature, and current graphs.

---

## System Architecture
```text
┌─────────────────────────┐       ┌─────────────────────────┐
│     SENSORS (GPIO)      │       │    CLIENT APPLICATIONS  │
│ (Vib / Sound / I2C ADC) │       │ (Laptop / Control Room) │
│      (1-Wire Temp)      │       │                         │
└────────────┬────────────┘       └────────────┬────────────┘
             │ Signal                          │ mTLS (SSL/TLS)
             ▼                                 ▼
┌─────────────────────────┐       ┌─────────────────────────┐
│   QNX DRIVER LAYER      │       │     PROTOCOL LAYER      │
│ (drivers/sensor_mgr.c)  │<─────>│  (protocol/protocol.c)  │
│ - 1kHz Polling Thread   │ Data  │ - Command Parsing       │
│ - I2C/ADC Read Logic    │       │ - Encryption            │
│ - Signal Accumulation   │       │ - Black Box Logging     │
│ - Health Evaluation     │       │                         │
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

| Component | Function | Protocol | Connection (RPi Pin) | GPIO Number |
|-----------|----------|----------|---------------------|-------------|
| SW-420 | Vibration Sensor | GPIO | Physical Pin 11 | GPIO 17 |
| LM393 Mic | Sound Sensor | GPIO | Physical Pin 13 | GPIO 27 |
| DS18B20 | Temperature Sensor | 1-Wire | Physical Pin 7 | GPIO 4 |
| ADS1115 | 16-bit ADC | I2C | Pins 3 & 5 (SDA/SCL) | - |
| ACS712 | Current Sensor | Analog | Connected to ADS1115 CH0 | - |
| LED (Opt) | Status Indicator | GPIO | Physical Pin 15 | GPIO 22 |

### I2C Configuration
Before running the server, ensure the I2C driver is loaded:
```bash
# Load BCM2711 I2C driver for Raspberry Pi 4
i2c-bcm2711 -p i2c1
```

**Note:** 
- The DS18B20 requires a 4.7kΩ pull-up resistor between VDD and DATA line.
- ADS1115 default I2C address: 0x48
- Sound level is calculated based on the % of time the pin is HIGH vs LOW (duty cycle).

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
│   ├── sensors.c          # Low-level QNX GPIO/I2C/1-Wire Mapping
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

### 3. Prepare Raspberry Pi (QNX RTOS)

**IMPORTANT:** GPIO access on QNX requires root privileges.

```bash
# SSH into QNX Raspberry Pi
ssh <user_name>@<RPI_IP_ADDRESS>

# Create the deployment directory
mkdir -p /data/home/<user_name>/ims
cd /data/home/<user_name>/ims
```

### 4. Transfer Binaries to Raspberry Pi

From your development machine:
```bash
# Transfer server binary and certificates
scp ims_server <user_name>@<RPI_IP_ADDRESS>:/data/home/<user_name>/ims/
scp -r certs/ <user_name>@<RPI_IP_ADDRESS>:/data/home/<user_name>/ims/
```

### 5. Start the Server (On Raspberry Pi)

**Must run as root to access GPIO hardware:**
```bash
# SSH into QNX Pi
ssh <user_name>@<RPI_IP_ADDRESS>

# Switch to root user (required for GPIO access)
su

# Navigate to deployment directory
cd /data/home/<user_name>/ims

# Ensure I2C driver is running
i2c-bcm2711 -p i2c1

# Start the server
./ims_server
```

Expected output:
```
[INFO] IMS Server starting...
[INFO] Loading certificates from ./certs/
[INFO] GPIO initialized (Vibration: GPIO17, Sound: GPIO27)
[INFO] I2C device opened: /dev/i2c1
[INFO] DS18B20 temperature sensor initialized
[INFO] Server listening on port 8443
```

## Client Usage

### Option A: Graphical Dashboard (Python)
Provides real-time charts of motor health including vibration, sound, temperature, and current readings.

**⚠️ Note:** The dashboard is currently under active development and may have incomplete features or bugs.

```bash
# On your Laptop (Linux/Windows/macOS)
python3 ./clients/dashboard.py
```

**Requirements:**
- Python 3.7+
- Required packages: `matplotlib`, `numpy`
- Valid client certificates (`client.crt` and `client.key`) in the project root

**Dashboard Features:**
- Real-time multi-panel graphs for all sensor readings
- Color-coded health status indicators
- Auto-scaling axes
- Connection status display

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
| `get_sensors` | Returns raw values (Vibration Events/sec, Sound Duty %, Temp °C, Current A). |
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

## Sensor Details

### Temperature Sensor (DS18B20)
- **Protocol:** 1-Wire (Dallas/Maxim)
- **Range:** -55°C to +125°C
- **Accuracy:** ±0.5°C from -10°C to +85°C
- **Resolution:** 9-12 bit configurable (default 12-bit)
- **Wiring:** Requires 4.7kΩ pull-up resistor on data line

### Current Sensor (ACS712 + ADS1115)
- **ACS712 Variants:** ±5A, ±20A, or ±30A models supported
- **Sensitivity:** 185mV/A (5A), 100mV/A (20A), 66mV/A (30A)
- **ADC Resolution:** 16-bit via ADS1115
- **I2C Address:** 0x48 (default, configurable)
- **Measurement:** Both AC and DC current supported

## Troubleshooting

### Permission Denied (GPIO Access)
```bash
# Error: "Permission denied" when accessing /dev/gpio
# Solution: Must run as root
su
./ims_server
```

### I2C Issues
```bash
# Verify I2C driver is loaded
pidin | grep i2c

# Scan for I2C devices
i2c -d /dev/i2c1 scan
# Expected: Device at 0x48 (ADS1115)
```

### Temperature Sensor Not Detected
```bash
# Check 1-Wire kernel module (if applicable)
# Verify 4.7kΩ pull-up resistor is connected
# Confirm GPIO 4 pin connection
```

### Current Readings Incorrect
- Verify ACS712 is powered correctly (5V)
- Check zero-current offset calibration
- Ensure proper connection to ADS1115 channel 0

### Server Won't Start
```bash
# Check if directory exists
ls -la /data/home/<user_name>/ims

# If not, create it
mkdir -p /data/home/<user_name>/ims

# Verify binary has execute permissions
chmod +x /data/home/<user_name>/ims/ims_server
```

## QNX-Specific Notes

### Root Access Requirement
On QNX RTOS, GPIO access is restricted to the root user. The server **must** be run with root privileges:
```bash
su          # Switch to root
./ims_server
```

### Deployment Path
All binaries and certificates should be placed in:
```
/data/home/<user_name>/ims/
```
Replace `<user_name>` with your actual QNX username.

### System Resources
- **GPIO Devices:** `/dev/gpio17`, `/dev/gpio27`, `/dev/gpio4`
- **I2C Bus:** `/dev/i2c1`
- **Process Priority:** Server runs with default QNX scheduling policy (can be adjusted with `nice` or `renice`)

## Future Roadmap

- ✅ ~~Current Sensing: Integration of ACS712 Sensor via ADS1115 ADC~~ (Completed)
- ✅ ~~Temperature: Integration of DS18B20 Digital Sensor~~ (Completed)
- 🔄 Dashboard Enhancement: Improve Python dashboard stability and features (In Progress)
- 📋 QNX Adaptive Partitioning: Implement APS to isolate Server thread from Client threads
- 📋 Data Logging: Add CSV export functionality for historical analysis
- 📋 Web Interface: Develop browser-based monitoring dashboard

---

**Author:** Hemanth Kumar  
**GitHub:** [@Hemanthkumar04](https://github.com/Hemanthkumar04)  
**Email:** hky21.github@gmail.com  
**Project Type:** Final Year Project - Industrial Monitoring System  
**License:** Academic / Open Source
