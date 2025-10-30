#ifndef NVS_H
#define NVS_H

#include <Preferences.h>

#include "peripheral/acquisition.h"

#define MIN_POLL_FREQ 100000 // 100 ms
#define MIN_UPLOAD_FREQ 1000000 // 1 second

    // Power management defaults
    #define DEFAULT_POWER_ENABLED false
    #define DEFAULT_POWER_TECHNIQUES 0x01  // POWER_TECH_WIFI_MODEM_SLEEP by default
    #define DEFAULT_ENERGY_POLL_FREQUENCY_US 300000000  // 5 minutes in microseconds

extern Preferences esp_prefs_nvs;  // NVS storage instance

class nvs{
    public:
        nvs();

        static uint8_t getReadRegCount();
        static const RegID* getReadRegs();
        static uint64_t getPollFreq();
        static uint64_t getUploadFreq();
        static uint64_t getConfigFreq();
        static uint64_t getCommandFreq();
        static uint64_t getOtaFreq();

        static bool saveReadRegs(uint16_t regMask, size_t count);
        static bool changePollFreq(uint64_t poll_time);
        static bool changeUploadFreq(uint64_t upload_time);
        static bool changeConfigFreq(uint64_t config_time);
        static bool changeCommandFreq(uint64_t command_time);
        static bool changeOtaFreq(uint64_t ota_time);
        
        // Power Management Functions
        static void initPowerNamespace();  // Initialize power NVS namespace with defaults
        static bool getPowerEnabled();
        static bool setPowerEnabled(bool enabled);
        static uint8_t getPowerTechniques();
        static bool setPowerTechniques(uint8_t techniques);
        static uint64_t getEnergyPollFreq();
        static bool setEnergyPollFreq(uint64_t freq);
};

#endif