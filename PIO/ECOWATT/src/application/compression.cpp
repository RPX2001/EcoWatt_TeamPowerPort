#include "application/compression.h"

// ==================== STATIC MEMBER INITIALIZATION ====================

String DataCompression::lastErrorMessage = "";
DataCompression::ErrorType DataCompression::lastErrorType = ERROR_NONE;
bool DataCompression::debugMode = false;
size_t DataCompression::maxMemoryUsage = DEFAULT_MAX_MEMORY;
float DataCompression::compressionPreference = DEFAULT_PREFERENCE;
uint16_t DataCompression::largeDeltaThreshold = DEFAULT_LARGE_DELTA_THRESHOLD;
uint8_t DataCompression::bitPackingThreshold = DEFAULT_BIT_PACKING_THRESHOLD;
float DataCompression::dictionaryLearningRate = DEFAULT_DICTIONARY_LEARNING_RATE;
uint8_t DataCompression::temporalWindowSize = DEFAULT_TEMPORAL_WINDOW_SIZE;

// Working memory buffer variables
uint8_t* DataCompression::workingBuffer = nullptr;
size_t DataCompression::workingBufferSize = 0;
bool DataCompression::workingBufferAllocated = false;

// Performance tracking variables
unsigned long DataCompression::totalCompressions = 0;
unsigned long DataCompression::totalDecompressions = 0;
float DataCompression::cumulativeCompressionRatio = 0.0f;
unsigned long DataCompression::cumulativeCompressionTime = 0;

// Smart Selection variables
DataCompression::SensorPattern DataCompression::sensorDictionary[16] = {};
uint8_t DataCompression::dictionarySize = 0;
uint32_t DataCompression::smartTotalCompressions = 0;
DataCompression::TemporalContext DataCompression::temporalBuffer = {};
DataCompression::MethodPerformance DataCompression::methodStats[4] = {
    {"DICTIONARY", 0, 0.0f, 0, 0.0f, 0.0f},
    {"TEMPORAL", 0, 0.0f, 0, 0.0f, 0.0f},
    {"SEMANTIC", 0, 0.0f, 0, 0.0f, 0.0f},
    {"BITPACK", 0, 0.0f, 0, 0.0f, 0.0f}
};

// Method identifiers
const char* DataCompression::METHOD_BINARY_PACKED = "BINPACK";
const char* DataCompression::METHOD_BINARY_DELTA = "BINDELTA";
const char* DataCompression::METHOD_BINARY_RLE = "BINRLE";
const char* DataCompression::METHOD_BINARY_HYBRID = "BINHYBRID";
const char* DataCompression::METHOD_RAW_BINARY = "RAWBIN";

// ==================== ADAPTIVE SMART SELECTION METHODS ====================

/**
 * @fn std::vector<uint8_t> DataCompression::compressWithSmartSelection(uint16_t* data, const RegID* selection, size_t count)
 * 
 * @brief Compress data using the adaptive smart selection system.
 * 
 * @param data Pointer to the array of uint16_t sensor data.
 * @param selection Pointer to the array of RegID indicating which registers are included.
 * @param count Number of data points (length of data and selection arrays).
 * 
 * @return std::vector<uint8_t> Compressed data as a byte vector.
 */
std::vector<uint8_t> DataCompression::compressWithSmartSelection(uint16_t* data, const RegID* selection, size_t count) 
{
    if (count == 0 || data == nullptr || selection == nullptr) 
    {
        setError("Invalid input for smart selection", ERROR_INVALID_INPUT);
        return std::vector<uint8_t>();
    }
    
    unsigned long startTime = micros();
    
    // Initialize dictionary if needed
    if (dictionarySize == 0) 
    {
        initializeSensorDictionary();
    }
    
    // Test all compression methods
    auto dictionaryResult = testCompressionMethod("DICTIONARY", data, selection, count);
    auto temporalResult = testCompressionMethod("TEMPORAL", data, selection, count);
    auto semanticResult = testCompressionMethod("SEMANTIC", data, selection, count);
    auto bitpackResult = testCompressionMethod("BITPACK", data, selection, count);
    
    // Find the best result (lowest compression ratio = best compression)
    std::vector<CompressionResult> results = {dictionaryResult, temporalResult, semanticResult, bitpackResult};
    
    CompressionResult bestResult = results[0];
    for (const auto& result : results) 
    {
        if (result.academicRatio < bestResult.academicRatio && result.academicRatio > 0) 
        {
            bestResult = result;
        }
    }
    
    // Update adaptive learning
    updateMethodPerformance(bestResult.method, bestResult.academicRatio, bestResult.timeUs);
    
    unsigned long totalTime = micros() - startTime;
    
    // Print original vs compressed data clearly
    size_t originalBytes = count * sizeof(uint16_t);
    size_t compressedBytes = bestResult.data.size();
    float savingsPercent = (1.0f - bestResult.academicRatio) * 100.0f;
    
    print("COMPRESSION RESULT: %s method\n", bestResult.method.c_str());
    print("Original: %zu bytes -> Compressed: %zu bytes (%.1f%% savings)\n", 
          originalBytes, compressedBytes, savingsPercent);
    print("Academic Ratio: %.3f | Time: %lu μs\n", bestResult.academicRatio, totalTime);

    // Update dictionary with new data for future improvements
    updateDictionary(data, selection, count);
    
    return bestResult.data;
}


