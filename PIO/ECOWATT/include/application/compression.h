#ifndef DATACOMPRESSION_H
#define DATACOMPRESSION_H

#include <vector>
#include <algorithm>

#include "peripheral/acquisition.h"  // For RegID enum
#include "peripheral/print.h"

struct SampleBatch {
    static const size_t MAX_REGISTERS = 10;  // Support up to 10 registers
    static const size_t MAX_SAMPLES = 5;     // Still keep 5 samples
    
    uint16_t samples[MAX_SAMPLES][MAX_REGISTERS];  // Dynamic size based on register count
    size_t sampleCount = 0;
    size_t registerCount = 0;  // Track how many registers per sample
    unsigned long timestamps[MAX_SAMPLES];
    
    void addSample(uint16_t* values, unsigned long timestamp, size_t regCount) {
        if (sampleCount < MAX_SAMPLES) {
            registerCount = regCount;  // Store the register count
            memcpy(samples[sampleCount], values, regCount * sizeof(uint16_t));
            timestamps[sampleCount] = timestamp;
            sampleCount++;
        }
    }
    
    bool isFull() const {
        return sampleCount >= MAX_SAMPLES;
    }
    
    void reset() {
        sampleCount = 0;
        registerCount = 0;
    }
    
    // Convert batch to linear array for compression
    void toLinearArray(uint16_t* output) const {
        for (size_t i = 0; i < sampleCount; i++) {
            memcpy(output + (i * registerCount), samples[i], registerCount * sizeof(uint16_t));
        }
    }
};

class DataCompression {
public:
    // ==================== ADAPTIVE SMART SELECTION SYSTEM ====================
    
    // Main smart selection compression method - automatically chooses best algorithm
    static std::vector<uint8_t> compressWithSmartSelection(uint16_t* data, const RegID* selection, size_t count);
    
    // Individual advanced compression methods tested by smart selection
    static std::vector<uint8_t> compressWithDictionary(uint16_t* data, const RegID* selection, size_t count);
    static std::vector<uint8_t> compressWithTemporalDelta(uint16_t* data, const RegID* selection, size_t count);  
    static std::vector<uint8_t> compressWithSemanticRLE(uint16_t* data, const RegID* selection, size_t count);
    
    // ==================== CORE BINARY COMPRESSION METHODS ====================
    
    // Main binary compression with intelligent algorithm selection
    static std::vector<uint8_t> compressBinary(uint16_t* data, size_t count);
    
    // Individual binary compression methods
    static std::vector<uint8_t> compressBinaryBitPacked(uint16_t* data, size_t count, uint8_t bitsPerValue);
    static std::vector<uint8_t> compressBinaryDelta(uint16_t* data, size_t count);
    static std::vector<uint8_t> compressBinaryRLE(uint16_t* data, size_t count);
    static std::vector<uint8_t> storeAsRawBinary(uint16_t* data, size_t count);
    
    // Binary decompression methods
    static std::vector<uint16_t> decompressBinary(const std::vector<uint8_t>& compressed);
    static std::vector<uint16_t> decompressRawBinary(const std::vector<uint8_t>& compressed);
    static std::vector<uint16_t> decompressBinaryBitPacked(const std::vector<uint8_t>& compressed);
    static std::vector<uint16_t> decompressBinaryDelta(const std::vector<uint8_t>& compressed);
    static std::vector<uint16_t> decompressBinaryRLE(const std::vector<uint8_t>& compressed);
    
    // ==================== SMART SELECTION DATA STRUCTURES ====================
    
    // Compression result with academic and traditional ratios
    struct CompressionResult {
        std::vector<uint8_t> data;
        String method;
        float academicRatio;      // Compressed/Original (academic definition - lower is better)
        float traditionalRatio;   // Original/Compressed (traditional definition - higher is better)
        unsigned long timeUs;
        float efficiency;
        bool lossless;
    };
    
    // Test individual compression method and return detailed results
    static CompressionResult testCompressionMethod(const String& method, uint16_t* data, const RegID* selection, size_t count);
    
    // ==================== DICTIONARY MANAGEMENT ====================
    
    // Sensor pattern dictionary for bitmask compression
    struct SensorPattern {
        uint16_t values[10];        // Values for all possible registers (REG_VAC1 to REG_PAC)
        uint32_t frequency;         // Usage frequency for adaptive learning
        uint16_t averageDeltas[10]; // Typical variations from this pattern
        uint8_t confidence;         // Confidence level (0-255) for this pattern
    };
    
