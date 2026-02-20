#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <pthread.h>
#include "sensors.h"

// ============================================================
// DATA STRUCTURES
// ============================================================

/**
 * SensorManager: Handle for managing background polling threads.
 */
typedef struct {
    pthread_t thread_id;
    int is_running;
} SensorManager;

// ============================================================
// PUBLIC API PROTOTYPES
// ============================================================

/**
 * manager_init: Starts the 1kHz background polling thread.
 * Returns 0 on success, -1 on hardware/thread failure.
 */
int manager_init(SensorManager* mgr);

/**
 * manager_get_health: Thread-safe retrieval of the latest sensor snapshot.
 * Returns 1 if data retrieved, 0 if unit_id not found.
 */
int manager_get_health(SensorManager* mgr, const char* unit_id, EquipmentHealth* out_health);

/**
 * manager_list_units: populates the list with registered equipment IDs.
 * Returns the number of units found.
 */
int manager_list_units(SensorManager* mgr, char list[MAX_UNITS][MAX_ID_LENGTH], int max_units);

/**
 * manager_cleanup: Signals thread to stop and joins it safely.
 */
void manager_cleanup(SensorManager* mgr);

#endif // SENSOR_MANAGER_H
