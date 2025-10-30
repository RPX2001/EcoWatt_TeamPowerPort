#include "application/nvs.h"
#include "config/test_config.h"
#include "application/system_config.h"

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
    uint64_t defaultPollFreq = DEFAULT_POLL_FREQUENCY_US;   // From test_config.h (5 seconds)
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
    uint64_t defaultUploadFreq = DEFAULT_UPLOAD_FREQUENCY_US;   // From test_config.h (15 seconds)
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


uint64_t nvs::getConfigFreq()
{
    uint64_t defaultConfigFreq = DEFAULT_CONFIG_FREQUENCY_US;   // From system_config.h
    // Open in read-only mode
    if (!esp_prefs_nvs.begin("freq", true)) {
        return defaultConfigFreq;
    }

    if (!esp_prefs_nvs.isKey("config_freq")) {
        esp_prefs_nvs.end();
        if (!esp_prefs_nvs.begin("freq", false)) {
            return defaultConfigFreq;
        }
        esp_prefs_nvs.putULong64("config_freq", defaultConfigFreq);
        esp_prefs_nvs.end();
        return defaultConfigFreq;
    }

    uint64_t stored_freq = esp_prefs_nvs.getULong64("config_freq", defaultConfigFreq);
    esp_prefs_nvs.end();

    if (stored_freq >= MIN_CONFIG_FREQUENCY_US) // Minimum validation
    {
        return stored_freq;
    }
    return defaultConfigFreq;
}


bool nvs::changeConfigFreq(uint64_t config_time)
{
    if (config_time == 0) 
    {
        return false; // Invalid input
    }

    if (!esp_prefs_nvs.begin("freq", false)) // Open NVS in read-write mode
    {
        return false; // Failed to open NVS
    }

    esp_prefs_nvs.putULong64("config_freq", config_time);
    esp_prefs_nvs.end();

    return true;
}


uint64_t nvs::getCommandFreq()
{
    uint64_t defaultCommandFreq = DEFAULT_COMMAND_FREQUENCY_US;   // From system_config.h
    // Open in read-only mode
    if (!esp_prefs_nvs.begin("freq", true)) {
        return defaultCommandFreq;
    }

    if (!esp_prefs_nvs.isKey("command_freq")) {
        esp_prefs_nvs.end();
        if (!esp_prefs_nvs.begin("freq", false)) {
            return defaultCommandFreq;
        }
        esp_prefs_nvs.putULong64("command_freq", defaultCommandFreq);
        esp_prefs_nvs.end();
        return defaultCommandFreq;
    }

    uint64_t stored_freq = esp_prefs_nvs.getULong64("command_freq", defaultCommandFreq);
    esp_prefs_nvs.end();

    if (stored_freq >= MIN_COMMAND_FREQUENCY_US) // Minimum validation
    {
        return stored_freq;
    }
    return defaultCommandFreq;
}


bool nvs::changeCommandFreq(uint64_t command_time)
{
    if (command_time == 0) 
    {
        return false; // Invalid input
    }

    if (!esp_prefs_nvs.begin("freq", false)) // Open NVS in read-write mode
    {
        return false; // Failed to open NVS
    }

    esp_prefs_nvs.putULong64("command_freq", command_time);
    esp_prefs_nvs.end();

    return true;
}


uint64_t nvs::getOtaFreq()
{
    uint64_t defaultOtaFreq = DEFAULT_OTA_FREQUENCY_US;   // From system_config.h
    // Open in read-only mode
    if (!esp_prefs_nvs.begin("freq", true)) {
        return defaultOtaFreq;
    }

    if (!esp_prefs_nvs.isKey("ota_freq")) {
        esp_prefs_nvs.end();
        if (!esp_prefs_nvs.begin("freq", false)) {
            return defaultOtaFreq;
        }
        esp_prefs_nvs.putULong64("ota_freq", defaultOtaFreq);
        esp_prefs_nvs.end();
        return defaultOtaFreq;
    }

    uint64_t stored_freq = esp_prefs_nvs.getULong64("ota_freq", defaultOtaFreq);
    esp_prefs_nvs.end();

    if (stored_freq >= MIN_OTA_FREQUENCY_US) // Minimum validation
    {
        return stored_freq;
    }
    return defaultOtaFreq;
}


bool nvs::changeOtaFreq(uint64_t ota_time)
{
    if (ota_time == 0) 
    {
        return false; // Invalid input
    }

    if (!esp_prefs_nvs.begin("freq", false)) // Open NVS in read-write mode
    {
        return false; // Failed to open NVS
    }

    esp_prefs_nvs.putULong64("ota_freq", ota_time);
    esp_prefs_nvs.end();

    return true;
}


// ============================================
// POWER MANAGEMENT NVS FUNCTIONS
// ============================================