    // Dictionary management functions
    static void initializeSensorDictionary();
    static void updateDictionary(uint16_t* data, const RegID* selection, size_t count);
    static int findClosestDictionaryPattern(uint16_t* data, const RegID* selection, size_t count);
    static void printDictionaryStats();
    
    // ==================== TEMPORAL CONTEXT MANAGEMENT ====================
    
    // Temporal context for time-series compression
    struct TemporalContext {
        uint16_t recentSamples[8][10];  // Last 8 samples, up to 10 registers
        RegID lastRegisters[10];        // Register order from last sample
        uint8_t lastRegisterCount;      // Number of registers in last sample
        uint8_t writeIndex;             // Circular buffer write position
        bool bufferFull;                // Whether we have enough temporal history
        unsigned long lastTimestamp;    // Timestamp of last sample
    };
    
    // Temporal management functions
    static void resetTemporalContext();
    static bool isTemporalContextValid(const RegID* selection, size_t count);
    static void printTemporalStats();
    
    // ==================== ADAPTIVE LEARNING SYSTEM ====================
    
    // Method performance tracking for adaptive selection
    struct MethodPerformance {
        String methodName;
        uint32_t useCount;
        float avgCompressionRatio;    // Academic ratio (lower is better)
        unsigned long avgTimeUs;
        float successRate;            // Percentage of successful compressions
        float adaptiveScore;          // Combined performance metric
        unsigned long totalSavings;   // Total bytes saved by this method
    };
    
    // Adaptive learning functions
    static void updateMethodPerformance(const String& method, float academicRatio, unsigned long timeUs);
    static String getAdaptiveRecommendation(uint16_t* data, const RegID* selection, size_t count);
    static void printMethodPerformanceStats();
    static void resetLearningStats();
    
    // ==================== DATA ANALYSIS AND INTELLIGENCE ====================
    
    // Comprehensive data characteristics analysis
    struct DataCharacteristics {
        float repeatRatio;           // Percentage of repeated consecutive values (0.0-1.0)
        float avgDeltaMagnitude;     // Average absolute delta between values
        float largeDeltaRatio;       // Percentage of large deltas (>threshold)
        uint16_t valueRange;         // Max - Min value
        uint16_t uniqueValues;       // Number of unique values in dataset
        bool hasTrend;              // Whether data shows trending behavior
        bool isOscillating;         // Whether data oscillates around a mean
        uint8_t optimalBits;        // Minimum bits needed to represent all values
        uint16_t maxValue;          // Maximum value in dataset
        uint16_t minValue;          // Minimum value in dataset
        float entropy;              // Shannon entropy of the data
        bool suitableForDelta;      // Whether delta compression is recommended
        bool suitableForRLE;        // Whether RLE compression is recommended
        bool suitableForBitPack;    // Whether bit-packing is recommended
        bool suitableForDictionary; // Whether dictionary compression is recommended
    };
    
    static DataCharacteristics analyzeData(uint16_t* data, size_t count);
    
    // Register-specific analysis
    static void getRegisterType(RegID regId, char* result, size_t resultSize);
    static uint8_t getRegisterTypeId(RegID regId);
    static uint16_t getTypeTolerances(uint8_t typeId);
    static uint8_t getBitsForType(uint8_t typeId);
    static uint16_t getTypicalValueRange(RegID regId);
    
    // ==================== PERFORMANCE BENCHMARKING ====================
    
    // Comprehensive performance metrics structure
    struct BinaryCompressionMetrics {
        char method[32];            // Compression method used
        size_t originalSize;        // Original data size in bytes
        size_t compressedSize;      // Compressed data size in bytes
        float academicRatio;        // Academic compression ratio (compressed/original)
        float traditionalRatio;     // Traditional compression ratio (original/compressed)
        unsigned long timeUs;       // Compression time in microseconds
        bool lossless;             // Whether compression is lossless
        float efficiency;          // Efficiency score (savings/time)
        bool recommended;          // Whether this method is recommended
        char suitabilityReason[64];  // Explanation of suitability
        uint8_t methodId;          // Binary method identifier
    };
    
    // Benchmark all methods and return detailed comparison
    static std::vector<BinaryCompressionMetrics> benchmarkAllMethods(uint16_t* data, const RegID* selection, size_t count);
    
    // Measure compression time for specific method
    static unsigned long measureCompressionTime(uint16_t* data, const RegID* selection, size_t count, const char* method);
    
    // ==================== VALIDATION AND INTEGRITY ====================
    
    // Validate compression/decompression integrity
    static bool validateBinaryCompression(uint16_t* original, const std::vector<uint8_t>& compressed, size_t count);
    
