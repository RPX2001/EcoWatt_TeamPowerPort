/**
 * @file protocol_adapter.h
 * @brief Network adapter interface for Inverter.
 *
 * @author Ravin, Yasith
 * @version 1.1
 * @date 2025-10-01
 *
 * @par Revision history
 * - 1.0 (2025-09-09, Ravin): Original file.
 * - 1.1 (2025-10-01, Yasith): Added detailed documentation, made portable.
 */

#ifndef PROTOCOL_ADAPTER_H
#define PROTOCOL_ADAPTER_H

#include "esp_wifi.h"
#include "esp_http_client.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <cJSON.h>

#include "esp32.h"

#define MAX_API_KEY_LENGTH 128
#define MAX_URL_LENGTH 256
#define MAX_PAYLOAD_LENGTH 512
#define MAX_RESPONSE_LENGTH 1024

extern const char* ssid;
extern const char* password;
extern char apiKey[MAX_API_KEY_LENGTH];


/**
 * @defgroup ProtocolAdapter Network / Protocol Adapter
 * @brief Functions for Wi-Fi and HTTP frame communication.
 * @{
 */

/**
 * @defgroup ProtocolAdapter_Main Main Operations
 * @brief Functions for initializing and communicating with the adapter.
 * @ingroup ProtocolAdapter
 * @{
 */

/**
 * @fn adapter_begin()
 * @brief Initializes the Wi-Fi connection.
 * 
 * @return bool Returns true if the connection is successful, false otherwise.
 */
bool adapter_begin();

/**
 * @fn adapter_writereg(const char* frame, char* response, int maxResponseLen)
 * @brief Sends a write register request to the inverter.
 * 
 * @param frame The Modbus frame to send as a hexadecimal string.
 * @param response A buffer to store the response frame as a hexadecimal string.
 * @param maxResponseLen The maximum length of the response buffer.
 * 
 * @return int Returns 200 if successful, or an error code otherwise.
 */
int adapter_writereg(const char* frame, char* response, int maxResponseLen);

/**
 * @fn adapter_readreg(const char* frame, char* response, int maxResponseLen)
 * @brief Sends a read register request to the inverter.
 * 
 * @param frame The Modbus frame to send as a hexadecimal string.
 * @param response A buffer to store the response frame as a hexadecimal string.
 * @param maxResponseLen The maximum length of the response buffer.
 * 
 * @return int Returns 200 if successful, or an error code otherwise.
 */
int adapter_readreg(const char* frame, char* response, int maxResponseLen);

/** @} */ // end of ProtocolAdapter_Main




/**
 * @defgroup ProtocolAdapter_Setters Setters
 * @brief Functions for setting adapter configuration.
 * @ingroup ProtocolAdapter
 * @{
 */

/**
 * @fn adapter_set_ssid(const char* newSSID)
 * @brief Sets the Wi-Fi SSID to the specified value.
 * 
 * @param newSSID The new SSID string.
 */
void adapter_set_ssid(const char* newSSID);

/**
 * @fn adapter_set_password(const char* newPassword)
 * @brief Sets the Wi-Fi password to the specified value.
 * 
 * @param newPassword The new password string.
 */
void adapter_set_password(const char* newPassword);

/**
 * @fn adapter_set_api_key(const char* newApiKey)
 * @brief Sets the API key to the specified value.
 * 
 * @param newApiKey The new API key string.
 */
void adapter_set_api_key(const char* newApiKey);

/** @} */ // end of ProtocolAdapter_Setters




/**
 * @defgroup ProtocolAdapter_Getters Getters
 * @brief Functions for retrieving adapter configuration.
 * @ingroup ProtocolAdapter
 * @{
 */

/**
 * @fn adapter_get_ssid(char* buf, int maxLen)
 * @brief Retrieves the current Wi-Fi SSID.
 * 
 * @param buf A buffer to store the SSID.
 * @param maxLen The maximum length of the buffer.
 */
void adapter_get_ssid(char* buf, int maxLen);

/**
 * @fn adapter_get_password(char* buf, int maxLen)
 * @brief Retrieves the current Wi-Fi password.
 * 
 * @param buf A buffer to store the password.
 * @param maxLen The maximum length of the buffer.
 */
void adapter_get_password(char* buf, int maxLen);

/**
 * @fn adapter_get_api_key(char* buf, int maxLen)
 * @brief Retrieves the current API key.
 * 
 * @param buf A buffer to store the API key.
 * @param maxLen The maximum length of the buffer.
 */
void adapter_get_api_key(char* buf, int maxLen);

/** @} */ // end of ProtocolAdapter_Getters

/** @} */ // end of ProtocolAdapter

#endif