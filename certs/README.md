# Certificates Directory

⚠️ **This directory is intentionally empty in version control.**

## Purpose

This folder is where SSL/TLS certificates must be placed for the Industrial Monitoring System to function. Certificate files are excluded from git for security reasons.

## Required Files

Before running the server or client, you must generate the following certificates:

### Server Certificates
- `ca.crt` - Certificate Authority public certificate
- `ca.key` - Certificate Authority private key  
- `server.crt` - Server public certificate
- `server.key` - Server private key

### Client Certificates (by role)
- `admin_client.crt` / `admin_client.key` - Full administrative access
- `operator_client.crt` / `operator_client.key` - Operational control
- `maintenance_client.crt` / `maintenance_client.key` - Maintenance access
- `viewer_client.crt` / `viewer_client.key` - Read-only monitoring
- `client.crt` / `client.key` - Default client certificate

## Certificate Generation

### Option 1: Using OpenSSL (Recommended for Development)

```bash
# Navigate to the certs directory
cd certs

# 1. Generate CA key and certificate
openssl genrsa -out ca.key 2048
openssl req -new -x509 -days 365 -key ca.key -out ca.crt \
  -subj "/C=US/ST=State/L=City/O=IndustrialMonitoring/CN=RootCA"

# 2. Generate server certificate
openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr \
  -subj "/C=US/ST=State/L=City/O=IndustrialMonitoring/CN=localhost"
openssl x509 -req -days 365 -in server.csr -CA ca.crt -CAkey ca.key \
  -CAcreateserial -out server.crt

# 3. Generate client certificates (example for admin)
openssl genrsa -out admin_client.key 2048
openssl req -new -key admin_client.key -out admin_client.csr \
  -subj "/C=US/ST=State/L=City/O=IndustrialMonitoring/OU=Security/CN=admin"
openssl x509 -req -days 365 -in admin_client.csr -CA ca.crt -CAkey ca.key \
  -CAcreateserial -out admin_client.crt

# 4. Repeat step 3 for other roles (operator, maintenance, viewer, client)
# Just change the CN and filename accordingly

# 5. Clean up CSR files
rm *.csr

# 6. Set proper permissions
chmod 600 *.key
chmod 644 *.crt
```

### Option 2: Using a Custom Script

Create a script like `generate_certs.sh` in the `scripts/` directory to automate certificate generation for all roles.

### Option 3: Production Certificates

For production deployment:
1. Use certificates from a trusted Certificate Authority (CA)
2. Follow your organization's PKI policies
3. Implement proper certificate rotation procedures
4. Store private keys in a secure key management system

## Verification

After generating certificates, verify them:

```bash
# Check certificate details
openssl x509 -in server.crt -text -noout

# Verify certificate chain
openssl verify -CAfile ca.crt server.crt

# Check key matches certificate
openssl x509 -noout -modulus -in server.crt | openssl md5
openssl rsa -noout -modulus -in server.key | openssl md5
# The MD5 hashes should match
```

## Security Best Practices

✅ **DO:**
- Generate unique certificates for each environment (dev, staging, production)
- Use strong key sizes (minimum 2048-bit RSA or 256-bit ECC)
- Store private keys securely with restricted permissions (`chmod 600`)
- Rotate certificates before expiration
- Use different certificates for each client role

❌ **DON'T:**
- Commit certificate files to version control
- Share private keys via email or messaging
- Use the same certificates across environments
- Use self-signed certificates in production
- Store keys in shared/network drives without encryption

## File Permissions

After generating certificates, ensure proper permissions:

```bash
chmod 600 *.key      # Private keys - owner read/write only
chmod 644 *.crt      # Certificates - world readable
```

## Troubleshooting

**"No such file or directory" when starting server:**
- Ensure all required certificate files exist in this directory
- Check file names match exactly (case-sensitive)

**"Permission denied" errors:**
- Run `chmod 600 *.key` to fix key permissions
- Ensure the user running the server has read access

**"Certificate verification failed":**
- Verify CA certificate is correct
- Check certificate chain is complete
- Ensure certificates haven't expired: `openssl x509 -in cert.crt -noout -dates`

## Need Help?

Refer to the main README.md or open an issue on GitHub for assistance with certificate generation.

---

**Last Updated:** 2025  
**Security Notice:** Never share or commit private key files (.key)