    // Validate input data before compression
    static bool validateInputData(uint16_t* data, size_t count);
    static bool validateRegisterSelection(const RegID* selection, size_t count);
    
    // Validate compressed data format
    static bool validateCompressedData(const std::vector<uint8_t>& compressed, const char* method);
    
    // Check if compression method is supported
    static bool isMethodSupported(const char* method);
    
    // ==================== STATISTICS AND REPORTING ====================
    
    // Print compression statistics with both academic and traditional ratios
    static void printCompressionStats(const char* method, size_t originalSize, size_t compressedSize);
    
    // Print comprehensive smart selection analysis report
    static void printSmartSelectionReport(uint16_t* data, const RegID* selection, size_t count);
    
    // Print data analysis report
    static void printDataAnalysisReport(uint16_t* data, size_t count);
    
    // Print method comparison table
    static void printMethodComparison(const std::vector<CompressionResult>& results);
    
    // ==================== MEMORY MANAGEMENT ====================
    
    // ESP32 memory monitoring and optimization
    static void printMemoryUsage();
    static bool checkMemoryAvailable(size_t requiredBytes);
    static size_t calculateMemoryOverhead(size_t dataSize, const char* method);
    static size_t getAvailableHeap();
    static void optimizeMemoryUsage();
    
    // ==================== CONFIGURATION MANAGEMENT ====================
    
    // Runtime configuration
    static void setMaxMemoryUsage(size_t maxBytes);
    static void setCompressionPreference(float preference);  // 0.0=speed, 1.0=compression
    static void setLargeDeltaThreshold(uint16_t threshold);
    static void setBitPackingThreshold(uint8_t minBitsSaved);
    static void setDictionaryLearningRate(float rate);
    static void setTemporalWindowSize(uint8_t size);
    
    // Configuration getters
    static size_t getMaxMemoryUsage();
    static float getCompressionPreference();
    static uint16_t getLargeDeltaThreshold();
    static uint8_t getBitPackingThreshold();
    
    // ==================== ERROR HANDLING ====================
    
    // Error management with detailed categories
    enum ErrorType {
        ERROR_NONE = 0,
        ERROR_INVALID_INPUT,
        ERROR_MEMORY_INSUFFICIENT,
        ERROR_COMPRESSION_FAILED,
        ERROR_DECOMPRESSION_FAILED,
        ERROR_VALIDATION_FAILED,
        ERROR_UNSUPPORTED_METHOD,
        ERROR_DICTIONARY_FULL,
        ERROR_TEMPORAL_CONTEXT_INVALID,
        ERROR_REGISTER_MISMATCH
    };
    
    static void getLastError(char* result, size_t resultSize);
    static ErrorType getLastErrorType();
    static void clearError();
    static bool hasError();
    static void printLastError();
    
    // ==================== LEGACY COMPATIBILITY ====================
    
    // Legacy string interface (uses smart selection internally)
    static void compressRegisterData(uint16_t* data, size_t count, char* result, size_t resultSize);
    static size_t decompressRegisterData(const char* compressed, uint16_t* result, size_t maxCount);
    
    // Base64 encoding/decoding for JSON transport
    static void base64Encode(const std::vector<uint8_t>& data, char* result, size_t resultSize);
    static std::vector<uint8_t> base64Decode(const String& encoded);
    
    // ==================== PUBLIC THRESHOLD CONSTANTS ====================
    
    // Performance thresholds (academic ratios - lower is better)
    static constexpr float EXCELLENT_RATIO_THRESHOLD = 0.5f;  // 50% of original
    static constexpr float GOOD_RATIO_THRESHOLD = 0.67f;      // 67% of original
    static constexpr float POOR_RATIO_THRESHOLD = 0.91f;      // 91% of original
    static constexpr size_t MEMORY_WARNING_THRESHOLD = 2048;  // Warn if < 2KB free

private:
    // ==================== INTERNAL UTILITY FUNCTIONS ====================
    
    // Bit manipulation utilities
    static void packBitsIntoBuffer(uint16_t value, uint8_t* buffer, size_t bitOffset, uint8_t numBits);
    static uint16_t unpackBitsFromBuffer(const uint8_t* buffer, size_t bitOffset, uint8_t numBits);
    
    // Data analysis internal functions
    static size_t countUniqueValues(uint16_t* data, size_t count);
    static size_t countRepeatedPairs(uint16_t* data, size_t count);
    static float calculateDeltaVariance(uint16_t* data, size_t count);
    static bool detectTrend(uint16_t* data, size_t count);
    static bool detectOscillation(uint16_t* data, size_t count);
    static uint8_t calculateOptimalBits(uint16_t* data, size_t count);
    static float calculateEntropy(uint16_t* data, size_t count);
    
