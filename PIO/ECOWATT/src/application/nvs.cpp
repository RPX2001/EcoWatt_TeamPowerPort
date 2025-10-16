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
    static RegID defaultRegs[REG_MAX] = {REG_VAC1, REG_IAC1, REG_IPV1, REG_PAC, REG_IPV2, REG_TEMP, REG_NONE, REG_NONE, REG_NONE, REG_NONE};
    uint8_t defaultRegCount = 6;

    uint16_t default_regs_bitmask = 0;
    for (size_t i=0; i<REG_MAX; i++)
    {
        if (defaultRegs[i] != REG_NONE) {
            default_regs_bitmask |= (1 << defaultRegs[i]);
        }
    }
    
    // Try to open read namespace
    if (!esp_prefs_nvs.begin("readregs", true)) {
        // cannot open NVS, return defaults
        return defaultRegs;
    }

    if (!esp_prefs_nvs.isKey("regs")) {
        // write default selection so future boots have it
        esp_prefs_nvs.end();
        if (!esp_prefs_nvs.begin("readregs", false)) {
            return defaultRegs;
        }

        esp_prefs_nvs.putInt("reg_count", defaultRegCount);
        esp_prefs_nvs.putUInt("regs", default_regs_bitmask);
        esp_prefs_nvs.end();
        return defaultRegs;
    }

    size_t stored_count = esp_prefs_nvs.getInt("reg_count", 0);
    if (stored_count == 0) {
        esp_prefs_nvs.end();
        return defaultRegs;
    }

    // allocate and read stored regs
    static RegID stored_regs[REG_MAX] = {REG_NONE, REG_NONE, REG_NONE, REG_NONE, REG_NONE, REG_NONE, REG_NONE, REG_NONE, REG_NONE, REG_NONE};

    uint16_t stored_regslist = esp_prefs_nvs.getUInt("regs", default_regs_bitmask);
    esp_prefs_nvs.end();

    size_t index = 0;
    for (RegID rid = REG_VAC1; rid < REG_MAX; rid = static_cast<RegID>(rid + 1)) {
        if (stored_regslist & (1 << rid)) {
            if (index < REG_MAX) {
                stored_regs[index++] = rid;
            }
        }
    }

    // (optional) ensure the rest are REG_NONE
    for (; index < REG_MAX; index++) {
        stored_regs[index] = REG_NONE;
    }

    return stored_regs;
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


bool nvs::saveReadRegs(uint16_t regMask, size_t count) 
{
    if (regMask == 0 || count == 0 || count > REG_MAX+1) 
    {
        return false; // Invalid input
    }

    if (!esp_prefs_nvs.begin("readregs", false)) // Open NVS in read-write mode
    {
        return false; // Failed to open NVS
    }

    esp_prefs_nvs.putUInt("regs", regMask);
    esp_prefs_nvs.putInt("reg_count", count);
    esp_prefs_nvs.end();

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
