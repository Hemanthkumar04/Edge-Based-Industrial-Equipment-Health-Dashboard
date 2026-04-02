#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sched.h>
#include "sensor_manager.h"
#include "sensors.h"

// ============================================================
// INTERNAL STATE & THREADING
// ============================================================
static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;
static EquipmentHealth current_health;
static int running = 0;

// Thresholds
#define VIB_WARNING_THRESHOLD  100.0
#define VIB_CRITICAL_THRESHOLD 200.0
#define TMP_CRITICAL_THRESHOLD 80.0
#define CUR_CRITICAL_THRESHOLD 15.0

#define POLL_INTERVAL_NS 1000000L

static void add_ns(struct timespec *ts, long ns)
{
    ts->tv_nsec += ns;
    while (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec += 1;
        ts->tv_nsec -= 1000000000L;
    }
}

// ============================================================
// BACKGROUND POLLING THREAD (1kHz)
// ============================================================
void* polling_thread(void* arg) {
    (void)arg; // Silence the unused variable warning
    float vib_accum = 0.0f;
    uint32_t snd_counts = 0;
    int seconds_counter = 0;
    struct timespec next_tick;
    uint64_t jitter_sum_ns = 0;
    uint64_t jitter_max_ns = 0;

    printf("[SENSORS] Background polling thread started.\n");

    if (clock_gettime(CLOCK_MONOTONIC, &next_tick) != 0) {
        perror("clock_gettime failed");
        return NULL;
    }

    while (running) {
        // 1. High-Frequency Digital Polling (GPIO)
        vib_accum += hw_read_vibration_i2c();
        if (hw_read_pin(PIN_SOUND)) snd_counts++;

        // 2. Accumulation & Evaluation (Every 1 second / 1000 ticks)
        add_ns(&next_tick, POLL_INTERVAL_NS);
#ifdef __QNX__
        while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_tick, NULL) == EINTR) {
        }
#else
        if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_tick, NULL) != 0)
            usleep(1000);
#endif

        {
            struct timespec now;
            if (clock_gettime(CLOCK_MONOTONIC, &now) == 0) {
                uint64_t now_ns = (uint64_t)now.tv_sec * 1000000000ULL + (uint64_t)now.tv_nsec;
                uint64_t tgt_ns = (uint64_t)next_tick.tv_sec * 1000000000ULL + (uint64_t)next_tick.tv_nsec;
                uint64_t jitter_ns = now_ns > tgt_ns ? now_ns - tgt_ns : tgt_ns - now_ns;
                jitter_sum_ns += jitter_ns;
                if (jitter_ns > jitter_max_ns)
                    jitter_max_ns = jitter_ns;
            }
        }

        seconds_counter++;

        if (seconds_counter >= 1000) {
            pthread_mutex_lock(&data_mutex);
            
            // Populate Snapshot
            current_health.snapshot.vibration_level = vib_accum;
            current_health.snapshot.sound_level = (float)(snd_counts / 10.0); // Simple duty cycle %
            current_health.snapshot.temperature_c = hw_read_temp_i2c();
            current_health.snapshot.current_a = hw_read_current_i2c();

            // Evaluate Health Status
            if (current_health.snapshot.vibration_level > VIB_CRITICAL_THRESHOLD || 
                current_health.snapshot.current_a > CUR_CRITICAL_THRESHOLD) {
                current_health.status = HEALTH_CRITICAL;
                strcpy(current_health.message, "CRITICAL FAULT DETECTED");
            } 
            else if (current_health.snapshot.vibration_level > VIB_WARNING_THRESHOLD) {
                current_health.status = HEALTH_WARNING;
                strcpy(current_health.message, "High Vibration Warning");
            } 
            else {
                current_health.status = HEALTH_HEALTHY;
                strcpy(current_health.message, "System Nominal");
            }

            pthread_mutex_unlock(&data_mutex);

            printf("[RT] Poll loop jitter: avg=%llu us max=%llu us\n",
                   (unsigned long long)((jitter_sum_ns / 1000ULL) / 1000ULL),
                   (unsigned long long)(jitter_max_ns / 1000ULL));

            // Reset local counters for the next second
            vib_accum = 0.0f;
            snd_counts = 0;
            seconds_counter = 0;
            jitter_sum_ns = 0;
            jitter_max_ns = 0;
        }
    }
    return NULL;
}

// ============================================================
// PUBLIC API
// ============================================================

int manager_init(SensorManager* mgr) {
    pthread_attr_t attr;

    if (hw_init() != 0) return -1;

    // Initialize state
    memset(&current_health, 0, sizeof(EquipmentHealth));
    strcpy(current_health.unit_id, "Sentinel-RT");
    
    running = 1;
    mgr->is_running = 1;

    // Start background thread with explicit RT scheduling on QNX.
    if (pthread_attr_init(&attr) != 0) {
        perror("pthread_attr_init failed");
        return -1;
    }

#ifdef __QNX__
    {
        struct sched_param sp;
        memset(&sp, 0, sizeof(sp));
        sp.sched_priority = 60;

        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
        pthread_attr_setschedparam(&attr, &sp);
    }
#endif

    if (pthread_create(&mgr->thread_id, &attr, polling_thread, mgr) != 0) {
        perror("pthread_create failed");
        pthread_attr_destroy(&attr);
        running = 0;
        mgr->is_running = 0;
        return -1;
    }

    pthread_attr_destroy(&attr);

    return 0; // Success
}

int manager_get_health(SensorManager* mgr, const char* unit_id, EquipmentHealth* out_health) {
    (void)mgr;
    pthread_mutex_lock(&data_mutex);
    
    // Only return data if IDs match
    if (strcmp(current_health.unit_id, unit_id) == 0) {
        memcpy(out_health, &current_health, sizeof(EquipmentHealth));
        pthread_mutex_unlock(&data_mutex);
        return 1;
    }
    
    pthread_mutex_unlock(&data_mutex);
    return 0;
}

int manager_list_units(SensorManager* mgr, char list[MAX_UNITS][MAX_ID_LENGTH], int max_units) {
    (void)mgr; (void)max_units;
    strcpy(list[0], current_health.unit_id);
    return 1;
}

void manager_cleanup(SensorManager* mgr) {
    if (running) {
        running = 0;
        mgr->is_running = 0;
        pthread_join(mgr->thread_id, NULL);
        printf("[SENSORS] Background thread joined and cleaned up successfully.\n");
    }
}
