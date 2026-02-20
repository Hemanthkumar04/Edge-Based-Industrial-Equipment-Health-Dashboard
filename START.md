# Sentinel-RT: Industrial Monitoring System

A professional-grade **real-time industrial monitoring system** built on **QNX RTOS** for Raspberry Pi 4. Sentinel-RT provides enterprise-level equipment health monitoring with military-grade security, persistent event logging, and live visualization capabilities.

---

## 🎯 Key Features

- **Real-Time Sensor Monitoring**: Vibration (SW-420), sound (LM393), temperature (DS18B20), and current (ACS712) detection at 1kHz sampling rate
- **I2C/1-Wire Integration**: 16-bit ADC (ADS1115) for precision current measurement and digital temperature sensing
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
- **drivers/sensors.c** — QNX-specific GPIO/I2C/1-Wire access via memory-mapped I/O (`mmap`) and `devctl()`
- **drivers/sensor_manager.c** — Background thread with 1kHz sensor polling for precision monitoring

### Client Applications
- **clients/ims_client.c** — Lightweight C-based terminal client
- **clients/dashboard.py** — Real-time graphical dashboard (Python + Matplotlib) **[Under Development]**

---

## 🚀 Quick Start

### Prerequisites
- Raspberry Pi 4 with QNX 8.0 installed
- Root/sudo access for GPIO memory mapping and I2C access
- Python 3.7+ with `matplotlib` (for dashboard client)
- **Sensors:**
  - SW-420 vibration sensor
  - LM393 sound sensor module
  - DS18B20 digital temperature sensor
  - ACS712 Hall-effect current sensor (±5A/±20A/±30A)
  - ADS1115 16-bit I2C ADC module

### Installation

```bash
# 1. Clone the repository
git clone https://github.com/Hemanthkumar04/sentinel-rt.git
cd sentinel-rt

# 2. Build and generate SSL certificates
./scripts/quick_start.sh all

# 3. On Raspberry Pi: Create deployment directory
mkdir -p /data/home/<user_name>/ims
cd /data/home/<user_name>/ims

# 4. Transfer binaries (from development machine)
scp ims_server <user_name>@<RPI_IP>:/data/home/<user_name>/ims/
scp -r certs/ <user_name>@<RPI_IP>:/data/home/<user_name>/ims/

# 5. On Raspberry Pi: Load I2C driver and start server
su  # Switch to root (required for GPIO/I2C access)
i2c-bcm2711 -p i2c1
./ims_server

# 6. Launch the dashboard (from your workstation)
python3 ./clients/dashboard.py
```

---

## 🔌 Hardware Setup

### Platform
- **Device**: Raspberry Pi 4
- **OS**: QNX 8.0 RTOS
- **GPIO**: BCM numbering scheme
- **I2C Bus**: `/dev/i2c1` (BCM2711 controller)

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

#### Temperature Sensor (DS18B20)
| Sensor Pin | Raspberry Pi |
|------------|--------------|
| VDD | 3.3V (Physical Pin 1) |
| GND | GND (Physical Pin 6) |
| DATA | GPIO 4 (Physical Pin 7) |

**Important**: Add a **4.7kΩ pull-up resistor** between VDD and DATA line.

#### Current Sensor (ACS712 + ADS1115)
| Component | Connection |
|-----------|------------|
| **ADS1115 ADC** | |
| VDD | 3.3V (Physical Pin 1) |
| GND | GND (Physical Pin 6) |
| SDA | GPIO 2 / SDA (Physical Pin 3) |
| SCL | GPIO 3 / SCL (Physical Pin 5) |
| **ACS712 Sensor** | |
| VCC | 5V (Physical Pin 2) |
| GND | GND (Physical Pin 6) |
| OUT | ADS1115 Channel A0 |

**I2C Address**: ADS1115 default is `0x48` (configurable via ADDR pin)

> **Note**: 
> - Calibrate the LM393 sensitivity using the onboard potentiometer. Adjust until the LED indicator only activates on loud sounds (e.g., clapping).
> - ACS712 requires 5V power supply but outputs 0-5V analog signal (compatible with ADS1115).
> - DS18B20 operates in parasitic or normal power mode; normal mode (3-pin connection) is recommended.

---

## 💻 Command Reference

Once connected via client or dashboard, the following commands are available:

| Command | Description |
|---------|-------------|
| `monitor` | Enable live monitoring mode; streams status updates every 1 second with automatic critical alerts |
| `get_sensors` | Retrieve raw sensor readings (vibration events/sec, sound duty cycle %, temperature °C, current A) |
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

