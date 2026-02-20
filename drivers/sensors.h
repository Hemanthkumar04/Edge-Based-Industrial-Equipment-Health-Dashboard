#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>
#include <time.h>

// Health states for the equipment
typedef enum {
    HEALTH_HEALTHY,
    HEALTH_WARNING,
    HEALTH_CRITICAL,
    HEALTH_FAULT
} HealthStatus;

// Data structure holding the exact readings at a given moment
typedef struct {
    double vibration_level; // Events per second
    double sound_level;     // Duty cycle %
    float temperature_c;    // Celsius (from DS18B20)
    float current_a;        // Amperes (from ACS712 via ADS1115)
    time_t timestamp;
} SensorSnapshot;

// Complete health profile for a specific unit
typedef struct {
    char unit_id[32];
    HealthStatus status;
    SensorSnapshot snapshot;
    char message[128];      // Alert/Warning description
} EquipmentHealth;

// ============================================================
// Hardware Abstraction Layer (HAL) Prototypes
// ============================================================

// Base GPIO
int hw_init();
void hw_configure_pin(int pin, int direction);
int hw_read_pin(int pin);
void hw_write_pin(int pin, int val);

// New Advanced Sensors
float hw_read_current_i2c();          // Reads from ADS1115 over I2C
float hw_read_temp_1wire(int pin);    // Reads from DS18B20 over 1-Wire

// ============================================================
// Shared Helpers
// ============================================================
const char* health_to_string(HealthStatus status);

#endif // SENSORS_H
