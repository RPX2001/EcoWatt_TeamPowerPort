#ifndef CREDENTIALS_H
#define CREDENTIALS_H

#warning: Check WiFi and server credentials in credentials.h before compiling!

// Wifi Credentials
#define WIFI_SSID "HydroBK"
#define WIFI_PASSWORD "Hydrolink123"

#define FLASK_SERVER_URL "http://10.78.228.2:5001"

// Security Credentials
// Pre-Shared Key (PSK) for HMAC-SHA256 authentication
// ⚠️ WARNING: This key must match the one on Flask server!
// ⚠️ In production, use secure key storage (encrypted NVS, secure element, etc.)
static const uint8_t HMAC_PSK[32] = {
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
    0x76, 0x2e, 0x71, 0x60, 0xf3, 0x8b, 0x4d, 0xa5,
    0x6a, 0x78, 0x4d, 0x90, 0x45, 0x19, 0x0c, 0xfe
};

// PSK as hex string (for easy sharing with Flask server)
// "2b7e151628aed2a6abf7158809cf4f3c762e7160f38b4da56a784d9045190cfe"

#endif // CREDENTIALS_H