/**
 * @fn DataCompression::CompressionResult DataCompression::testCompressionMethod(const String& method, uint16_t* data, const RegID* selection, size_t count)
 * 
 * @brief Test an individual compression method and return detailed results.
 * 
 * @param method Compression method to test ("DICTIONARY", "TEMPORAL", "SEMANTIC", "BITPACK").
 * @param data Pointer to the array of uint16_t sensor data.
 * @param selection Pointer to the array of RegID indicating which registers are included.
 * @param count Number of data points (length of data and selection arrays).
 * 
 * @return CompressionResult Struct containing compressed data and performance metrics.
 */
DataCompression::CompressionResult DataCompression::testCompressionMethod(const String& method, uint16_t* data, const RegID* selection, size_t count) 
{
    CompressionResult result;
    unsigned long startTime = micros();
    
    if (method == "DICTIONARY") 
    {
        result.data = compressWithDictionary(data, selection, count);
    } 
    else if (method == "TEMPORAL") 
    {
        result.data = compressWithTemporalDelta(data, selection, count);
    } 
    else if (method == "SEMANTIC") 
    {
        result.data = compressWithSemanticRLE(data, selection, count);
    } 
    else if (method == "BITPACK") 
    {
        result.data = compressBinary(data, count);
    }
    
    result.timeUs = micros() - startTime;
    result.method = method;
    
    // Calculate academic compression ratio (compressed/original)
    size_t originalBits = count * 16;  // 16 bits per uint16_t
    size_t compressedBits = result.data.size() * 8;  // 8 bits per byte
    
    result.academicRatio = result.data.empty() ? 1.0f : (float)compressedBits / (float)originalBits;
    result.traditionalRatio = result.data.empty() ? 0.0f : (float)originalBits / (float)compressedBits;
    result.efficiency = result.academicRatio > 0 ? (1.0f / result.academicRatio) / (result.timeUs / 1000.0f) : 0.0f;
    
    return result;
}

// ==================== DICTIONARY-BASED BITMASK COMPRESSION ====================

/**
 * @fn std::vector<uint8_t> DataCompression::compressWithDictionary(uint16_t* data, const RegID* selection, size_t count)
 * 
 * @brief Compress data using dictionary-based bitmask compression.
 * 
 * @param data Pointer to the array of uint16_t sensor data.
 * @param selection Pointer to the array of RegID indicating which registers are included.
 * @param count Number of data points (length of data and selection arrays).
 * 
 * @return std::vector<uint8_t> Compressed data as a byte vector.
 */
std::vector<uint8_t> DataCompression::compressWithDictionary(uint16_t* data, const RegID* selection, size_t count) 
{
    std::vector<uint8_t> result;
    
    // Find closest dictionary pattern
    int bestMatch = findClosestDictionaryPattern(data, selection, count);
    
    if (bestMatch >= 0) 
    {
        // Bitmask compression with dictionary
        result.push_back(0xD0);  // Dictionary compression marker
        result.push_back(bestMatch);  // Dictionary index
        result.push_back(count);  // Number of values
        
        // Create bitmask for differences and collect deltas
        uint16_t differencesMask = 0;
        std::vector<int16_t> deltas;
        uint8_t deltaBits = 0;
        
        for (size_t i = 0; i < count; i++) 
        {
            int16_t delta = (int16_t)data[i] - (int16_t)sensorDictionary[bestMatch].values[selection[i]];
            if (delta != 0) 
            {
                differencesMask |= (1 << i);
                deltas.push_back(delta);
            }
        }
        
        // Store bitmask (up to 16 bits for max 16 registers)
        result.push_back(differencesMask & 0xFF);
        result.push_back((differencesMask >> 8) & 0xFF);
        
        // Encode deltas using variable-length encoding
        for (int16_t delta : deltas) 
        {
            if (delta >= -127 && delta <= 127) 
            {
                // 8-bit signed delta
                result.push_back(0x80 | ((uint8_t)delta & 0x7F));
                if (delta < 0) result[result.size()-1] |= 0x40;  // Sign bit
            } 
            else 
            {
                // 16-bit delta with escape marker
                result.push_back(0x00);  // 16-bit marker
                result.push_back(delta & 0xFF);
                result.push_back((delta >> 8) & 0xFF);
            }
        }
    } 
    else 
    {
        // No good dictionary match, fall back to bit-packing
        return compressBinary(data, count);
    }
    
    return result;
}

// ==================== TEMPORAL DELTA COMPRESSION ====================
/**
 * @fn std::vector<uint8_t> DataCompression::compressWithTemporalDelta(uint16_t* data, const RegID* selection, size_t count)
 * 
 * @brief Compress data using temporal delta compression with trend analysis.
 * 
 * @param data Pointer to the array of uint16_t sensor data.
 * @param selection Pointer to the array of RegID indicating which registers are included.
 * @param count Number of data points (length of data and selection arrays).
 * 
 * @return std::vector<uint8_t> Compressed data as a byte vector.
 */
