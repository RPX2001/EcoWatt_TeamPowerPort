#include "application/nvs.h"

// Global NVS instance definition
Preferences esp_prefs_nvs;

nvs::nvs() {}

uint8_t nvs::getReadRegCount()
{
    if (!esp_prefs_nvs.isKey("reg_count"))
    {
        return 6;
    }
    else
    {
        esp_prefs_nvs.begin("readregs", true); // Open NVS in read-only mode
        uint8_t stored_count = esp_prefs_nvs.getInt("reg_count", 0);
        esp_prefs_nvs.end();
        return stored_count;
    }
}

const RegID* nvs::getReadRegs()
{
    static RegID defaultRegs[] = {REG_VAC1, REG_IAC1, REG_IPV1, REG_PAC, REG_IPV2, REG_TEMP};

    if (!esp_prefs_nvs.isKey("read_regs"))
    {
        esp_prefs_nvs.begin("readregs", false); // Open NVS in read-write mode

        size_t bytes_to_write = sizeof(defaultRegs);
        esp_prefs_nvs.putInt("reg_count", sizeof(defaultRegs) / sizeof(defaultRegs[0]));
        size_t bytes_written = esp_prefs_nvs.putBytes("read_regs", defaultRegs, bytes_to_write);

        esp_prefs_nvs.end();
        
        return defaultRegs;
    }
    else
    {
        esp_prefs_nvs.begin("readregs", true); // Open NVS in read-only mode

        size_t stored_count = esp_prefs_nvs.getInt("reg_count", 0);
        static RegID* stored_regs = nullptr;
        
        // Clean up previous allocation
        if (stored_regs != nullptr) {
            delete[] stored_regs;
        }
        
        stored_regs = new RegID[stored_count];
        size_t bytes_read = esp_prefs_nvs.getBytes("read_regs", stored_regs, stored_count * sizeof(RegID));
        esp_prefs_nvs.end();

        if (bytes_read == stored_count * sizeof(RegID))
        {
            return stored_regs;
        }
        else
        {
            return defaultRegs;
        }
    }
}


uint64_t nvs::getPollFreq()
{
    uint64_t defaultPollFreq = 2000000;   // 2 seconds

    if (!esp_prefs_nvs.isKey("poll_freq"))
    {
        esp_prefs_nvs.begin("freq", false); // Open NVS in read-write mode

        esp_prefs_nvs.putULong64("poll_freq", defaultPollFreq);
        esp_prefs_nvs.end();

        return defaultPollFreq;
    }
    else
    {
        esp_prefs_nvs.begin("freq", true); // Open NVS in read-only mode

        uint64_t stored_freq = esp_prefs_nvs.getULong64("poll_freq", defaultPollFreq);
        esp_prefs_nvs.end();

        if (stored_freq > MIN_POLL_FREQ) // Minimum 100ms
        {
            return stored_freq;
        }
        else
        {
            return defaultPollFreq;
        }        
    }
}


uint64_t nvs::getUploadFreq()
{
    uint64_t defaultUploadFreq = 15000000;   // 15 seconds

    if (!esp_prefs_nvs.isKey("upload_freq"))
    {
        esp_prefs_nvs.begin("freq", false); // Open NVS in read-write mode

        esp_prefs_nvs.putULong64("upload_freq", defaultUploadFreq);
        esp_prefs_nvs.end();

        return defaultUploadFreq;
    }
    else
    {
        esp_prefs_nvs.begin("freq", true); // Open NVS in read-only mode

        uint64_t stored_freq = esp_prefs_nvs.getULong64("upload_freq", defaultUploadFreq);
        esp_prefs_nvs.end();

        if (stored_freq > MIN_UPLOAD_FREQ) // Minimum 1 second
        {
            return stored_freq;
        }
        else
        {
            return defaultUploadFreq;
        }
    }
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

    size_t bytes_written = esp_prefs_nvs.putULong64("poll_freq", poll_time);
    esp_prefs_nvs.end();

    if (bytes_written != sizeof(uint64_t))
    {
        return false;
    }

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

    size_t bytes_written = esp_prefs_nvs.putULong64("upload_freq", upload_time);
    esp_prefs_nvs.end();

    if (bytes_written != sizeof(uint64_t))
    {
        return false;
    }

    return true;
}
