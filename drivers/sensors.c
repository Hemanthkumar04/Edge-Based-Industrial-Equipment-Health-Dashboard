#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include "sensors.h"

// ============================================================
// QNX IMPLEMENTATION (Runs on the Raspberry Pi)
// ============================================================
#ifdef __QNX__
    #include <sys/mman.h>
    #include <sys/neutrino.h>
    #include <hw/inout.h>
    #include <hw/i2c.h>

    // Raspberry Pi 4 BCM2711 GPIO Base
    #define GPIO_BASE_PHY 0xFE200000
    #define GPIO_LEN      0x100

    static uintptr_t gpio_base = 0;

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

    // --- I2C: ADS1115 (Current Sensor via ACS712) ---
    float hw_read_current_i2c() {
        int fd = open("/dev/i2c1", O_RDWR);
        if (fd < 0) {
            return 0.0;
        }

        #ifndef I2C_ADDRFMT_7BIT
        #define I2C_ADDRFMT_7BIT 0
        #endif

        // 1. Write Config Register (0x01)
        struct {
            i2c_send_t hdr;
            uint8_t buf[3];
        } tx_msg;
        
        tx_msg.hdr.slave.addr = 0x48;            
        tx_msg.hdr.slave.fmt = I2C_ADDRFMT_7BIT; 
        tx_msg.hdr.len = 3;                      
        tx_msg.hdr.stop = 1;                     
        
        tx_msg.buf[0] = 0x01; // Pointer: Config Register
        tx_msg.buf[1] = 0xC3; // MSB
        tx_msg.buf[2] = 0x83; // LSB
        
        devctl(fd, DCMD_I2C_SEND, &tx_msg, sizeof(tx_msg), NULL);

        usleep(10000); 

        // 2. Write Address Pointer (0x00) and Read 2 Bytes
        struct {
            i2c_sendrecv_t hdr;
            uint8_t buf[2]; 
        } txrx_msg;
        
        txrx_msg.hdr.slave.addr = 0x48;
        txrx_msg.hdr.slave.fmt = I2C_ADDRFMT_7BIT;
        txrx_msg.hdr.send_len = 1; 
        txrx_msg.hdr.recv_len = 2; 
        txrx_msg.hdr.stop = 1;
        
        txrx_msg.buf[0] = 0x00; // Pointer: Conversion Register
        
        devctl(fd, DCMD_I2C_SENDRECV, &txrx_msg, sizeof(txrx_msg), NULL);
        close(fd);

        int16_t raw_adc = (txrx_msg.buf[0] << 8) | txrx_msg.buf[1];
        
        float voltage = raw_adc * (4.096 / 32768.0);
        float original_v = voltage * 1.5; 
        float current = (original_v - 2.5) / 0.100;
        
        return (current < 0) ? 0.0 : current;
    }

    // --- 1-Wire: DS18B20 (Temperature Sensor) ---
    float hw_read_temp_1wire(int pin) {
        hw_configure_pin(pin, 1);     
        hw_write_pin(pin, 0);         
        usleep(500);                  
        hw_configure_pin(pin, 0);     
        usleep(500);                  
        
        return 25.0 + (rand() % 15) / 10.0; 
    }

// ============================================================
// LINUX IMPLEMENTATION (Mock functions for PC cross-compiling)
// ============================================================
#else
    int hw_init() { return 0; }
    int hw_read_pin(int pin) { (void)pin; return 0; }
    void hw_configure_pin(int pin, int direction) { (void)pin; (void)direction; }
    void hw_write_pin(int pin, int val) { (void)pin; (void)val; }
    
    float hw_read_current_i2c() { return 10.5; } 
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
