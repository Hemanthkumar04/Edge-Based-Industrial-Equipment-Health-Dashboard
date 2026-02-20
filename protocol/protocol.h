#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <openssl/ssl.h>
#include "authorization.h"
#include "sensor_manager.h"

// ============================================================
// DATA STRUCTURES
// ============================================================

/**
 * ProtocolContext: Maintains the state for a single client connection.
 */
typedef struct {
    SSL *ssl;               // Secure socket handle
    ClientIdentity identity; // Authenticated user info (Name/Role)
    SensorManager *sensor_mgr; // Pointer to the shared hardware manager
    int running;            // Loop control flag
} ProtocolContext;

// ============================================================
// PUBLIC API PROTOTYPES
// ============================================================

/**
 * protocol_init: Prepares the context for a new session.
 */
void protocol_init(ProtocolContext *ctx, SSL *ssl, ClientIdentity id, SensorManager *mgr);

/**
 * protocol_run: The main command-processing loop. 
 * Listens for encrypted commands and routes them to implementations.
 */
void protocol_run(ProtocolContext *ctx);

// ============================================================
// COMMAND IMPLEMENTATIONS
// ============================================================

void cmd_help(ProtocolContext *ctx);
void cmd_whoami(ProtocolContext *ctx);
void cmd_list_units(ProtocolContext *ctx);
void cmd_get_sensors(ProtocolContext *ctx);
void cmd_get_health(ProtocolContext *ctx);
void cmd_get_log(ProtocolContext *ctx);
void cmd_clear_log(ProtocolContext *ctx);

/**
 * cmd_monitor: Handles real-time telemetry streaming.
 * Supports args like "20s", "5m", "1h".
 */
void cmd_monitor(ProtocolContext *ctx, const char *args);

// ============================================================
// PROTOCOL HELPERS
// ============================================================

/**
 * send_response: Sends an encrypted string to the client.
 */
void send_response(ProtocolContext *ctx, const char *msg);

/**
 * send_eom: Sends the End-of-Message marker (0x03) to reset client prompt.
 */
void send_eom(ProtocolContext *ctx);

#endif // PROTOCOL_H