| Status | Vibration (events/sec) | Sound (duty %) | Temperature (°C) | Current (A) | System Action |
|--------|------------------------|----------------|------------------|-------------|---------------|
| **HEALTHY** | 0 – 20 | 0% – 50% | 15 – 40 | 0 – 15 | Standard logging |
| **WARNING** | 20 – 50 | 50% – 80% | 40 – 60 | 15 – 25 | Console warning output |
| **CRITICAL** | > 50 | > 80% | > 60 | > 25 | Persistent log to `blackbox.log` |

**Note**: Thresholds are configurable based on your specific equipment and environment.

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

### 3. Multi-Protocol Sensor Integration
Sentinel-RT demonstrates professional-grade hardware abstraction:
- **GPIO** for digital sensors (vibration, sound)
- **I2C** for precision ADC communication (current sensing)
- **1-Wire** for digital temperature sensors
- **QNX devctl()** for atomic I2C transactions

### 4. Real-Time Visualization
Data doesn't just stream to a terminal:
- Live Python GUI with `matplotlib` integration
- Visual threshold indicators (red zones for critical values)
- Historical trend analysis
- **[Dashboard under active development - features may be incomplete]**

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
su  # Switch to root user
./ims_server
```

### Issue: "I2C device not found" or "ADS1115 read failed"

**Cause**: I2C driver not loaded or ADS1115 not connected properly

**Solution**: Load I2C driver and verify device

```bash
# Load I2C driver
i2c-bcm2711 -p i2c1

# Scan for I2C devices
i2c -d /dev/i2c1 scan
# Expected output: Device at 0x48
```

### Issue: "Temperature sensor timeout"

**Cause**: DS18B20 not wired correctly or missing pull-up resistor

**Solution**: Verify connections

- Check 4.7kΩ pull-up resistor between VDD and DATA
- Confirm GPIO 4 connection
- Ensure sensor is getting 3.3V power

### Issue: "Current reading always 0" or "Invalid current value"

**Cause**: ACS712 not powered or not connected to ADC

**Solution**: Check wiring

- Verify ACS712 has 5V power supply
- Confirm ACS712 OUT pin is connected to ADS1115 A0
- Check I2C connections (SDA/SCL)

### Issue: "ModuleNotFoundError: No module named 'matplotlib'"

**Cause**: Python dependencies not installed on client workstation

**Solution**: Install required Python packages

```bash
pip3 install matplotlib numpy
# Or for system-wide installation:
sudo apt install python3-matplotlib python3-numpy
```

### Issue: "Permission denied: /data/home/<user_name>/ims"

**Cause**: Deployment directory doesn't exist

**Solution**: Create the directory structure

```bash
mkdir -p /data/home/<user_name>/ims
chmod 755 /data/home/<user_name>/ims
```

---

## 📦 Project Structure

```
sentinel-rt/
├── apps/
│   └── server.c              # Main server application
├── clients/
│   ├── ims_client.c          # C-based terminal client
│   └── dashboard.py          # Python GUI dashboard [Under Development]
├── common/
│   └── tls_helper.c          # TLS/SSL utilities
├── drivers/
│   ├── sensors.c             # GPIO/I2C/1-Wire hardware abstraction
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

## 🔬 Sensor Specifications

### DS18B20 Temperature Sensor
- **Protocol**: 1-Wire (Dallas/Maxim)
- **Range**: -55°C to +125°C
- **Accuracy**: ±0.5°C (-10°C to +85°C)
- **Resolution**: 9-12 bit configurable
- **Conversion Time**: 750ms (12-bit)

### ACS712 Current Sensor
- **Variants**: ±5A, ±20A, ±30A
- **Sensitivity**: 
  - 5A: 185 mV/A
  - 20A: 100 mV/A
  - 30A: 66 mV/A
- **Zero Current**: 2.5V (Vcc/2)
- **Bandwidth**: DC to 80 kHz

### ADS1115 ADC
- **Resolution**: 16-bit
- **Sample Rate**: Up to 860 SPS
- **Input Range**: ±6.144V to ±0.256V (programmable)
- **I2C Address**: 0x48 (default), configurable to 0x49-0x4B
- **Interface**: I2C (up to 3.4 MHz)

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
- [ADS1115 Datasheet](https://www.ti.com/product/ADS1115)
- [DS18B20 Datasheet](https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf)
- [ACS712 Datasheet](https://www.allegromicro.com/en/products/sense/current-sensor-ics/zero-to-fifty-amp-integrated-conductor-sensor-ics/acs712)

---

**Author**: Hemanth Kumar  
**GitHub**: [@Hemanthkumar04](https://github.com/Hemanthkumar04)  
**Email**: hky21.github@gmail.com

**Built with ❤️ for industrial IoT applications**
