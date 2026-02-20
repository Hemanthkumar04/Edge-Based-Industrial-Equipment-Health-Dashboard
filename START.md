# Sentinel-RT: Industrial Monitoring System

[![QNX RTOS](https://img.shields.io/badge/QNX-8.0-blue.svg)](https://www.qnx.com/)
[![Platform](https://img.shields.io/badge/Platform-Raspberry%20Pi%204-red.svg)](https://www.raspberrypi.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

A professional-grade **real-time industrial monitoring system** built on **QNX RTOS** for Raspberry Pi 4. Sentinel-RT provides enterprise-level equipment health monitoring with military-grade security, persistent event logging, and live visualization capabilities.

---

## 🎯 Key Features

- **Real-Time Sensor Monitoring**: Vibration (SW-420) and sound (LM393) detection at 1kHz sampling rate
- **Mutual TLS (mTLS) Security**: Zero-trust architecture with bidirectional certificate authentication
- **Black Box Recorder**: Persistent event logging for forensic analysis and compliance
- **Live Dashboard**: Python-based GUI with real-time graphing and threshold visualization
- **QNX RTOS**: Deterministic real-time performance for critical industrial applications

---

## 📚 Documentation Structure

| Document | Read Time | Description |
|----------|-----------|-------------|
| **START.md** (this file) | 5 min | Quick start guide and hardware setup |
| **README.md** | 10 min | Architecture deep-dive and usage guide |
| **scripts/quick_start.sh** | — | Automated build and deployment script |

---

## 🏗️ System Architecture

### Core Application
- **apps/server.c** — Main entry point; handles TCP connections and client session management

### Security Layer
- **protocol/protocol.c** — Command processing (`monitor`, `get_log`), encryption, and black box logic
- **protocol/authorization.c** — Role-based access control (Admin/Viewer) via certificate extraction
- **common/tls_helper.c** — OpenSSL initialization and key management wrappers

### Hardware Abstraction Layer
- **drivers/sensors.c** — QNX-specific GPIO access via memory-mapped I/O (`mmap`)
- **drivers/sensor_manager.c** — Background thread with 1kHz sensor polling for precision monitoring

### Client Applications
- **clients/ims_client.c** — Lightweight C-based terminal client
- **clients/dashboard.py** — Real-time graphical dashboard (Python + Matplotlib)

---

## 🚀 Quick Start

### Prerequisites
- Raspberry Pi 4 with QNX 8.0 installed
- Root/sudo access for GPIO memory mapping
- Python 3.7+ with `matplotlib` (for dashboard client)
- SW-420 vibration sensor
- LM393 sound sensor module

### Installation

```bash
# 1. Clone the repository
git clone https://github.com/yourusername/sentinel-rt.git
cd sentinel-rt

# 2. Build and generate SSL certificates
./scripts/quick_start.sh all

# 3. Start the server (requires root for GPIO access)
sudo ./ims_server

# 4. Launch the dashboard (from your workstation)
python3 clients/dashboard.py
```

---

## 🔌 Hardware Setup

### Platform
- **Device**: Raspberry Pi 4
- **OS**: QNX 8.0 RTOS
- **GPIO**: BCM numbering scheme

### Wiring Diagram

#### Vibration Sensor (SW-420)
| Sensor Pin | Raspberry Pi |
|------------|--------------|
| VCC | 3.3V (Physical Pin 1) |
| GND | GND (Physical Pin 6) |
| DO | GPIO 17 (Physical Pin 11) |

#### Sound Sensor (LM393 Microphone Module)
| Sensor Pin | Raspberry Pi |
|------------|--------------|
| VCC | 3.3V (Physical Pin 1) |
| GND | GND (Physical Pin 6) |
| DO | GPIO 27 (Physical Pin 13) |

> **Note**: Calibrate the LM393 sensitivity using the onboard potentiometer. Adjust until the LED indicator only activates on loud sounds (e.g., clapping).

---

## 💻 Command Reference

Once connected via client or dashboard, the following commands are available:

| Command | Description |
|---------|-------------|
| `monitor` | Enable live monitoring mode; streams status updates every 1 second with automatic critical alerts |
| `get_sensors` | Retrieve raw sensor readings (vibration events/sec, sound duty cycle %) |
| `get_health` | Query evaluated health status (HEALTHY / WARNING / CRITICAL) |
| `get_log` | Download the `blackbox.log` file for forensic analysis |
| `whoami` | Display your certificate Common Name (CN) and assigned role |
| `quit` | Terminate connection gracefully |

---

## 🛡️ Security Architecture

### Mutual TLS Authentication
Sentinel-RT implements **bidirectional certificate validation**:

1. **Server → Client**: Server validates client certificate before accepting connection
2. **Client → Server**: Client validates server certificate before transmitting data
3. **Role-Based Access**: Permissions extracted from certificate attributes
   - **Admin**: Full access (monitor, logs, configuration)
   - **Viewer**: Read-only access (monitor, health status)

This "zero-trust" model ensures that unauthorized devices are rejected at the transport layer, before any application data is exchanged.

---

## 📊 Monitoring Thresholds

Health status is evaluated using the following criteria (configured in `sensor_manager.c`):

| Status | Vibration (events/sec) | Sound (duty cycle %) | System Action |
|--------|------------------------|----------------------|---------------|
| **HEALTHY** | 0 – 20 | 0% – 50% | Standard logging |
| **WARNING** | 20 – 50 | 50% – 80% | Console warning output |
| **CRITICAL** | > 50 | > 80% | Persistent log to `blackbox.log` |

### Black Box Event Recorder
When a **CRITICAL** event occurs:
- Event details are **immediately flushed** to persistent storage (`blackbox.log`)
- Timestamp, sensor values, and health status are recorded
- Data survives power loss or system crash (similar to aircraft flight recorders)

---

## 🎨 What Makes Sentinel-RT Special?

### 1. Zero-Trust Security (mTLS)
Unlike typical IoT projects that only authenticate the server, Sentinel-RT validates **both endpoints**:
- Prevents unauthorized device access
- Cryptographic identity verification
- Role-based authorization from X.509 certificate attributes

### 2. Flight Recorder Persistence
Critical events are **immediately persisted to disk**:
- Forensic analysis capability
- Compliance logging for industrial regulations
- Survives unexpected power failures

### 3. Real-Time Visualization
Data doesn't just stream to a terminal:
- Live Python GUI with `matplotlib` integration
- Visual threshold indicators (red zones for critical values)
- Historical trend analysis

---

## 🚦 Troubleshooting

### Issue: "Certificate verify failed"

**Cause**: System clock is incorrect (e.g., reset to January 1, 1970)

**Solution**: Set the correct date and time on the Raspberry Pi

```bash
# Format: MMDDhhmmYYYY (Month, Day, Hour, Minute, Year)
date 021510002026  # Example: Feb 15, 10:00 AM, 2026
```

### Issue: "GPIO Init Failed" or "ThreadCtl failed"

**Cause**: Insufficient permissions for memory-mapped I/O

**Solution**: Run the server with root privileges

```bash
sudo ./ims_server
```

### Issue: "ModuleNotFoundError: No module named 'matplotlib'"

**Cause**: Python dependencies not installed on client workstation

**Solution**: Install required Python packages

```bash
pip3 install matplotlib
# Or for system-wide installation:
sudo apt install python3-matplotlib
```

---

## 📦 Project Structure

```
sentinel-rt/
├── apps/
│   └── server.c              # Main server application
├── clients/
│   ├── ims_client.c          # C-based terminal client
│   └── dashboard.py          # Python GUI dashboard
├── common/
│   └── tls_helper.c          # TLS/SSL utilities
├── drivers/
│   ├── sensors.c             # GPIO hardware abstraction
│   └── sensor_manager.c      # Sensor polling thread
├── protocol/
│   ├── protocol.c            # Command protocol implementation
│   └── authorization.c       # Certificate-based auth
├── scripts/
│   └── quick_start.sh        # Automated build script
├── certs/                    # SSL certificates (generated)
├── START.md                  # This file
└── README.md                 # Detailed documentation
```

---

## 🤝 Contributing

Contributions are welcome! Please read our contributing guidelines before submitting pull requests.

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 🔗 Additional Resources

- [QNX Neutrino RTOS Documentation](https://www.qnx.com/developers/docs/)
- [Raspberry Pi GPIO Pinout](https://pinout.xyz/)
- [OpenSSL mTLS Guide](https://www.openssl.org/docs/)

---

**Built with ❤️ for industrial IoT applications**
