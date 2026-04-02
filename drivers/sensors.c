#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "sensors.h"

// ============================================================
// QNX IMPLEMENTATION (Runs on the Raspberry Pi)
// ============================================================
#ifdef __QNX__
    #include <sys/mman.h>
    #include <sys/neutrino.h>
    #include <devctl.h>
    #include <hw/inout.h>
    #include <hw/i2c.h>

    // Raspberry Pi 4 BCM2711 GPIO Base
    #define GPIO_BASE_PHY 0xFE200000
    #define GPIO_LEN      0x100
    #define ADS1115_I2C_ADDR 0x48
    #define TMP102_I2C_ADDR  0x49
    #define ADXL345_I2C_ADDR 0x53

    #ifndef I2C_ADDRFMT_7BIT
    #define I2C_ADDRFMT_7BIT 0
    #endif

    static uintptr_t gpio_base = 0;
    static int i2c_fd = -1;

    static int ensure_i2c_open(void) {
        if (i2c_fd >= 0)
            return i2c_fd;

        i2c_fd = open("/dev/i2c1", O_RDWR);
        if (i2c_fd < 0)
            perror("open /dev/i2c1 failed");

        return i2c_fd;
    }

    static int i2c_write_bytes(uint16_t slave_addr, const uint8_t *buf, uint16_t len)
    {
        struct {
            i2c_send_t hdr;
            uint8_t payload[8];
        } msg;

        if (len > sizeof(msg.payload))
            return -1;

        memcpy(msg.payload, buf, len);
        msg.hdr.slave.addr = slave_addr;
        msg.hdr.slave.fmt = I2C_ADDRFMT_7BIT;
        msg.hdr.len = len;
        msg.hdr.stop = 1;

        return devctl(ensure_i2c_open(), DCMD_I2C_SEND, &msg, sizeof(msg), NULL) == 0 ? 0 : -1;
    }

    static int i2c_write_read(uint16_t slave_addr, uint8_t reg, uint8_t *rx_buf, uint16_t rx_len)
    {
        struct {
            i2c_sendrecv_t hdr;
            uint8_t data[8];
        } msg;

        if (rx_len > sizeof(msg.data))
            return -1;

        memset(&msg, 0, sizeof(msg));
        msg.hdr.slave.addr = slave_addr;
        msg.hdr.slave.fmt = I2C_ADDRFMT_7BIT;
        msg.hdr.send_len = 1;
        msg.hdr.recv_len = rx_len;
        msg.hdr.stop = 1;
        msg.data[0] = reg;

        if (devctl(ensure_i2c_open(), DCMD_I2C_SENDRECV, &msg, sizeof(msg), NULL) != 0)
            return -1;

        memcpy(rx_buf, msg.data, rx_len);
        return 0;
    }

    static int detect_ads1115(void)
    {
        uint8_t data[2];
        return i2c_write_read(ADS1115_I2C_ADDR, 0x00, data, 2) == 0;
    }

    static int detect_tmp102(void)
    {
        uint8_t data[2];
        return i2c_write_read(TMP102_I2C_ADDR, 0x00, data, 2) == 0;
    }

    static int detect_adxl345(void)
    {
        uint8_t devid = 0;
        if (i2c_write_read(ADXL345_I2C_ADDR, 0x00, &devid, 1) != 0)
            return 0;
        return devid == 0xE5;
    }

    static void init_i2c_sensors(void)
    {
        uint8_t adxl_pw_ctl[2] = {0x2D, 0x08};
        (void)i2c_write_bytes(ADXL345_I2C_ADDR, adxl_pw_ctl, sizeof(adxl_pw_ctl));
    }

    int hw_init() {
        if (ThreadCtl(_NTO_TCTL_IO, 0) == -1) {
            perror("ThreadCtl failed");
            return -1;
        }

        gpio_base = mmap_device_io(GPIO_LEN, GPIO_BASE_PHY);
        if (gpio_base == (uintptr_t)MAP_FAILED) {
            perror("mmap_device_io failed");
            return -1;
        }

        if (ensure_i2c_open() < 0)
            return -1;

        init_i2c_sensors();

        printf("[I2C] ADS1115 @0x48 : %s\n", detect_ads1115() ? "detected" : "not detected");
        printf("[I2C] TMP102  @0x49 : %s\n", detect_tmp102() ? "detected" : "not detected");
        printf("[I2C] ADXL345 @0x53 : %s\n", detect_adxl345() ? "detected" : "not detected");

        return 0;
    }

    int hw_read_pin(int pin) {
        if (!gpio_base) return 0;
        uint32_t level = in32(gpio_base + 0x34);
        return (level & (1 << pin)) ? 1 : 0;
    }

    void hw_configure_pin(int pin, int direction) {
        if (!gpio_base) return;
        
        uint32_t fsel_offset = 0x00 + (pin / 10) * 4;
        uint32_t shift = (pin % 10) * 3;
        uint32_t current = in32(gpio_base + fsel_offset);
        
        current &= ~(7 << shift); 
        if (direction == 1) {     
            current |= (1 << shift);
        }
        out32(gpio_base + fsel_offset, current);
    }

    void hw_write_pin(int pin, int val) {
        if (!gpio_base) return;
        if (val) {
            out32(gpio_base + 0x1C, 1 << pin); 
        } else {
            out32(gpio_base + 0x28, 1 << pin); 
        }
    }

    float hw_read_vibration_i2c() {
        uint8_t data[2];

        if (i2c_write_read(ADXL345_I2C_ADDR, 0x32, data, 2) != 0)
            return 0.0f;

        int16_t x_raw = (int16_t)((data[1] << 8) | data[0]);
        float g = (float)x_raw * 0.0039f;
        return g < 0.0f ? -g : g;
    }

    // --- I2C: ADS1115 (Current Sensor via ACS712) ---
    float hw_read_current_i2c() {
        int fd = ensure_i2c_open();
        if (fd < 0) {
            return 0.0;
        }

        // 1. Write Config Register (0x01)
        struct {
            i2c_send_t hdr;
            uint8_t buf[3];
        } tx_msg;
        
        tx_msg.hdr.slave.addr = ADS1115_I2C_ADDR;
        tx_msg.hdr.slave.fmt = I2C_ADDRFMT_7BIT; 
        tx_msg.hdr.len = 3;                      
        tx_msg.hdr.stop = 1;                     
        
        tx_msg.buf[0] = 0x01; // Pointer: Config Register
        tx_msg.buf[1] = 0xC3; // MSB
        tx_msg.buf[2] = 0x83; // LSB
        
        // QNX I2C devctl path: transfer details are handled by the kernel driver,
        // and DMA is leveraged when the active controller driver supports it.
        if (devctl(fd, DCMD_I2C_SEND, &tx_msg, sizeof(tx_msg), NULL) != 0) {
            return 0.0;
        }

        usleep(10000); 

        // 2. Write Address Pointer (0x00) and Read 2 Bytes
        struct {
            i2c_sendrecv_t hdr;
            uint8_t buf[2]; 
        } txrx_msg;
        
        txrx_msg.hdr.slave.addr = ADS1115_I2C_ADDR;
        txrx_msg.hdr.slave.fmt = I2C_ADDRFMT_7BIT;
        txrx_msg.hdr.send_len = 1; 
        txrx_msg.hdr.recv_len = 2; 
        txrx_msg.hdr.stop = 1;
        
        txrx_msg.buf[0] = 0x00; // Pointer: Conversion Register
        
        if (devctl(fd, DCMD_I2C_SENDRECV, &txrx_msg, sizeof(txrx_msg), NULL) != 0) {
            return 0.0;
        }

        int16_t raw_adc = (txrx_msg.buf[0] << 8) | txrx_msg.buf[1];
        
        float voltage = raw_adc * (4.096 / 32768.0);
        float original_v = voltage * 1.5; 
        float current = (original_v - 2.5) / 0.100;
        
        return (current < 0) ? 0.0 : current;
    }

    float hw_read_temp_i2c() {
        uint8_t data[2];

        if (i2c_write_read(TMP102_I2C_ADDR, 0x00, data, 2) != 0)
            return 0.0f;

        int16_t raw = (int16_t)((data[0] << 8) | data[1]);
        raw >>= 4;
        if (raw & 0x800)
            raw |= 0xF000;

        return (float)raw * 0.0625f;
    }

    // Compatibility wrapper for existing call sites.
    float hw_read_temp_1wire(int pin) {
        (void)pin;
        return hw_read_temp_i2c();
    }

// ============================================================
// LINUX IMPLEMENTATION (Mock functions for PC cross-compiling)
// ============================================================
#else
    int hw_init() { return 0; }
    int hw_read_pin(int pin) { (void)pin; return 0; }
    void hw_configure_pin(int pin, int direction) { (void)pin; (void)direction; }
    void hw_write_pin(int pin, int val) { (void)pin; (void)val; }
    
    float hw_read_vibration_i2c() { return 0.2f; }
    float hw_read_current_i2c() { return 10.5; }
    float hw_read_temp_i2c() { return 35.2f; }
    float hw_read_temp_1wire(int pin) { (void)pin; return 35.2; } 
#endif

// ============================================================
// SHARED HELPER (Runs on both QNX and Linux)
// ============================================================
const char* health_to_string(HealthStatus status) {
    switch(status) {
        case HEALTH_HEALTHY:  return "HEALTHY";
        case HEALTH_WARNING:  return "WARNING";
        case HEALTH_CRITICAL: return "CRITICAL";
        case HEALTH_FAULT:    return "FAULT";
        default:              return "UNKNOWN";
    }
}
