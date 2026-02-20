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

    // 1. Initialize hardware and threads (manager_init now returns int and handles registration internally)
    if (manager_init(&manager) != 0) {
        fprintf(stderr, "[ERROR] Failed to initialize Sensor Manager. Check hardware mapping.\n");
        return 1;
    }

    printf("Sensors initialized successfully.\n");
    printf(" - Digital polling: 1kHz (Vibration/Sound)\n");
    printf(" - Analog polling: 1Hz (Temp/Current)\n");
    printf("Starting loop. Press Ctrl+C to stop.\n\n");

    EquipmentHealth health;

    while (1) {
        // 2. Wait for data to accumulate (Sample window = 1.0s)
        sleep(1); 

        // 3. Get the health report
        if (manager_get_health(&manager, "Sentinel-RT", &health)) {
            print_health(&health);
        } else {
            fprintf(stderr, "Error: Could not retrieve health for Sentinel-RT\n");
        }
    }

    // Cleanup (Unreachable due to while(1), but good practice)
    manager_cleanup(&manager);
    return 0;
}
