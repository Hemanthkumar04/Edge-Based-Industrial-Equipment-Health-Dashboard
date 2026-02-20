#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>

// ============================================================
// CONSTANTS & HARDWARE MAPPING
// ============================================================
#define PIN_VIBRATION 17  // Physical Pin 11
#define PIN_SOUND     27  // Physical Pin 13
#define PIN_TEMP_1W   4   // Physical Pin 7 (DS18B20)

#define MAX_ID_LENGTH 32
#define MAX_UNITS     5

// ============================================================
// DATA STRUCTURES
// ============================================================

/**
 * HealthStatus: Enumerates the evaluated state of a piece of equipment.
 */
typedef enum {
    HEALTH_HEALTHY = 0,
    HEALTH_WARNING,
    HEALTH_CRITICAL,
    HEALTH_FAULT
} HealthStatus;

/**
 * SensorSnapshot: Represents a point-in-time capture of all sensor data.
 */
typedef struct {
    float vibration_level; // Events per second
    float sound_level;     // Duty cycle percentage (0-100)
    float temperature_c;   // Degrees Celsius
    float current_a;       // Amperes
} SensorSnapshot;

/**
 * EquipmentHealth: The unified packet sent over the protocol.
 */
typedef struct {
    char unit_id[MAX_ID_LENGTH];
    HealthStatus status;
    SensorSnapshot snapshot;
    char message[128];     // Descriptive fault message
} EquipmentHealth;

// ============================================================
// HARDWARE ABSTRACTION LAYER (HAL)
// ============================================================

int hw_init();
void hw_configure_pin(int pin, int direction);
int hw_read_pin(int pin);
void hw_write_pin(int pin, int val);
float hw_read_current_i2c();
float hw_read_temp_1wire(int pin);
const char* health_to_string(HealthStatus status);

#endif // SENSORS_H