    // Smart selection internal functions
    static float calculateCompressionEfficiency(float academicRatio, unsigned long timeUs);
    static String generateSuitabilityReason(const DataCharacteristics& characteristics, const String& method);
    static bool isCompressionBeneficial(float academicRatio);
    static String selectBestMethod(const std::vector<CompressionResult>& results);
    
    // Dictionary internal functions
    static uint32_t calculatePatternDistance(uint16_t* data, const RegID* selection, size_t count, uint8_t patternIndex);
    static bool isPatternUnique(uint16_t* data, const RegID* selection, size_t count);
    static void evictLeastUsedPattern();
    
    // Temporal analysis internal functions
    static uint16_t predictNextValue(RegID regId, uint8_t lookback);
    static float calculatePredictionAccuracy();
    static void updateTemporalStatistics();
    
    // Memory management internal functions
    static void reportMemoryWarning(const String& operation, size_t requiredBytes);
    static bool allocateWorkingMemory(size_t requiredBytes);
    static void freeWorkingMemory();
    
    // Error handling internal functions
    static void setError(const String& errorMsg, ErrorType errorType = ERROR_COMPRESSION_FAILED);
    static void logError(const String& context, const String& errorMsg);
    
    // ==================== STATIC MEMBER VARIABLES ====================
    
    // Error state management
    static String lastErrorMessage;
    static ErrorType lastErrorType;
    static bool debugMode;
    
    // Configuration variables
    static size_t maxMemoryUsage;
    static float compressionPreference;
    static uint16_t largeDeltaThreshold;
    static uint8_t bitPackingThreshold;
    static float dictionaryLearningRate;
    static uint8_t temporalWindowSize;
    
    // Smart Selection System variables
    static SensorPattern sensorDictionary[16];    // Dictionary of common sensor patterns
    static uint8_t dictionarySize;               // Current dictionary size
    static uint32_t smartTotalCompressions;     // Total smart compressions performed
    static TemporalContext temporalBuffer;      // Temporal context for time-series analysis
    static MethodPerformance methodStats[4];   // Performance statistics for each method
    
    // Working memory buffer (to minimize heap fragmentation)
    static uint8_t* workingBuffer;
    static size_t workingBufferSize;
    static bool workingBufferAllocated;
    
    // Performance tracking
    static unsigned long totalCompressions;
    static unsigned long totalDecompressions;
    static float cumulativeCompressionRatio;
    static unsigned long cumulativeCompressionTime;
    
    // ==================== CONSTANTS AND DEFINITIONS ====================
    
    // Binary method identifiers for headers
    static const uint8_t METHOD_ID_RAW_BINARY = 0x00;
    static const uint8_t METHOD_ID_BIT_PACKED = 0x01;
    static const uint8_t METHOD_ID_BINARY_DELTA = 0x02;
    static const uint8_t METHOD_ID_BINARY_RLE = 0x03;
    static const uint8_t METHOD_ID_DICTIONARY = 0xD0;
    static const uint8_t METHOD_ID_TEMPORAL_BASE = 0x70;
    static const uint8_t METHOD_ID_TEMPORAL_DELTA = 0x71;
    static const uint8_t METHOD_ID_SEMANTIC_RLE = 0x50;
    
    // Method name constants
    static const char* METHOD_BINARY_PACKED;
    static const char* METHOD_BINARY_DELTA;
    static const char* METHOD_BINARY_RLE;
    static const char* METHOD_BINARY_HYBRID;
    static const char* METHOD_RAW_BINARY;
    
    // Default configuration values
    static const size_t DEFAULT_MAX_MEMORY = 32768;        // 32KB for ESP32
    static const size_t MAX_DATA_SIZE = 1024;              // Max input array size
    static constexpr float DEFAULT_PREFERENCE = 0.7f;          // Favor compression over speed
    static const uint16_t DEFAULT_LARGE_DELTA_THRESHOLD = 500;
    static const uint8_t DEFAULT_BIT_PACKING_THRESHOLD = 2; // Min bits saved
    static constexpr float DEFAULT_DICTIONARY_LEARNING_RATE = 0.1f;
    static const uint8_t DEFAULT_TEMPORAL_WINDOW_SIZE = 8;
    
    // Performance thresholds (academic ratios - lower is better)
    static const unsigned long MAX_ACCEPTABLE_TIME_US = 200000;  // 200ms max
    