std::vector<uint8_t> DataCompression::compressWithTemporalDelta(uint16_t* data, const RegID* selection, size_t count) 
{
    std::vector<uint8_t> result;
    
    // Check if we have temporal context and matching register layout
    bool hasCompatibleHistory = temporalBuffer.bufferFull && 
                               temporalBuffer.lastRegisterCount == count &&
                               memcmp(temporalBuffer.lastRegisters, selection, count * sizeof(RegID)) == 0;
    
    if (!hasCompatibleHistory) 
    {
        // Store as base sample
        result.push_back(0x70);  // Temporal base marker
        result.push_back(count);
        
        // Store register layout
        for (size_t i = 0; i < count; i++) 
        {
            result.push_back(selection[i]);
        }
        
        // Store values
        for (size_t i = 0; i < count; i++) 
        {
            result.push_back(data[i] & 0xFF);
            result.push_back((data[i] >> 8) & 0xFF);
        }
    } 
    else 
    {
        // Use temporal prediction with trend analysis
        result.push_back(0x71);  // Temporal delta marker
        result.push_back(count);
        
        uint8_t prevIndex = (temporalBuffer.writeIndex - 1 + 8) % 8;
        uint8_t prev2Index = (temporalBuffer.writeIndex - 2 + 8) % 8;
        
        for (size_t i = 0; i < count; i++) 
        {
            // Linear prediction: predict[i] = 2*prev[i] - prev2[i]
            int32_t prev1 = temporalBuffer.recentSamples[prevIndex][selection[i]];
            int32_t prev2 = temporalBuffer.recentSamples[prev2Index][selection[i]];
            int32_t predicted = 2 * prev1 - prev2;
            
            // Clamp prediction to reasonable range
            if (predicted < 0) predicted = prev1;
            if (predicted > 65535) predicted = prev1;
            
            int16_t delta = (int16_t)data[i] - (int16_t)predicted;
            
            // Variable-length delta encoding
            if (delta >= -63 && delta <= 63) 
            {
                // 7-bit delta with sign
                uint8_t encoded = (abs(delta) & 0x3F) | 0x80;
                if (delta < 0) encoded |= 0x40;
                result.push_back(encoded);
            } 
            else if (delta >= -127 && delta <= 127) 
            {
                // 8-bit delta  
                result.push_back(0x00);  // 8-bit marker
                result.push_back((uint8_t)delta);
            } 
            else 
            {
                // 16-bit delta
                result.push_back(0x01);  // 16-bit marker
                result.push_back(delta & 0xFF);
                result.push_back((delta >> 8) & 0xFF);
            }
        }
    }
    
    // Update temporal buffer
    for (size_t i = 0; i < count; i++) 
    {
        temporalBuffer.recentSamples[temporalBuffer.writeIndex][selection[i]] = data[i];
    }
    memcpy(temporalBuffer.lastRegisters, selection, count * sizeof(RegID));
    temporalBuffer.lastRegisterCount = count;
    temporalBuffer.writeIndex = (temporalBuffer.writeIndex + 1) % 8;
    if (temporalBuffer.writeIndex == 0) temporalBuffer.bufferFull = true;
    
    return result;
}

// ==================== SEMANTIC RLE COMPRESSION ====================
/**
 * @fn std::vector<uint8_t> DataCompression::compressWithSemanticRLE(uint16_t* data, const RegID* selection, size_t count)
 * 
 * @brief Compress data using semantic run-length encoding (RLE) with type-aware encoding.
 * 
 * @param data Pointer to the array of uint16_t sensor data.
 * @param selection Pointer to the array of RegID indicating which registers are included.
 * @param count Number of data points (length of data and selection arrays).
 * 
 * @return std::vector<uint8_t> Compressed data as a byte vector.
 */
std::vector<uint8_t> DataCompression::compressWithSemanticRLE(uint16_t* data, const RegID* selection, size_t count) 
{
    std::vector<uint8_t> result;
    result.push_back(0x50);  // Semantic RLE marker
    result.push_back(count);
    
    // Group registers by semantic type
    struct RegisterGroup 
    {
        std::vector<uint16_t> values;
        std::vector<uint8_t> positions;
        String type;
        uint8_t typeId;
    };
    
    std::vector<RegisterGroup> groups;
    
    // Classify and group registers
    for (size_t i = 0; i < count; i++) 
    {
        char regType[16];
        getRegisterType(selection[i], regType, sizeof(regType));
        uint8_t typeId = getRegisterTypeId(selection[i]);
        
        // Find existing group or create new one
        bool found = false;
        for (auto& group : groups) 
        {
            if (group.typeId == typeId) 
            {
                group.values.push_back(data[i]);
                group.positions.push_back(i);
                found = true;
                break;
            }
        }
        
        if (!found) 
        {
            RegisterGroup newGroup;
            newGroup.type = regType;
            newGroup.typeId = typeId;
            newGroup.values.push_back(data[i]);
            newGroup.positions.push_back(i);
            groups.push_back(newGroup);
        }
    }
    
    result.push_back(groups.size());  // Number of semantic groups
    
    // Compress each group with type-specific RLE
    for (const auto& group : groups) 
    {
        result.push_back(group.typeId);  // Type identifier
        result.push_back(group.values.size());  // Group size
        
        // Store positions
        for (uint8_t pos : group.positions) 
        {
            result.push_back(pos);
        }
        
        // Apply RLE with type-aware encoding
        size_t i = 0;
        while (i < group.values.size()) 
        {
            uint16_t currentValue = group.values[i];
            uint8_t runLength = 1;
            
            // Count consecutive similar values (with tolerance for same type)
            uint16_t tolerance = getTypeTolerances(group.typeId);
            
            while (i + runLength < group.values.size() && runLength < 255) 
            {
                if (abs((int32_t)group.values[i + runLength] - (int32_t)currentValue) <= tolerance) 
                {
                    runLength++;
                } 
                else 
                {
                    break;
                }
            }
            
            // Store run with type-specific bit packing
            uint8_t bitsNeeded = getBitsForType(group.typeId);
            if (bitsNeeded <= 8) 
            {
                result.push_back(currentValue & 0xFF);
                result.push_back(runLength);
            } else {
                result.push_back(currentValue & 0xFF);
                result.push_back((currentValue >> 8) & 0xFF);
                result.push_back(runLength);
            }
            
            i += runLength;
        }
    }
    
    return result;
}

