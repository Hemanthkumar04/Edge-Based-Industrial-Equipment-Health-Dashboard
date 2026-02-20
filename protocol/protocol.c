#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <openssl/ssl.h>
#include <time.h>

#include "protocol.h"
#include "authorization.h"
#include "sensor_manager.h"

#define EOM_MARKER '\x03'

/* ============================================================ */
/* Helper Functions                                             */
/* ============================================================ */
void send_response(ProtocolContext *ctx, const char *msg)
{
    if (ctx && ctx->ssl && msg)
        SSL_write(ctx->ssl, msg, (int)strlen(msg));
}

void send_eom(ProtocolContext *ctx)
{
    char marker = EOM_MARKER;
    if (ctx && ctx->ssl)
        SSL_write(ctx->ssl, &marker, 1);
}

void log_alert(const char *unit, const char *message)
{
    FILE *f = fopen("blackbox.log", "a");
    if (!f)
        return;

    time_t now = time(NULL);
    char *ts = ctime(&now);
    ts[strlen(ts) - 1] = 0;

    fprintf(f, "[%s] CRITICAL ALERT | Unit: %s | %s\n", ts, unit, message);
    fclose(f);
}

/* ============================================================ */
/* Command Handlers                                             */
/* ============================================================ */

void cmd_help(ProtocolContext *ctx)
{
    send_response(ctx, "Available commands:\n");
    send_response(ctx, "  monitor [time] - Live stream\n");
    send_response(ctx, "  list_units     - List equipment\n");
    send_response(ctx, "  get_sensors    - Raw sensors\n");
    send_response(ctx, "  get_health     - Health report\n");
    send_response(ctx, "  get_log        - Show blackbox.log\n");
    send_response(ctx, "  clear_log      - Wipe blackbox.log\n");
    send_response(ctx, "  whoami         - Identity info\n");
    send_response(ctx, "  quit           - Disconnect session\n");
    send_eom(ctx);
}

void cmd_whoami(ProtocolContext *ctx)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "User: %s | Role: %s\n",
             ctx->identity.common_name,
             role_to_string(ctx->identity.role));
    send_response(ctx, buf);
    send_eom(ctx);
}

void cmd_list_units(ProtocolContext *ctx)
{
    char list[MAX_UNITS][MAX_ID_LENGTH];
    int count = manager_list_units(ctx->sensor_mgr, list, MAX_UNITS);

    send_response(ctx, "=== Registered Units ===\n");

    for (int i = 0; i < count; i++) {
        char buf[128];
        snprintf(buf, sizeof(buf), " - %s\n", list[i]);
        send_response(ctx, buf);
    }
    send_eom(ctx);
}

void cmd_get_sensors(ProtocolContext *ctx)
{
    EquipmentHealth h;
    if (manager_get_health(ctx->sensor_mgr, "Sentinel-RT", &h)) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "Vib: %.0f | Snd: %.1f%% | Temp: %.1fC | Cur: %.2fA\n",
                 h.snapshot.vibration_level,
                 h.snapshot.sound_level,
                 h.snapshot.temperature_c,
                 h.snapshot.current_a);

        send_response(ctx, buf);
    }
    send_eom(ctx);
}

void cmd_get_health(ProtocolContext *ctx)
{
    EquipmentHealth h;
    if (manager_get_health(ctx->sensor_mgr, "Sentinel-RT", &h)) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "Status: %s | Message: %s\n",
                 health_to_string(h.status),
                 h.message);

        send_response(ctx, buf);
    }
    send_eom(ctx);
}

void cmd_get_log(ProtocolContext *ctx)
{
    FILE *f = fopen("blackbox.log", "r");
    if (!f) {
        send_response(ctx, "[INFO] Log is empty.\n");
        send_eom(ctx);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f))
        send_response(ctx, line);

    fclose(f);
    send_eom(ctx);
}

void cmd_clear_log(ProtocolContext *ctx)
{
    FILE *f = fopen("blackbox.log", "w");
    if (f)
        fclose(f);

    send_response(ctx, "[SUCCESS] Log cleared.\n");
    send_eom(ctx);
}

