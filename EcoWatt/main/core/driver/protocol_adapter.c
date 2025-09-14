#include "protocol_adapter.h"

char writeURL[MAX_URL_LENGTH] = "http://20.15.114.131:8080/api/inverter/write";
char readURL[MAX_URL_LENGTH]  = "http://20.15.114.131:8080/api/inverter/read";

// retry & timeout
const int maxRetries = 3;
const int httpTimeout = 5000; // ms

bool adapter_begin() 
{
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_mode(WIFI_MODE_STA);

  wifi_config_t wifi_config = {};
  strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
  strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

  esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
  esp_wifi_start();
  esp_wifi_connect();  // Add this to initiate connection

  while (true) 
  {
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) break;
    esp32_wait(500);
  }

  return true;
}


int adapter_writereg(const char* frame, char* response, int maxResponseLen) 
{
  send_request(writeURL, frame, response, maxResponseLen);
  int err = parse_response(response);
  return err;
}


int adapter_readreg(const char* frame, char* response, int maxResponseLen) 
{
  send_request(readURL, frame, response, maxResponseLen);
  int err = parse_response(response);
  return err;
}


static void send_request(const char* url, const char* frame, char* response, int maxResponseLen) 
{
  // Check WiFi status
  wifi_ap_record_t ap_info;
  if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) 
  {
    if (maxResponseLen > 0) response[0] = '\0';
    return;
  }

  int attempt = 0;
  int backoffDelay = 500; // start with 500ms

  while (attempt < maxRetries) 
  {
    attempt++;

    esp_http_client_config_t config = { .url = url, .timeout_ms = httpTimeout };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "accept", "*/*");
    esp_http_client_set_header(client, "Authorization", apiKey);

    char payload[MAX_PAYLOAD_LENGTH];
    snprintf(payload, MAX_PAYLOAD_LENGTH, "{\"frame\": \"%s\"}", frame);
    esp_http_client_set_post_field(client, payload, strlen(payload));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) 
    {
      int len = esp_http_client_get_content_length(client);
      if (len > 0) 
      {
        if (len >= maxResponseLen) len = maxResponseLen - 1;
        int read_len = esp_http_client_read(client, response, len);
        response[read_len] = '\0';
            
        int status_code = esp_http_client_get_status_code(client);
               
        if (response[0] != '\0') 
        {
          esp_http_client_cleanup(client);
          return; // success
        }
      }
    } 
    else 
    {
      // Error handling if needed
    }

    esp_http_client_cleanup(client);

    esp32_wait(backoffDelay);
    backoffDelay *= 2;
  }

  if (maxResponseLen > 0) response[0] = '\0';
  return;
}


// Parse & Error Handling 
int parse_response(const char* response) 
{
  if (response[0] == '\0') return 458;

  cJSON* doc = cJSON_Parse(response);
  if (!doc) return 500;

  cJSON* frameItem = cJSON_GetObjectItem(doc, "frame");
  if (!frameItem || !cJSON_IsString(frameItem)) 
  {
    cJSON_Delete(doc);
    return 501;
  }

  const char* frame = frameItem->valuestring;

  // Modbus function code check
  char funcCodeStr[3] = {0};
  strncpy(funcCodeStr, frame + 2, 2);
  int funcCode = strtol(funcCodeStr, NULL, 16);
  if (funcCode & 0x80) 
  {
    char errorCodeStr[3] = {0};
    strncpy(errorCodeStr, frame + 4, 2);
    int errorCode = strtol(errorCodeStr, NULL, 16);
    return errorCode;
  } 
  else 
  {
    return 200;
  }

  cJSON_Delete(doc);
}


// Frame Validation 
static bool is_frame_valid(const char* frame) 
{
  int len = strlen(frame);
  if (len < 6) return false;

  for (int i = 0; i < len; i++) 
  {
    if (!isxdigit(frame[i])) return false;
  }
  return true;
}

/*
//  Error Printer 
static void ProtocolAdapter::printErrorCode(int code) 
{
  switch (code) 
  {
    case 0x01: Serial.println("01 - Illegal Function"); break;
    case 0x02: Serial.println("02 - Illegal Data Address"); break;
    case 0x03: Serial.println("03 - Illegal Data Value"); break;
    case 0x04: Serial.println("04 - Slave Device Failure"); break;
    case 0x05: Serial.println("05 - Acknowledge (processing delayed)"); break;
    case 0x06: Serial.println("06 - Slave Device Busy"); break;
    case 0x08: Serial.println("08 - Memory Parity Error"); break;
    case 0x0A: Serial.println("0A - Gateway Path Unavailable"); break;
    case 0x0B: Serial.println("0B - Gateway Target Device Failed to Respond"); break;
    default:   Serial.println("Unknown error code"); break;
  }
}*/

// Setters
void adapter_set_ssid(const char* newSSID) 
{ 
  ssid = newSSID; 
}

void adapter_set_password(const char* newPassword) 
{ 
  password = newPassword; 
}

void adapter_set_api_key(const char* newApiKey) 
{ 
  strncpy(apiKey, newApiKey, MAX_API_KEY_LENGTH);
  apiKey[MAX_API_KEY_LENGTH - 1] = '\0';
}

// Getters 
void adapter_get_ssid(char* buf, int maxLen) 
{ 
  strncpy(buf, ssid, maxLen);
  buf[maxLen - 1] = '\0';
}

void adapter_get_password(char* buf, int maxLen) 
{ 
  strncpy(buf, password, maxLen);
  buf[maxLen - 1] = '\0';
}

void adapter_get_api_key(char* buf, int maxLen) 
{ 
  strncpy(buf, apiKey, maxLen);
  buf[maxLen - 1] = '\0';
}