void nvs::initPowerNamespace()
{
    // Initialize power namespace with default values if it doesn't exist
    // This ensures the namespace exists before any reads/writes
    
    if (!esp_prefs_nvs.begin("power", false)) {
        // Failed to create/open namespace - this is a critical error
        Serial.println("[NVS] ERROR: Failed to initialize power namespace!");
        return;
    }
    
    // Check and initialize each key with defaults if not present
    if (!esp_prefs_nvs.isKey("enabled")) {
        esp_prefs_nvs.putBool("enabled", DEFAULT_POWER_ENABLED);
        Serial.printf("[NVS] Initialized power.enabled = %s\n", DEFAULT_POWER_ENABLED ? "true" : "false");
    }
    
    if (!esp_prefs_nvs.isKey("techniques")) {
        esp_prefs_nvs.putUChar("techniques", DEFAULT_POWER_TECHNIQUES);
        Serial.printf("[NVS] Initialized power.techniques = 0x%02X\n", DEFAULT_POWER_TECHNIQUES);
    }
    
    if (!esp_prefs_nvs.isKey("energy_poll")) {
        esp_prefs_nvs.putULong64("energy_poll", DEFAULT_ENERGY_POLL_FREQUENCY_US);
        Serial.printf("[NVS] Initialized power.energy_poll = %llu us (%.1f s)\n", 
                     DEFAULT_ENERGY_POLL_FREQUENCY_US, 
                     DEFAULT_ENERGY_POLL_FREQUENCY_US / 1000000.0);
    }
    
    esp_prefs_nvs.end();
    Serial.println("[NVS] Power namespace initialized successfully");
}

bool nvs::getPowerEnabled()
{
    // Open in read-only mode
    if (!esp_prefs_nvs.begin("power", true)) {
        return DEFAULT_POWER_ENABLED;  // Default: false (disabled)
    }

    if (!esp_prefs_nvs.isKey("enabled")) {
        // Write default value for future calls
        esp_prefs_nvs.end();
        if (!esp_prefs_nvs.begin("power", false)) {
            return DEFAULT_POWER_ENABLED;
        }
        esp_prefs_nvs.putBool("enabled", DEFAULT_POWER_ENABLED);
        esp_prefs_nvs.end();
        return DEFAULT_POWER_ENABLED;
    }

    bool enabled = esp_prefs_nvs.getBool("enabled", DEFAULT_POWER_ENABLED);
    esp_prefs_nvs.end();
    return enabled;
}


bool nvs::setPowerEnabled(bool enabled)
{
    if (!esp_prefs_nvs.begin("power", false)) // Open NVS in read-write mode
    {
        return false; // Failed to open NVS
    }

    esp_prefs_nvs.putBool("enabled", enabled);
    esp_prefs_nvs.end();
    return true;
}


uint8_t nvs::getPowerTechniques()
{
    // Open in read-only mode
    if (!esp_prefs_nvs.begin("power", true)) {
        return DEFAULT_POWER_TECHNIQUES;  // Default: 0x01 (WiFi modem sleep only)
    }

    if (!esp_prefs_nvs.isKey("techniques")) {
        // Write default value for future calls
        esp_prefs_nvs.end();
        if (!esp_prefs_nvs.begin("power", false)) {
            return DEFAULT_POWER_TECHNIQUES;
        }
        esp_prefs_nvs.putUChar("techniques", DEFAULT_POWER_TECHNIQUES);
        esp_prefs_nvs.end();
        return DEFAULT_POWER_TECHNIQUES;
    }

    uint8_t techniques = esp_prefs_nvs.getUChar("techniques", DEFAULT_POWER_TECHNIQUES);
    esp_prefs_nvs.end();
    
    // Validate techniques are within valid range (0x00-0x0F for 4 techniques)
    if (techniques > 0x0F) {
        return DEFAULT_POWER_TECHNIQUES;
    }
    
    return techniques;
}


bool nvs::setPowerTechniques(uint8_t techniques)
{
    // Validate input (max 4 bits for 4 techniques)
    if (techniques > 0x0F) {
        return false; // Invalid technique combination
    }

    if (!esp_prefs_nvs.begin("power", false)) // Open NVS in read-write mode
    {
        return false; // Failed to open NVS
    }

    esp_prefs_nvs.putUChar("techniques", techniques);
    esp_prefs_nvs.end();
    return true;
}


uint64_t nvs::getEnergyPollFreq()
{
    // Open in read-only mode
    if (!esp_prefs_nvs.begin("power", true)) {
        return DEFAULT_ENERGY_POLL_FREQUENCY_US;  // Default: 5 minutes
    }

    if (!esp_prefs_nvs.isKey("energy_poll")) {
        // Write default value for future calls
        esp_prefs_nvs.end();
        if (!esp_prefs_nvs.begin("power", false)) {
            return DEFAULT_ENERGY_POLL_FREQUENCY_US;
        }
        esp_prefs_nvs.putULong64("energy_poll", DEFAULT_ENERGY_POLL_FREQUENCY_US);
        esp_prefs_nvs.end();
        return DEFAULT_ENERGY_POLL_FREQUENCY_US;
    }

    uint64_t freq = esp_prefs_nvs.getULong64("energy_poll", DEFAULT_ENERGY_POLL_FREQUENCY_US);
    esp_prefs_nvs.end();
    
    // Minimum 1 minute for energy polling
    if (freq < 60000000) {
        return DEFAULT_ENERGY_POLL_FREQUENCY_US;
    }
    
    return freq;
}


bool nvs::setEnergyPollFreq(uint64_t freq)
{
    // Validate input (minimum 1 minute)
    if (freq < 60000000) {
        return false; // Too short for energy polling
    }

    if (!esp_prefs_nvs.begin("power", false)) // Open NVS in read-write mode
    {
        return false; // Failed to open NVS
    }

    esp_prefs_nvs.putULong64("energy_poll", freq);
    esp_prefs_nvs.end();
    return true;
}