void cmd_monitor(ProtocolContext *ctx, const char *args)
{
    int max_ticks = -1;

    if (args && strlen(args) > 0) {
        int val;
        char unit = 's';
        int items = sscanf(args, "%d%c", &val, &unit);

        if (items >= 1) {
            if (unit == 'm')
                max_ticks = val * 60;
            else if (unit == 'h')
                max_ticks = val * 3600;
            else
                max_ticks = val;
        }
    }

    char msg[128];
    if (max_ticks > 0)
        snprintf(msg, sizeof(msg), "\n>>> MONITOR START (Limit: %s) <<<\n", args);
    else
        snprintf(msg, sizeof(msg), "\n>>> MONITOR START (Infinite) <<<\n");

    send_response(ctx, msg);
    send_response(ctx, "Press 'ENTER' to stop monitoring.\n\n");

    EquipmentHealth h;
    int fd = SSL_get_fd(ctx->ssl);
    int ticks = 0;

    while (ctx->running) {
        if (manager_get_health(ctx->sensor_mgr, "Sentinel-RT", &h)) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "[%s] Vib: %.0f | Snd: %.0f%% | Temp: %.1fC | Cur: %.2fA\n",
                     health_to_string(h.status),
                     h.snapshot.vibration_level,
                     h.snapshot.sound_level,
                     h.snapshot.temperature_c,
                     h.snapshot.current_a);

            send_response(ctx, buf);

            if (h.status == HEALTH_CRITICAL)
                log_alert(h.unit_id, h.message);
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        struct timeval tv = {1, 0};

        if (SSL_pending(ctx->ssl) > 0 ||
            select(fd + 1, &fds, NULL, NULL, &tv) > 0) {
            char dummy;
            SSL_read(ctx->ssl, &dummy, 1);
            send_response(ctx, "\n>>> MONITOR STOPPED <<<\n");
            break;
        }

        if (max_ticks > 0 && ++ticks >= max_ticks) {
            send_response(ctx, "\n>>> MONITOR TIME LIMIT REACHED <<<\n");
            break;
        }
    }

    send_eom(ctx);
}

/* ============================================================ */
/* Protocol Loop                                                */
/* ============================================================ */

void protocol_init(ProtocolContext *ctx, SSL *ssl, ClientIdentity id, SensorManager *mgr)
{
    ctx->ssl = ssl;
    ctx->identity = id;
    ctx->sensor_mgr = mgr;
    ctx->running = 1;
}

void protocol_run(ProtocolContext *ctx)
{
    char buf[1024];

    send_response(ctx, "--- Connected to Sentinel-RT Secure Server ---\n");
    send_eom(ctx);

    while (ctx->running) {
        int n = SSL_read(ctx->ssl, buf, sizeof(buf) - 1);
        if (n <= 0)
            break;

        buf[n] = 0;

        if (!strncmp(buf, "help", 4)) cmd_help(ctx);
        else if (!strncmp(buf, "monitor", 7)) cmd_monitor(ctx, buf + 7);
        else if (!strncmp(buf, "list_units", 10)) cmd_list_units(ctx);
        else if (!strncmp(buf, "get_sensors", 11)) cmd_get_sensors(ctx);
        else if (!strncmp(buf, "get_health", 10)) cmd_get_health(ctx);
        else if (!strncmp(buf, "get_log", 7)) cmd_get_log(ctx);
        else if (!strncmp(buf, "clear_log", 9)) cmd_clear_log(ctx);
        else if (!strncmp(buf, "whoami", 6)) cmd_whoami(ctx);
        else if (!strncmp(buf, "quit", 4) || !strncmp(buf, "exit", 4)) {
            send_response(ctx, "\n>>> DISCONNECTING <<<\n");
            send_eom(ctx);
            ctx->running = 0; 
            break; /* Let the server.c gracefully close the socket */
        }
        else {
            send_response(ctx, "Unknown command. Type 'help'.\n");
            send_eom(ctx);
        }
    }
}
