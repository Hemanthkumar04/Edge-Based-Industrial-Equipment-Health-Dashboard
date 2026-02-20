#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "sensors.h"

#define MAX_UNITS 10
#define MAX_ID_LENGTH 32

typedef struct {
    int dummy; // Placeholder
} SensorManager;

void manager_init(SensorManager *mgr);

// UPDATED: Added temp_pin parameter for the DS18B20 sensor
int manager_register_unit(SensorManager *mgr, const char *unit_id, int vib_pin, int snd_pin, int temp_pin);

int manager_get_health(SensorManager *mgr, const char *unit_id, EquipmentHealth *out_health);

int manager_list_units(SensorManager *mgr, char list[][MAX_ID_LENGTH], int max_count);

#endif // SENSOR_MANAGER_H