// ==================== HELPER FUNCTIONS ====================
/**
 * @fn DataCompression::getRegisterType(RegID regId, char* result, size_t resultSize)
 * 
 * @brief Get the semantic type string for a given register ID.
 * 
 * @param regId Register ID to classify.
 * @param result Buffer to store the resulting type string.
 * @param resultSize Size of the result buffer.
 */
void DataCompression::getRegisterType(RegID regId, char* result, size_t resultSize) 
{
    const char* typeStr;
    switch (regId) 
    {
        case REG_VAC1: typeStr = "voltage"; break;
        case REG_IAC1: typeStr = "current"; break;
        case REG_FAC1: typeStr = "frequency"; break;
        case REG_VPV1:
        case REG_VPV2: typeStr = "pv_voltage"; break;
        case REG_IPV1:
        case REG_IPV2: typeStr = "pv_current"; break;
        case REG_TEMP: typeStr = "temperature"; break;
        case REG_POW:
        case REG_PAC: typeStr = "power"; break;
        default: typeStr = "unknown"; break;
    }
    strncpy(result, typeStr, resultSize - 1);
    result[resultSize - 1] = '\0';
}


/**
 * @fn uint8_t DataCompression::getRegisterTypeId(RegID regId)
 * 
 * @brief Get a numeric type identifier for a given register ID.
 * 
 * @param regId Register ID to classify.
 * 
 * @return uint8_t Numeric type identifier (1-7), or 0 for unknown.
 */
uint8_t DataCompression::getRegisterTypeId(RegID regId) 
{
    switch (regId) 
    {
        case REG_VAC1: return 1;  // AC voltage
        case REG_IAC1: return 2;  // AC current
        case REG_FAC1: return 3;  // Frequency
        case REG_VPV1:
        case REG_VPV2: return 4;  // PV voltage
        case REG_IPV1:
        case REG_IPV2: return 5;  // PV current
        case REG_TEMP: return 6;  // Temperature
        case REG_POW:
        case REG_PAC: return 7;   // Power
        default: return 0;        // Unknown
    }
}

/**
 * @fn uint16_t DataCompression::getTypeTolerances(uint8_t typeId)
 * 
 * @brief Get the tolerance value for a given semantic type ID.
 * 
 * @param typeId Numeric type identifier (1-7).
 * 
 * @return uint16_t Tolerance value for the type.
 */
uint16_t DataCompression::getTypeTolerances(uint8_t typeId) 
{
    switch (typeId) 
    {
        case 1: return 10;  // AC voltage ±10V
        case 2: return 5;   // AC current ±5A
        case 3: return 1;   // Frequency ±1Hz
        case 4: return 15;  // PV voltage ±15V
        case 5: return 3;   // PV current ±3A
        case 6: return 5;   // Temperature ±5°C
        case 7: return 50;  // Power ±50W
        default: return 0;
    }
}

/**
 * @fn uint8_t DataCompression::getBitsForType(uint8_t typeId)
 * 
 * @brief Get the number of bits needed to represent a given semantic type ID.
 * 
 * @param typeId Numeric type identifier (1-7).
 * 
 * @return uint8_t Number of bits needed for the type.
 */
uint8_t DataCompression::getBitsForType(uint8_t typeId) 
{
    switch (typeId) 
    {
        case 1: return 12;  // AC voltage (0-4095V)
        case 2: return 8;   // AC current (0-255A)
        case 3: return 6;   // Frequency (0-63Hz)
        case 4: return 9;   // PV voltage (0-511V)
        case 5: return 7;   // PV current (0-127A)
        case 6: return 10;  // Temperature (0-1023°C)
        case 7: return 13;  // Power (0-8191W)
        default: return 16;
    }
}


/**
 * @fn void DataCompression::initializeSensorDictionary()
 * 
 * @brief Initialize the sensor dictionary with common patterns.
 */
void DataCompression::initializeSensorDictionary() 
{
    // Pattern 0: Typical daytime operation
    uint16_t pattern0[] = {2400, 170, 50, 400, 380, 70, 65, 550, 4000, 4200};
    memcpy(sensorDictionary[0].values, pattern0, sizeof(pattern0));
    sensorDictionary[0].frequency = 1;
    
    // Pattern 1: Low power operation
    uint16_t pattern1[] = {2380, 100, 50, 200, 180, 30, 25, 520, 2000, 2500};
    memcpy(sensorDictionary[1].values, pattern1, sizeof(pattern1));
    sensorDictionary[1].frequency = 1;
    
    // Pattern 2: High power operation  
    uint16_t pattern2[] = {2450, 200, 50, 450, 420, 90, 85, 580, 5000, 5200};
    memcpy(sensorDictionary[2].values, pattern2, sizeof(pattern2));
    sensorDictionary[2].frequency = 1;
    
    // Pattern 3: Your typical current readings (learned from data)
    uint16_t pattern3[] = {2430, 165, 50, 350, 350, 70, 65, 545, 4100, 4150};
    memcpy(sensorDictionary[3].values, pattern3, sizeof(pattern3));
    sensorDictionary[3].frequency = 1;
    
    dictionarySize = 4;
}


