#include "application/nvs.h"

// Global NVS instance definition
Preferences esp_prefs_nvs;

nvs::nvs() {}

uint8_t nvs::getReadRegCount()
{
    // Open namespace in read-only mode to check stored value
    if (!esp_prefs_nvs.begin("readregs", true)) {
        // if begin failed, fall back to default
        return 6;
    }

    if (!esp_prefs_nvs.isKey("reg_count")) {
        esp_prefs_nvs.end();
        return 6;
    }

    uint8_t stored_count = esp_prefs_nvs.getInt("reg_count", 0);
    esp_prefs_nvs.end();
    return stored_count;
}

const RegID* nvs::getReadRegs()
{
    static RegID defaultRegs[] = {REG_VAC1, REG_IAC1, REG_IPV1, REG_PAC, REG_IPV2, REG_TEMP};
    // Try to open read namespace
    if (!esp_prefs_nvs.begin("readregs", true)) {
        // cannot open NVS, return defaults
        return defaultRegs;
    }

    if (!esp_prefs_nvs.isKey("read_regs")) {
        // write default selection so future boots have it
        esp_prefs_nvs.end();
        if (!esp_prefs_nvs.begin("readregs", false)) {
            return defaultRegs;
        }
        esp_prefs_nvs.putInt("reg_count", sizeof(defaultRegs) / sizeof(defaultRegs[0]));
        esp_prefs_nvs.putBytes("read_regs", defaultRegs, sizeof(defaultRegs));
        esp_prefs_nvs.end();
        return defaultRegs;
    }

    size_t stored_count = esp_prefs_nvs.getInt("reg_count", 0);
    if (stored_count == 0) {
        esp_prefs_nvs.end();
        return defaultRegs;
    }

    // allocate and read stored regs
    RegID* stored_regs = new RegID[stored_count];
    size_t bytes_read = esp_prefs_nvs.getBytes("read_regs", stored_regs, stored_count * sizeof(RegID));
    esp_prefs_nvs.end();

    if (bytes_read == stored_count * sizeof(RegID)) {
        return stored_regs;
    } else {
        delete[] stored_regs;
        return defaultRegs;
    }
}


uint64_t nvs::getPollFreq()
{
    uint64_t defaultPollFreq = 2000000;   // 2 seconds
    // Open in read-only mode
    if (!esp_prefs_nvs.begin("freq", true)) {
        return defaultPollFreq;
    }

    if (!esp_prefs_nvs.isKey("poll_freq")) {
        // write default value so future calls will see it
        esp_prefs_nvs.end();
        if (!esp_prefs_nvs.begin("freq", false)) {
            return defaultPollFreq;
        }
        esp_prefs_nvs.putULong64("poll_freq", defaultPollFreq);
        esp_prefs_nvs.end();
        return defaultPollFreq;
    }

    uint64_t stored_freq = esp_prefs_nvs.getULong64("poll_freq", defaultPollFreq);
    esp_prefs_nvs.end();

    if (stored_freq >= MIN_POLL_FREQ) // Minimum 100ms
    {
        return stored_freq;
    }
    return defaultPollFreq;
}


uint64_t nvs::getUploadFreq()
{
    uint64_t defaultUploadFreq = 15000000;   // 15 seconds
    // Open in read-only mode
    if (!esp_prefs_nvs.begin("freq", true)) {
        return defaultUploadFreq;
    }

    if (!esp_prefs_nvs.isKey("upload_freq")) {
        esp_prefs_nvs.end();
        if (!esp_prefs_nvs.begin("freq", false)) {
            return defaultUploadFreq;
        }
        esp_prefs_nvs.putULong64("upload_freq", defaultUploadFreq);
        esp_prefs_nvs.end();
        return defaultUploadFreq;
    }

    uint64_t stored_freq = esp_prefs_nvs.getULong64("upload_freq", defaultUploadFreq);
    esp_prefs_nvs.end();

    if (stored_freq >= MIN_UPLOAD_FREQ) // Minimum 1 second
    {
        return stored_freq;
    }
    return defaultUploadFreq;
}


bool nvs::saveReadRegs(const RegID* selection, size_t count) 
{
    if (selection == nullptr || count == 0) 
    {
        return false; // Invalid input
    }

    for (size_t i = 0; i < count; i++) 
    {
        if (selection[i] >= REG_MAX) 
        {
            return false; // Invalid RegID
        }
    }

    if (!esp_prefs_nvs.begin("readregs", false)) // Open NVS in read-write mode
    {
        return false; // Failed to open NVS
    }

    size_t bytes_to_write = count * sizeof(RegID);
    size_t bytes_written = esp_prefs_nvs.putBytes("read_regs", selection, bytes_to_write);
    esp_prefs_nvs.end();

    if (bytes_to_write != bytes_written)
    {
        return false;
    }

    return true;
}

bool nvs::changePollFreq(uint64_t poll_time)
{
    if (poll_time == 0) 
    {
        return false; // Invalid input
    }

    if (!esp_prefs_nvs.begin("freq", false)) // Open NVS in read-write mode
    {
        return false; // Failed to open NVS
    }

    esp_prefs_nvs.putULong64("poll_freq", poll_time);
    esp_prefs_nvs.end();

    return true;
}


bool nvs::changeUploadFreq(uint64_t upload_time)
{
    if (upload_time == 0) 
    {
        return false; // Invalid input
    }

    if (!esp_prefs_nvs.begin("freq", false)) // Open NVS in read-write mode
    {
        return false; // Failed to open NVS
    }

    esp_prefs_nvs.putULong64("upload_freq", upload_time);
    esp_prefs_nvs.end();

    return true;
}
