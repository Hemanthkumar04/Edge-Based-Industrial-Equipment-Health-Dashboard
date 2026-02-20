#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "authorization.h"
#include "sensor_manager.h"
#include "protocol.h"

#define PORT 8080

/* Pointing to the centralized certs folder */
#define SERVER_CERT "certs/server.crt"
#define SERVER_KEY  "certs/server.key"
#define CA_CERT     "certs/ca.crt"

void init_openssl();
void cleanup_openssl();

/* ============================================================
 * Helper: Initialize OpenSSL Context
 * ============================================================ */
SSL_CTX* create_context() {
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = TLS_server_method();
    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

/* ============================================================
 * Helper: Configure Certificates
 * ============================================================ */
void configure_context(SSL_CTX *ctx) {
    if (SSL_CTX_use_certificate_file(ctx, SERVER_CERT, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, SERVER_KEY, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (!SSL_CTX_load_verify_locations(ctx, CA_CERT, NULL)) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    /* Force Client Authentication */
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
}

/* ============================================================
 * Helper: Create Listening Socket
 * ============================================================ */
int create_socket(int port) {
    int s;
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("Unable to create socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(s);
        exit(EXIT_FAILURE);
    }

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Unable to bind");
        close(s);
        exit(EXIT_FAILURE);
    }

    if (listen(s, 1) < 0) {
        perror("Unable to listen");
        close(s);
        exit(EXIT_FAILURE);
    }

    return s;
}

/* ============================================================
 * MAIN SERVER ENTRY
 * ============================================================ */
int main() {
    int sock;
    SSL_CTX *ctx;

    printf("[INFO] Starting Industrial Monitoring System...\n");

    /* Initialize Hardware/Sensors */
    SensorManager sensor_mgr;
    manager_init(&sensor_mgr);

    /* Register hardware unit
     * Pin 17 = Vibration, Pin 27 = Sound, Pin 4 = Temperature
     * (Current uses I2C directly)
     */
    if (manager_register_unit(&sensor_mgr, "Sentinel-RT", 17, 27, 4)) {
        printf("[INFO] Registered 'Sentinel-RT' (Vib:17, Snd:27, Temp:4, Cur:I2C)\n");
    } else {
        fprintf(stderr, "[ERROR] Failed to register equipment\n");
        return 1;
    }

    init_openssl();
    ctx = create_context();
    configure_context(ctx);

    sock = create_socket(PORT);

    printf("[INFO] Server listening on port %d...\n", PORT);

    while (1) {
        struct sockaddr_in addr;
        unsigned int len = sizeof(addr);
        int client_fd = accept(sock, (struct sockaddr*)&addr, &len);

        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }

        printf("[CONN] Connection from %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

        SSL *ssl = SSL_new(ctx);
        if (!ssl) {
            fprintf(stderr, "[ERROR] SSL_new() failed\n");
            close(client_fd);
            continue;
        }

        SSL_set_fd(ssl, client_fd);

        if (SSL_accept(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
        } else {
            /* Use authorize_client() from authorization.c to extract identity
             * and role. authorize_client() internally retrieves and frees the
             * peer certificate, so do not call SSL_get_peer_certificate() here.
             */
            ClientIdentity id;
            memset(&id, 0, sizeof(id));

            int auth_ret = authorize_client(ssl, &id);
            if (auth_ret != 0) {
                printf("[AUTH] Authorization failed or no certificate provided (code=%d)\n", auth_ret);
            } else {
                if (id.role != ROLE_UNAUTHORIZED) {
                    printf("[AUTH] Access GRANTED: %s (%s)\n", id.common_name, role_to_string(id.role));

                    ProtocolContext protocol_ctx;
                    protocol_init(&protocol_ctx, ssl, id, &sensor_mgr);
                    protocol_run(&protocol_ctx);

                    printf("[CONN] Session ended for %s\n", id.common_name);
                } else {
                    printf("[AUTH] Access DENIED: %s (Unauthorized Role)\n", id.common_name);
                }
            }
        }

        /* Shutdown & cleanup */
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_fd);
    }

    /* Cleanup (unreachable in current loop, kept for completeness) */
    close(sock);
    SSL_CTX_free(ctx);
    cleanup_openssl();
    return 0;
}

void init_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl() {
    EVP_cleanup();
}
