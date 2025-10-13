#ifndef NVS_H
#define NVS_H

#include <Preferences.h>

#include "peripheral/acquisition.h"

#define MIN_POLL_FREQ 100000 // 100 ms
#define MIN_UPLOAD_FREQ 1000000 // 1 second

extern Preferences esp_prefs_nvs;  // NVS storage instance

class nvs{
    public:
        nvs();

        static uint8_t getReadRegCount();
        static const RegID* getReadRegs();
        static uint64_t getPollFreq();
        static uint64_t getUploadFreq();

        static bool saveReadRegs(const RegID* selection, size_t count);
        static bool changePollFreq(uint64_t poll_time);
        static bool changeUploadFreq(uint64_t upload_time);
};

#endif