/**
 * @fn int DataCompression::findClosestDictionaryPattern(uint16_t* data, const RegID* selection, size_t count)
 * 
 * @brief Find the closest matching pattern in the dictionary for the given data.
 * 
 * @param data Pointer to the array of uint16_t sensor data.
 * @param selection Pointer to the array of RegID indicating which registers are included.
 * @param count Number of data points (length of data and selection arrays).
 * 
 * @return int Index of the closest matching pattern, or -1 if no good match found.
 */
int DataCompression::findClosestDictionaryPattern(uint16_t* data, const RegID* selection, size_t count) 
{
    if (dictionarySize == 0) return -1;
    
    int bestMatch = -1;
    uint32_t minDistance = UINT32_MAX;
    
    for (uint8_t i = 0; i < dictionarySize; i++) 
    {
        uint32_t distance = 0;
        
        for (size_t j = 0; j < count; j++) 
        {
            int32_t diff = (int32_t)data[j] - (int32_t)sensorDictionary[i].values[selection[j]];
            distance += abs(diff);
        }
        
        if (distance < minDistance) 
        {
            minDistance = distance;
            bestMatch = i;
        }
    }
    
    // Use dictionary if average error per value is reasonable
    uint32_t avgError = minDistance / count;
    if (avgError < 200) 
    {  // Threshold for acceptable match
        return bestMatch;
    }
    
    return -1;  // No good match
}

/**
 * @fn void DataCompression::updateDictionary(uint16_t* data, const RegID* selection, size_t count)
 * 
 * @brief Update the sensor dictionary with a new pattern if it's unique enough.
 * 
 * @param data Pointer to the array of uint16_t sensor data.
 * @param selection Pointer to the array of RegID indicating which registers are included.
 * @param count Number of data points (length of data and selection arrays).
 */
void DataCompression::updateDictionary(uint16_t* data, const RegID* selection, size_t count) 
{
    // Simple dictionary learning: if we have space and this pattern is unique enough
    if (dictionarySize < 15) 
    {  // Leave room for one more pattern
        int closestMatch = findClosestDictionaryPattern(data, selection, count);
        if (closestMatch < 0) 
        {  // No close match found
            // Add this as a new pattern
            for (size_t i = 0; i < count; i++) 
            {
                sensorDictionary[dictionarySize].values[selection[i]] = data[i];
            }
            sensorDictionary[dictionarySize].frequency = 1;
            dictionarySize++;
        } 
        else 
        {
            // Update frequency of existing pattern
            sensorDictionary[closestMatch].frequency++;
        }
    }
}


/**
 * @fn void DataCompression::updateMethodPerformance(const String& method, float academicRatio, unsigned long timeUs)
 * 
 * @brief Update performance statistics for a given compression method.
 * 
 * @param method Compression method name ("DICTIONARY", "TEMPORAL", "SEMANTIC", "BITPACK").
 * @param academicRatio Academic compression ratio achieved (compressed/original).
 * @param timeUs Time taken for compression in microseconds.
 */
void DataCompression::updateMethodPerformance(const String& method, float academicRatio, unsigned long timeUs) {
    for (auto& stat : methodStats) {
        if (stat.methodName == method) {
            stat.useCount++;
            stat.avgCompressionRatio = (stat.avgCompressionRatio * (stat.useCount - 1) + academicRatio) / stat.useCount;
            stat.avgTimeUs = (stat.avgTimeUs * (stat.useCount - 1) + timeUs) / stat.useCount;
            
            // Success rate: compression ratio < 0.8 is considered successful
            if (academicRatio < 0.8f) {
                stat.successRate = (stat.successRate * (stat.useCount - 1) + 1.0f) / stat.useCount;
            } else {
                stat.successRate = (stat.successRate * (stat.useCount - 1) + 0.0f) / stat.useCount;
            }
            
            // Calculate adaptive score (lower is better for academic ratio)
            stat.adaptiveScore = stat.successRate / (stat.avgCompressionRatio + 0.1f);
            break;
        }
    }
}

// ==================== EXISTING BINARY COMPRESSION METHODS ====================
/**
 * @fn std::vector<uint8_t> DataCompression::compressBinary(uint16_t* data, size_t count)
 * 
 * @brief Compress binary data using adaptive selection of methods.
 * 
 * @param data Pointer to the array of uint16_t sensor data.
 * @param count Number of data points (length of data array).
 * 
 * @return std::vector<uint8_t> Compressed data as a byte vector.
 */
