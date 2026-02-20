#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "sensor_manager.h"
#include "sensors.h"

// ANSI Color Codes
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RESET   "\x1b[0m"

void print_health(const EquipmentHealth *h) {
    const char *color = ANSI_COLOR_RESET;
    const char *status_str = "UNKNOWN";

    switch (h->status) {
        case HEALTH_HEALTHY:
            color = ANSI_COLOR_GREEN;
            status_str = "HEALTHY";
            break;
        case HEALTH_WARNING:
            color = ANSI_COLOR_YELLOW;
            status_str = "WARNING";
            break;
        case HEALTH_CRITICAL:
        case HEALTH_FAULT:
            color = ANSI_COLOR_RED;
            status_str = "CRITICAL";
            break;
        default:
            status_str = "UNKNOWN";
            break;
    }

    printf("------------------------------------------------\n");
    printf("Equipment : %s\n", h->unit_id);
    printf("Status    : %s%s%s\n", color, status_str, ANSI_COLOR_RESET);
    printf("Vibration : %.0f events/s\n", h->snapshot.vibration_level);
    printf("Sound     : %.1f %%\n", h->snapshot.sound_level);
    printf("Temp      : %.1f C\n", h->snapshot.temperature_c);
    printf("Current   : %.2f A\n", h->snapshot.current_a);
    
    // Print critical alert messages if they exist
    if (h->status == HEALTH_CRITICAL && strlen(h->message) > 0) {
        printf("Message   : %s%s%s\n", color, h->message, ANSI_COLOR_RESET);
    }
    printf("------------------------------------------------\n");
}

int main() {
    printf("Sentinel-RT Hardware Test (C Version)\n");
    printf("Initializing Sensor Manager...\n");

    SensorManager manager;
    manager_init(&manager);

    // Register Sentinel-RT 
    // Pin 17 = Vib, Pin 27 = Sound, Pin 4 = Temperature
    if (!manager_register_unit(&manager, "Sentinel-RT", 17, 27, 4)) {
        fprintf(stderr, "Failed to register unit 'Sentinel-RT'\n");
        return 1;
    }

    printf("Sensors initialized.\n");
    printf(" - Digital polling: 1kHz (Vibration/Sound)\n");
    printf(" - Analog polling: 1Hz (Temp/Current)\n");
    printf("Starting loop. Press Ctrl+C to stop.\n\n");

    EquipmentHealth health;

    while (1) {
        // 1. Wait for data to accumulate (Sample window = 1.0s)
        // This matches the slow_loop_counter in sensor_manager.c
        sleep(1); 

        // 2. Get the health report
        if (manager_get_health(&manager, "Sentinel-RT", &health)) {
            print_health(&health);
        } else {
            fprintf(stderr, "Error: Could not retrieve health for Sentinel-RT\n");
        }
    }

    return 0;
}
