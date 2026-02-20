#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "sensor_manager.h"
#include "sensors.h"

// Internal DB
typedef struct {
    char unit_id[32];
    int vibration_pin;
    int sound_pin;
    int temp_pin;
    
    int vib_pulse_count;
    int sound_high_samples;
    int total_samples;
    
    float current_temp;
    float current_amps;
} MonitoredUnit;

static MonitoredUnit unit_db[MAX_UNITS];
static int unit_count = 0;
static pthread_t monitor_thread;
static int thread_running = 0;
static pthread_mutex_t data_lock = PTHREAD_MUTEX_INITIALIZER;



// Background Thread
void* background_poller(void* arg) {
    (void)arg;
    int slow_loop_counter = 0;

    while (thread_running) {
        pthread_mutex_lock(&data_lock);
        
        for (int i = 0; i < unit_count; i++) {
            // Fast polling (1ms) for digital signals
            if (hw_read_pin(unit_db[i].vibration_pin)) unit_db[i].vib_pulse_count++;
            if (hw_read_pin(unit_db[i].sound_pin))     unit_db[i].sound_high_samples++;
            unit_db[i].total_samples++;

            // Slow polling (1000ms) for Analog/I2C/1-Wire to prevent blocking the thread
            if (slow_loop_counter >= 1000) {
                unit_db[i].current_temp = hw_read_temp_1wire(unit_db[i].temp_pin);
                unit_db[i].current_amps = hw_read_current_i2c();
            }
        }
        
        if (slow_loop_counter >= 1000) {
            slow_loop_counter = 0;
        } else {
            slow_loop_counter++;
        }
        
        pthread_mutex_unlock(&data_lock);
        usleep(1000); // 1ms sample rate
    }
    return NULL;
}

void manager_init(SensorManager *mgr) {
    (void)mgr;
    if (hw_init() != 0) fprintf(stderr, "[HW] Failed to init GPIO\n");
    if (!thread_running) {
        thread_running = 1;
        pthread_create(&monitor_thread, NULL, background_poller, NULL);
    }
}

// Updated registration to include temp_pin
int manager_register_unit(SensorManager *mgr, const char* id, int vib_pin, int sound_pin, int temp_pin) {
    (void)mgr;
    if (unit_count >= MAX_UNITS) return 0;
    pthread_mutex_lock(&data_lock);
    
    strncpy(unit_db[unit_count].unit_id, id, 31);
    unit_db[unit_count].vibration_pin = vib_pin;
    unit_db[unit_count].sound_pin = sound_pin;
    unit_db[unit_count].temp_pin = temp_pin;
    
    unit_db[unit_count].vib_pulse_count = 0;
    unit_db[unit_count].total_samples = 0;
    unit_db[unit_count].current_temp = 0.0;
    unit_db[unit_count].current_amps = 0.0;
    
    unit_count++;
    pthread_mutex_unlock(&data_lock);
    return 1;
}

int manager_get_health(SensorManager *mgr, const char* unit_id, EquipmentHealth *out_health) {
    (void)mgr;
    pthread_mutex_lock(&data_lock);
    for (int i = 0; i < unit_count; i++) {
        if (strcmp(unit_db[i].unit_id, unit_id) == 0) {
            
            double norm = (unit_db[i].total_samples > 0) ? (1000.0 / unit_db[i].total_samples) : 1.0;
            double vib = unit_db[i].vib_pulse_count * norm;
            double snd = (unit_db[i].total_samples > 0) ? 
                         ((double)unit_db[i].sound_high_samples / unit_db[i].total_samples * 100.0) : 0.0;

            strncpy(out_health->unit_id, unit_db[i].unit_id, 31);
            out_health->snapshot.vibration_level = vib;
            out_health->snapshot.sound_level = snd;
            out_health->snapshot.temperature_c = unit_db[i].current_temp;
            out_health->snapshot.current_a = unit_db[i].current_amps;

            out_health->message[0] = '\0'; // Clear previous message

            // Combined Threshold Logic
            if (vib > 200 || snd > 80 || unit_db[i].current_amps > 15.0 || unit_db[i].current_temp > 80.0) {
                out_health->status = HEALTH_CRITICAL;
                strcpy(out_health->message, "CRITICAL FAULT DETECTED");
            } 
            else if (vib > 100 || snd > 50 || unit_db[i].current_amps > 12.0 || unit_db[i].current_temp > 65.0) {
                out_health->status = HEALTH_WARNING;
            } 
            else {
                out_health->status = HEALTH_HEALTHY;
            }

            // Reset fast polling counters (Do not reset current/temp, they update 1x per sec)
            unit_db[i].vib_pulse_count = 0;
            unit_db[i].sound_high_samples = 0;
            unit_db[i].total_samples = 0;

            pthread_mutex_unlock(&data_lock);
            return 1;
        }
    }
    pthread_mutex_unlock(&data_lock);
    return 0;
}

int manager_list_units(SensorManager *mgr, char list[][MAX_ID_LENGTH], int max_count) {
    (void)mgr;
    int count = 0;
    pthread_mutex_lock(&data_lock);
    for (int i = 0; i < unit_count && i < max_count; i++) {
        strncpy(list[i], unit_db[i].unit_id, MAX_ID_LENGTH);
        count++;
    }
    pthread_mutex_unlock(&data_lock);
    return count;
}