std::vector<uint8_t> DataCompression::compressBinary(uint16_t* data, size_t count) 
{
    if (count == 0 || data == nullptr) 
    {
        setError("Invalid input data");
        return std::vector<uint8_t>();
    }

    DataCharacteristics characteristics = analyzeData(data, count);
    
    std::vector<uint8_t> bestResult;
    String bestMethod = "RAW_BINARY";
    size_t originalSize = count * 2;
    
    // Try bit-packing if it can save significant space
    if (characteristics.optimalBits < 16 && characteristics.optimalBits >= 8) 
    {
        std::vector<uint8_t> bitPacked = compressBinaryBitPacked(data, count, characteristics.optimalBits);
        
        if (bitPacked.size() < originalSize) 
        {
            bestResult = bitPacked;
            bestMethod = "BIT_PACKED";
        }
    }
    
    // If no compression helped, use raw binary
    if (bestResult.empty() || bestResult.size() >= originalSize) 
    {
        return storeAsRawBinary(data, count);
    }
    
    return bestResult;
}


/**
 * @fn std::vector<uint8_t> DataCompression::compressBinaryBitPacked(uint16_t* data, size_t count, uint8_t bitsPerValue)
 * 
 * @brief Compress binary data using bit-packing.
 * 
 * @param data Pointer to the array of uint16_t sensor data.
 * @param count Number of data points (length of data array).
 * @param bitsPerValue Number of bits needed to represent each value.
 * 
 * @return std::vector<uint8_t> Compressed data as a byte vector.
 */
std::vector<uint8_t> DataCompression::compressBinaryBitPacked(uint16_t* data, size_t count, uint8_t bitsPerValue) 
{
    std::vector<uint8_t> result;
    
    if (bitsPerValue == 0 || bitsPerValue > 16) 
    {
        setError("Invalid bits per value");
        return result;
    }
    
    size_t totalBits = count * bitsPerValue;
    size_t packedBytes = (totalBits + 7) / 8;
    size_t originalBytes = count * 2;
    
    // For small datasets, skip header if it negates compression benefit
    bool useHeader = (count > 8) || (packedBytes + 3 < originalBytes);
    
    if (useHeader) 
    {
        result.push_back(0x01);  // Binary bit-packed method ID
        result.push_back(bitsPerValue);
        result.push_back(count);
    }
    
    std::vector<uint8_t> packedData(packedBytes, 0);
    
    size_t bitOffset = 0;
    for (size_t i = 0; i < count; i++) 
    {
        packBitsIntoBuffer(data[i], packedData.data(), bitOffset, bitsPerValue);
        bitOffset += bitsPerValue;
    }
    
    result.insert(result.end(), packedData.begin(), packedData.end());
    return result;
}


/**
 * @fn std::vector<uint8_t> DataCompression::storeAsRawBinary(uint16_t* data, size_t count)
 * 
 * @brief Store data as raw binary with minimal overhead.
 * 
 * @param data Pointer to the array of uint16_t sensor data.
 * @param count Number of data points (length of data array).
 * 
 * @return std::vector<uint8_t> Raw binary data as a byte vector.
 */
std::vector<uint8_t> DataCompression::storeAsRawBinary(uint16_t* data, size_t count) 
{
    std::vector<uint8_t> result;
    
    // For small datasets (≤8 values), skip header overhead
    if (count <= 8) 
    {
        result.reserve(count * 2);
        for (size_t i = 0; i < count; i++) 
        {
            result.push_back(data[i] & 0xFF);
            result.push_back((data[i] >> 8) & 0xFF);
        }
        return result;
    }
    
    // For larger datasets, use header
    result.push_back(0x00);  // METHOD_ID
    result.push_back(count); // COUNT
    for (size_t i = 0; i < count; i++) 
    {
        result.push_back(data[i] & 0xFF);
        result.push_back((data[i] >> 8) & 0xFF);
    }
    return result;
}

// ==================== UTILITY FUNCTIONS ====================
/**
 * @fn void DataCompression::packBitsIntoBuffer(uint16_t value, uint8_t* buffer, size_t bitOffset, uint8_t numBits)
 * 
 * @brief Pack a value into a byte buffer at a specific bit offset.
 * 
 * @param value The value to pack (should fit within numBits).
 * @param buffer Pointer to the byte buffer.
 * @param bitOffset Bit offset in the buffer to start packing.
 * @param numBits Number of bits to use for the value.
 */
void DataCompression::packBitsIntoBuffer(uint16_t value, uint8_t* buffer, size_t bitOffset, uint8_t numBits) 
{
    uint16_t mask = (1 << numBits) - 1;
    value &= mask;
    
    size_t byteOffset = bitOffset / 8;
    uint8_t bitPos = bitOffset % 8;
    
    if (bitPos + numBits <= 8) 
    {
        buffer[byteOffset] |= (value << (8 - bitPos - numBits));
    } 
    else 
    {
        uint8_t firstBits = 8 - bitPos;
        uint8_t remainingBits = numBits - firstBits;
        
        buffer[byteOffset] |= (value >> remainingBits);
        buffer[byteOffset + 1] |= ((value & ((1 << remainingBits) - 1)) << (8 - remainingBits));
    }
}

/**
 * @fn DataCompression::DataCharacteristics DataCompression::analyzeData(uint16_t* data, size_t count)
 * 
 * @brief Analyze data characteristics to inform compression strategy.
 * 
 * @param data Pointer to the array of uint16_t sensor data.
 * @param count Number of data points (length of data array).
 * 
 * @return DataCharacteristics Struct containing analysis results.
 */
