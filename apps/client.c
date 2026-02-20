#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <termios.h>

#define PORT 8080
#define CLIENT_CERT "certs/client.crt"
#define CLIENT_KEY  "certs/client.key"
#define CA_CERT     "certs/ca.crt"
#define EOM_MARKER  '\x03'

struct termios orig_termios;

void reset_terminal_mode() {
    tcsetattr(0, TCSANOW, &orig_termios);
}

void set_conio_terminal_mode() {
    struct termios new_termios;
    tcgetattr(0, &orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));
    atexit(reset_terminal_mode);
    // Disable canonical mode (line buffering) and echoing
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &new_termios);
}

void init_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl() {
    EVP_cleanup();
}

SSL_CTX* create_context() {
    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

void configure_context(SSL_CTX *ctx) {
    if (SSL_CTX_use_certificate_file(ctx, CLIENT_CERT, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    if (SSL_CTX_use_PrivateKey_file(ctx, CLIENT_KEY, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    if (!SSL_CTX_load_verify_locations(ctx, CA_CERT, NULL)) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <server_ip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_ip = argv[1];
    int sock;
    struct sockaddr_in addr;
    SSL_CTX *ctx;

    init_openssl();
    ctx = create_context();
    configure_context(ctx);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Unable to create socket");
        exit(EXIT_FAILURE);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, server_ip, &addr.sin_addr);

    printf("[INFO] Connecting securely to %s:%d...\n", server_ip, PORT);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);

    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
    } else {
        printf("\n✓ Connected securely to server.\n");
        set_conio_terminal_mode();
        printf("Type 'help' for available commands:\n\n");

        char rx_buf[4096];
        char tx_buf[1024];
        int tx_ptr = 0;
        fd_set readfds;
        int max_fd = (sock > STDIN_FILENO) ? sock : STDIN_FILENO;
        int in_monitor_mode = 0;

        printf("IMS> ");
        fflush(stdout);

        while (1) {
            FD_ZERO(&readfds);
            FD_SET(STDIN_FILENO, &readfds);
            FD_SET(sock, &readfds);

            if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
                perror("select");
                break;
            }

            // --- 1. Data from Server ---
            if (FD_ISSET(sock, &readfds)) {
                int bytes = SSL_read(ssl, rx_buf, sizeof(rx_buf) - 1);
                if (bytes <= 0) {
                    printf("\n[SERVER] Connection closed.\n");
                    break;
                }
                for (int i = 0; i < bytes; i++) {
                    if (rx_buf[i] == EOM_MARKER) {
                        in_monitor_mode = 0;
                        printf("\nIMS> ");
                    } else {
                        putchar(rx_buf[i]);
                    }
                }
                fflush(stdout);
            }

           // --- 2. Keyboard Input ---
            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                char c;
                if (read(STDIN_FILENO, &c, 1) > 0) {
                    if (c == '\n' || c == '\r') {
                        tx_buf[tx_ptr] = '\0';
                        if (tx_ptr > 0) {
                            if (strncmp(tx_buf, "monitor", 7) == 0) in_monitor_mode = 1;
                            SSL_write(ssl, tx_buf, (int)strlen(tx_buf));
                        } else if (in_monitor_mode) {
                            // Instant interrupt for monitor mode
                            SSL_write(ssl, "\n", 1);
                        }
                        
                        if (strcmp(tx_buf, "quit") == 0) break;
                        
                        tx_ptr = 0;
                        if (!in_monitor_mode) printf("\nIMS> ");
                    } 
                    else if (c == 127 || c == 8) { // Backspace handling
                        if (tx_ptr > 0) {
                            tx_ptr--;
                            // Only visually erase if we aren't in monitor mode
                            if (!in_monitor_mode) printf("\b \b");
                        }
                    } 
                    else {
                        // FIX: Cast sizeof to int to resolve signed/unsigned warning
                        if (tx_ptr < (int)sizeof(tx_buf) - 1) {
                            tx_buf[tx_ptr++] = c;
                            if (!in_monitor_mode) putchar(c);
                        }
                    }
                    fflush(stdout);
                }
            }        }
    }
    printf("\n");
    reset_terminal_mode();
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sock);
    SSL_CTX_free(ctx);
    cleanup_openssl();
    return 0;
}