    // Data analysis thresholds
    static constexpr float HIGH_REPEAT_THRESHOLD = 0.3f;       // 30% repeated values
    static constexpr float LOW_DELTA_THRESHOLD = 100.0f;       // Average delta < 100
    static const uint16_t SMALL_RANGE_THRESHOLD = 1024;    // Value range < 1024
    static constexpr float HIGH_ENTROPY_THRESHOLD = 0.8f;      // Entropy > 0.8
    
    // Dictionary and temporal limits
    static const uint8_t MAX_DICTIONARY_SIZE = 15;         // Max dictionary entries
    static const uint8_t MAX_TEMPORAL_HISTORY = 8;         // Max temporal samples
    static const uint32_t DICTIONARY_EVICTION_THRESHOLD = 100;  // Usage threshold
    
    // Memory limits
    static const size_t WORKING_BUFFER_SIZE = 4096;        // 4KB working buffer
    static const size_t MAX_COMPRESSED_SIZE = 8192;        // Max allowed compressed size
};

// ==================== INLINE UTILITY FUNCTIONS ====================

// Quick compression assessment functions (corrected for academic definition)
inline float calculateAcademicCompressionRatio(size_t original, size_t compressed) {
    return (original > 0) ? (float)compressed / (float)original : 1.0f;
}

inline float calculateTraditionalCompressionRatio(size_t original, size_t compressed) {
    return (compressed > 0) ? (float)original / (float)compressed : 0.0f;
}

inline String assessCompressionQuality(float academicRatio) {
    if (academicRatio <= DataCompression::EXCELLENT_RATIO_THRESHOLD) return "Excellent";
    if (academicRatio <= DataCompression::GOOD_RATIO_THRESHOLD) return "Good";
    if (academicRatio <= DataCompression::POOR_RATIO_THRESHOLD) return "Fair";
    if (academicRatio < 1.0f) return "Minimal";
    return "Counterproductive";
}

inline bool isCompressionSuccessful(float academicRatio) {
    return academicRatio < 0.95f;  // Less than 95% of original size
}

inline float calculateCompressionSavings(float academicRatio) {
    return (1.0f - academicRatio) * 100.0f;  // Percentage savings
}

// Register analysis helpers
inline bool isVoltageRegister(RegID regId) {
    return (regId == REG_VAC1 || regId == REG_VPV1 || regId == REG_VPV2);
}

inline bool isCurrentRegister(RegID regId) {
    return (regId == REG_IAC1 || regId == REG_IPV1 || regId == REG_IPV2);
}

inline bool isPowerRegister(RegID regId) {
    return (regId == REG_POW || regId == REG_PAC);
}

// Memory management helpers
inline bool isMemorySufficient(size_t requiredBytes) {
    return ESP.getFreeHeap() > (requiredBytes + DataCompression::MEMORY_WARNING_THRESHOLD);
}

inline size_t calculateBitPackingSavings(uint8_t bitsNeeded, size_t count) {
    size_t originalBytes = count * 2;  // 16 bits per uint16_t
    size_t packedBytes = (count * bitsNeeded + 7) / 8;  // Round up to byte boundary
    return (originalBytes > packedBytes) ? (originalBytes - packedBytes) : 0;
}

// Smart Selection scoring helpers
inline float calculateMethodScore(float academicRatio, unsigned long timeUs, float successRate) {
    // Lower academic ratio is better, faster time is better, higher success rate is better
    float compressionScore = 1.0f / (academicRatio + 0.1f);  // Avoid division by zero
    float speedScore = 1000.0f / (timeUs / 1000.0f + 1.0f);  // Convert to ms, avoid division by zero
    return (compressionScore * 0.5f + speedScore * 0.3f + successRate * 0.2f);
}

// Error checking macros for internal use
#define CHECK_MEMORY_AVAILABLE(bytes) \
    if (!isMemorySufficient(bytes)) { \
        DataCompression::setError("Insufficient memory: " + String(bytes) + " bytes needed", \
                                 DataCompression::ERROR_MEMORY_INSUFFICIENT); \
        return {}; \
    }

#define VALIDATE_INPUT_PARAMS(data, count) \
    if (!DataCompression::validateInputData(data, count)) { \
        return {}; \
    }

#define VALIDATE_REGISTER_SELECTION(selection, count) \
    if (!DataCompression::validateRegisterSelection(selection, count)) { \
        return {}; \
    }

#define LOG_COMPRESSION_ATTEMPT(method, dataSize) \
    if (DataCompression::debugMode) { \
        Serial.printf("Attempting %s compression on %zu bytes\n", method.c_str(), dataSize); \
    }

#endif // DATACOMPRESSION_H