DataCompression::DataCharacteristics DataCompression::analyzeData(uint16_t* data, size_t count) 
{
    DataCharacteristics characteristics = {0};
    
    if (count == 0 || data == nullptr) return characteristics;
    
    uint16_t minVal = data[0], maxVal = data[0];
    uint32_t sum = 0;
    size_t repeatedPairs = 0;
    int32_t totalDeltaMagnitude = 0;
    size_t largeDeltas = 0;
    
    for (size_t i = 0; i < count; i++) 
    {
        if (data[i] < minVal) minVal = data[i];
        if (data[i] > maxVal) maxVal = data[i];
        sum += data[i];
        
        if (i > 0) 
        {
            if (data[i] == data[i-1]) repeatedPairs++;
            
            int32_t delta = abs((int32_t)data[i] - (int32_t)data[i-1]);
            totalDeltaMagnitude += delta;
            
            if (delta > largeDeltaThreshold) largeDeltas++;
        }
    }
    
    characteristics.minValue = minVal;
    characteristics.maxValue = maxVal;
    characteristics.valueRange = maxVal - minVal;
    characteristics.repeatRatio = (count > 1) ? (float)repeatedPairs / (count - 1) : 0.0f;
    characteristics.avgDeltaMagnitude = (count > 1) ? (float)totalDeltaMagnitude / (count - 1) : 0.0f;
    characteristics.largeDeltaRatio = (count > 1) ? (float)largeDeltas / (count - 1) : 0.0f;
    
    // Calculate optimal bits needed
    characteristics.optimalBits = 1;
    while ((1 << characteristics.optimalBits) <= maxVal) 
    {
        characteristics.optimalBits++;
    }
    
    characteristics.suitableForBitPack = (characteristics.optimalBits < 16);
    characteristics.suitableForDelta = (characteristics.avgDeltaMagnitude < 200);
    characteristics.suitableForRLE = (characteristics.repeatRatio > 0.3f);
    
    return characteristics;
}

// ==================== STATISTICS AND REPORTING ====================
/**
 * @fn void DataCompression::printCompressionStats(const char* method, size_t originalSize, size_t compressedSize)
 * 
 * @brief Print detailed compression statistics.
 * 
 * @param method Compression method name.
 * @param originalSize Size of the original uncompressed data in bytes.
 * @param compressedSize Size of the compressed data in bytes.
 */
void DataCompression::printCompressionStats(const char* method, size_t originalSize, size_t compressedSize) 
{
    if (originalSize == 0) 
    {
        print("Error: Original size is zero\n");
        return;
    }
    
    // Calculate both academic and traditional ratios
    float academicRatio = (float)compressedSize / (float)originalSize;
    float traditionalRatio = (float)originalSize / (float)compressedSize;
    float savings = (1.0f - academicRatio) * 100.0f;

    print("COMPRESSION STATISTICS (Academic Format)\n");
    print("Method: %s\n", method);
    print("Original: %zu bytes -> Compressed: %zu bytes\n", originalSize, compressedSize);
    print("Academic Compression Ratio: %.3f (%.1f%% of original)\n", academicRatio, academicRatio * 100);
    print("Traditional Ratio: %.2f:1\n", traditionalRatio);
    print("Storage Savings: %.1f%%\n", savings);
    
    const char* efficiency = (academicRatio < EXCELLENT_RATIO_THRESHOLD) ? "Excellent" :
                           (academicRatio < GOOD_RATIO_THRESHOLD) ? "Good" :
                           (academicRatio < POOR_RATIO_THRESHOLD) ? "Fair" : "Poor";
    print("Efficiency Rating: %s\n", efficiency);
    print("================================\n");
}


/**
 * @fn void DataCompression::printMemoryUsage()
 * 
 * @brief Print current memory usage statistics of the ESP32.
 */
void DataCompression::printMemoryUsage() 
{
    print("ESP32 MEMORY STATUS\n");
    print("Free Heap: %u bytes\n", ESP.getFreeHeap());
    print("Heap Size: %u bytes\n", ESP.getHeapSize());
    print("Max Alloc: %u bytes\n", ESP.getMaxAllocHeap());
    print("PSRAM Free: %u bytes\n", ESP.getFreePsram());
    print("Flash Size: %u bytes\n", ESP.getFlashChipSize());
    print("==========================\n");
}

// ==================== ERROR HANDLING ====================
/**
 * @fn void DataCompression::setError(const String& errorMsg, ErrorType errorType)
 * 
 * @brief Set the last error message and type.
 * 
 * @param errorMsg Error message string.
 * @param errorType Type of error (ERROR_NONE, ERROR_WARNING, ERROR_CRITICAL).
 */
void DataCompression::setError(const String& errorMsg, ErrorType errorType) 
{
    lastErrorMessage = errorMsg;
    lastErrorType = errorType;
    if (debugMode) 
    {
        print("DataCompression Error: %s\n", errorMsg.c_str());
    }
}


/**
 * @fn void DataCompression::getLastError(char* result, size_t resultSize)
 * 
 * @brief Retrieve the last error message.
 * 
 * @param result Buffer to store the error message.
 * @param resultSize Size of the result buffer.
 */
void DataCompression::getLastError(char* result, size_t resultSize) 
{
    strncpy(result, lastErrorMessage.c_str(), resultSize - 1);
    result[resultSize - 1] = '\0';
}


/**
 * @fn void DataCompression::clearError()
 * 
 *  @brief Clear the last error message and type.
 */
void DataCompression::clearError() 
{
    lastErrorMessage = "";
    lastErrorType = ERROR_NONE;
}


