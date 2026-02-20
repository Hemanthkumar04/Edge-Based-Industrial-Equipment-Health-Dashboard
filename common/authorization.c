#include <stdio.h>
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include "authorization.h"

// ============================================================
// ROLE MAPPING
// ============================================================
const char* role_to_string(UserRole role) {
    switch(role) {
        case ROLE_ADMIN:    return "ADMIN";
        case ROLE_OPERATOR: return "OPERATOR";
        case ROLE_VIEWER:   return "VIEWER";
        default:            return "UNAUTHORIZED";
    }
}

// ============================================================
// CERTIFICATE PARSING LOGIC
// ============================================================
int authorize_client(SSL *ssl, ClientIdentity *out_id) {
    X509 *cert = SSL_get_peer_certificate(ssl);
    if (!cert) {
        return -1; // No certificate provided
    }

    // 1. Extract Common Name (CN)
    X509_NAME_get_text_by_NID(X509_get_subject_name(cert), 
                              NID_commonName, 
                              out_id->common_name, 
                              sizeof(out_id->common_name));

    // 2. Extract Organization Unit (OU) to determine Role
    char ou_buf[64] = {0};
    X509_NAME_get_text_by_NID(X509_get_subject_name(cert), 
                              NID_organizationalUnitName, 
                              ou_buf, 
                              sizeof(ou_buf));

    // 3. Assign Internal Role
    if (strcmp(ou_buf, "ADMIN") == 0) {
        out_id->role = ROLE_ADMIN;
    } else if (strcmp(ou_buf, "OPERATOR") == 0) {
        out_id->role = ROLE_OPERATOR;
    } else {
        out_id->role = ROLE_VIEWER;
    }

    X509_free(cert);
    return 0; // Success
}
