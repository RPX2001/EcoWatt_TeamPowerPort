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

const char* ssid;
const char* password;
char apiKey[MAX_API_KEY_LENGTH] = "";

bool adapter_begin();
int adapter_writereg(const char* frame, char* response, int maxResponseLen);
int adapter_readreg(const char* frame, char* response, int maxResponseLen);
int parse_response(const char* response);

void adapter_set_ssid(const char* newSSID);
void adapter_set_password(const char* newPassword);
void adapter_set_api_key(const char* newApiKey);

void adapter_get_ssid(char* buf, int maxLen);
void adapter_get_password(char* buf, int maxLen);
void adapter_get_api_key(char* buf, int maxLen);

#endif