/**
 * @fn bool DataCompression::hasError()
 * 
 * @brief Check if there is a current error.
 */
bool DataCompression::hasError() 
{
    return lastErrorType != ERROR_NONE;
}

// ==================== LEGACY COMPATIBILITY ====================
/**
 * @fn void DataCompression::compressRegisterData(uint16_t* data, size_t count, char* result, size_t resultSize)
 * 
 * @brief Compress register data and encode as base64 with prefix.
 * 
 * @param data Pointer to the array of uint16_t sensor data.
 * @param count Number of data points (length of data array).
 * @param result Buffer to store the resulting string (base64 encoded).
 * @param resultSize Size of the result buffer.
 */
void DataCompression::compressRegisterData(uint16_t* data, size_t count, char* result, size_t resultSize) 
{
    std::vector<uint8_t> binaryCompressed = compressBinary(data, count);
    
    if (binaryCompressed.empty()) 
    {
        strncpy(result, "ERROR:", resultSize - 1);
        result[resultSize - 1] = '\0';
        return;
    }
    
    // Create BINARY: prefix
    size_t prefixLen = strlen("BINARY:");
    if (prefixLen < resultSize) 
    {
        strcpy(result, "BINARY:");
        base64Encode(binaryCompressed, result + prefixLen, resultSize - prefixLen);
    } 
    else 
    {
        result[0] = '\0';
    }
}


/**
 * @fn void DataCompression::base64Encode(const std::vector<uint8_t>& data, char* result, size_t resultSize)
 * 
 * @brief Encode binary data to base64 string.
 * 
 * @param data Vector of bytes to encode.
 * @param result Buffer to store the resulting base64 string.
 * @param resultSize Size of the result buffer.
 */
void DataCompression::base64Encode(const std::vector<uint8_t>& data, char* result, size_t resultSize) 
{
    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t pos = 0;
    
    for (size_t i = 0; i < data.size() && pos < resultSize - 5; i += 3) 
    {
        uint32_t value = data[i] << 16;
        if (i + 1 < data.size()) value |= data[i + 1] << 8;
        if (i + 2 < data.size()) value |= data[i + 2];
        
        result[pos++] = chars[(value >> 18) & 0x3F];
        result[pos++] = chars[(value >> 12) & 0x3F];
        result[pos++] = chars[(value >> 6) & 0x3F];
        result[pos++] = chars[value & 0x3F];
    }
    
    while (pos % 4 && pos < resultSize - 1) result[pos++] = '=';
    result[pos] = '\0';
}

// ==================== CONFIGURATION MANAGEMENT IMPLEMENTATIONS ====================
/**
 * @fn void DataCompression::enableDebug(bool enable)
 * 
 * @brief Enable or disable debug mode.
 * 
 * @param enable True to enable debug mode, false to disable.
 */
void DataCompression::setMaxMemoryUsage(size_t maxBytes) 
{
    maxMemoryUsage = maxBytes;
}


/**
 * @fn void DataCompression::enableDebug(bool enable)
 * 
 * @brief Enable or disable debug mode.
 * 
 * @param enable True to enable debug mode, false to disable.
 */
void DataCompression::setCompressionPreference(float preference) 
{
    compressionPreference = constrain(preference, 0.0f, 1.0f);
}

/**
 * @fn void DataCompression::enableDebug(bool enable)
 * 
 * @brief Enable or disable debug mode.
 * 
 * @param enable True to enable debug mode, false to disable.
 */
void DataCompression::setLargeDeltaThreshold(uint16_t threshold) 
{
    largeDeltaThreshold = threshold;
}


/**
 * @fn void DataCompression::enableDebug(bool enable)
 * 
 * @brief Enable or disable debug mode.
 * 
 * @param enable True to enable debug mode, false to disable.
 */
void DataCompression::setBitPackingThreshold(uint8_t minBitsSaved) 
{
    bitPackingThreshold = minBitsSaved;
}


/**
 * @fn void DataCompression::enableDebug(bool enable)
 * 
 * @brief Enable or disable debug mode.
 * 
 * @param enable True to enable debug mode, false to disable.
 */
void DataCompression::setDictionaryLearningRate(float rate) 
{
    dictionaryLearningRate = constrain(rate, 0.0f, 1.0f);
}

// ==================== MISSING REPORTING FUNCTION ====================
/**
 * @fn void DataCompression::printMethodPerformanceStats()
 * 
 * @brief Print performance statistics for all compression methods.
 */
void DataCompression::printMethodPerformanceStats() 
{
    print("\nMETHOD PERFORMANCE STATISTICS\n");
    print("═══════════════════════════════════════\n");
    
    for (const auto& stat : methodStats) 
    {
        if (stat.useCount > 0) {
            print("Method: %s\n", stat.methodName.c_str());
            print("   Uses: %lu times\n", stat.useCount);
            print("   Avg Ratio: %.3f\n", stat.avgCompressionRatio);
            print("   Avg Time: %lu μs\n", stat.avgTimeUs);
            print("   Success Rate: %.1f%%\n", stat.successRate * 100);
            print("   Adaptive Score: %.3f\n", stat.adaptiveScore);
            print("   Total Savings: %lu bytes\n", stat.totalSavings);
            print("   ───────────────────────\n");
        }
    }

    print("═══════════════════════════════════════\n");
}