/**
 * @file acquisition.c
 * @brief Data acquisition from Inverter via Modbus protocol.
 *
 * @author Ravin, Yasith
 * @version 1.1
 * @date 2025-10-01
 *
 * @par Revision history
 * - 1.0 (2025-09-09, Ravin): Original file.
 * - 1.1 (2025-10-01, Yasith): Added detailed documentation, made portable.
 */

#include "acquisition.h"

/**
 * @defgroup Acquisition_Helper_Private Helper Functions
 * @brief Internal helper functions for building frames and decoding responses.
 * @ingroup Acquisition
 * @{
 */

/**
 * @fn static void build_read_frame(uint8_t slave, const RegID* regs, size_t regCount, uint16_t* outStart, uint16_t* outCount, char* frameHex, size_t frameHexSize)
 * @brief Builds a Modbus read frame for the specified registers.
 * 
 * @param slave The Modbus slave address.
 * @param regs An array of RegID specifying which registers to read.
 * @param regCount The number of registers in the regs array.
 * @param outStart Pointer to store the starting register address.
 * @param outCount Pointer to store the number of registers to read.
 * @param frameHex Buffer to store the resulting Modbus frame as a hexadecimal string.
 * @param frameHexSize The size of the frameHex buffer.
 */
static void build_read_frame(uint8_t slave, const RegID* regs, size_t regCount, uint16_t* outStart, uint16_t* outCount, char* frameHex, size_t frameHexSize);

/**
 * @fn static void build_write_frame(uint8_t slave, uint16_t regAddr, uint16_t value,
 * @brief Builds a Modbus write frame for a single register.
 * 
 * @param slave The Modbus slave address.
 * @param regAddr The register address to write to.
 * @param value The value to write to the register.
 * @param frameHex Buffer to store the resulting Modbus frame as a hexadecimal string.
 * @param frameHexSize The size of the frameHex buffer.
 */
static void build_write_frame(uint8_t slave, uint16_t regAddr, uint16_t value, char* frameHex, size_t frameHexSize);

/**
 * @fn static void to_hex(const uint8_t* data, size_t len, char* outBuf, size_t outBufSize)
 * @brief Converts binary data to a hexadecimal string.
 * 
 * @param data The binary data to convert.
 * @param len The length of the binary data.
 * @param outBuf The buffer to store the resulting hexadecimal string.
 * @param outBufSize The size of the outBuf buffer.
 */
static void to_hex(const uint8_t* data, size_t len, char* outBuf, size_t outBufSize);

/**
 * @fn static uint16_t calculate_crc(const uint8_t* data, int length)
 * @brief Calculates the Modbus CRC16 checksum for the given data.
 * 
 * @param data The data to calculate the CRC for.
 * @param length The length of the data.
 * 
 * @return uint16_t The calculated CRC16 checksum.
 */
static uint16_t calculate_crc(const uint8_t* data, int length);

/**
 * @fn static const RegisterDef* find_register(RegID id)
 * @brief Finds a register definition by its ID.
 * 
 * @param id The ID of the register to find.
 * 
 * @return const RegisterDef* A pointer to the found register definition, or NULL if not found.
 */
static const RegisterDef* find_register(RegID id);

/**
 * @fn static DecodedValues decode_read_response(const char* frameHex, uint16_t startAddr, uint16_t count, const RegID* regs, size_t regCount)
 * @brief Decodes a Modbus read response frame and extracts the requested register values.
 * 
 * @param frameHex The Modbus response frame as a hexadecimal string.
 * @param startAddr The starting register address that was requested.
 * @param count The number of registers that were requested.
 * @param regs An array of RegID specifying which registers to extract.
 * @param regCount The number of registers in the regs array.
 * 
 * @return DecodedValues A struct containing the decoded register values.
 */
static DecodedValues decode_read_response(const char* frameHex, uint16_t startAddr, uint16_t count, const RegID* regs, size_t regCount);

/** @} */ // end of Acquisition_Helper_Private



// Sets the power value on the inverter.
bool set_power(uint16_t powerValue) 
{
    char frame[32];
    build_write_frame(0x11, 8, powerValue, frame, sizeof(frame));

    print("Sending write frame: %s\n", frame);

    // Call adapter to actually send (real implementation)
    char response[128];
    adapter_writereg(frame, response, sizeof(response));

    if (strcmp(response, frame) == 0) 
    {
        print("Power set to %u successfully\n", powerValue);
        return true;
    } 
    else 
    {
        print("Failed to set power, response mismatch\n");
        print("Raw response: %s\n", response);
        return false;
    }
}

// Reads the specified registers from the inverter.
DecodedValues read_request(const RegID* regs, size_t regCount) 
{
    adapter_set_ssid("Raveenpsp");
    adapter_set_password("raveen1234");
    adapter_set_api_key("NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTFmOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExNQ==");
    print("Connecting inverter");
    while(!adapter_begin())
    {
        delay_wait(500);
        print(".");
    }
    print("\rInverter connected\n");

    DecodedValues result;
    result.count = 0;

    // Build read frame
    uint16_t startAddr = 0, count = 0;
    char frame[32];   // ---------------------------------------------------------------RECHECK!
    build_read_frame(0x11, regs, regCount, &startAddr, &count, frame, sizeof(frame));
    if (frame[0] == '\0') 
    {
        print("Failed to build read frame.\n");
        return result;
    }

    // Send request
    char response[256];
    int err = adapter_readreg(frame, response, sizeof(response));
    if (err != 200)
    {
        print("Read request failed. %d\n", err);
        return result;
    } 

    // Decode response
    result = decode_read_response(response, startAddr, count, regs, regCount);

    return result;
}

// Builds a Modbus read frame for the specified registers.
static void build_read_frame(uint8_t slave, const RegID* regs, size_t regCount, uint16_t* outStart, uint16_t* outCount, char* frameHex, size_t frameHexSize) 
{
    // Find min/max addresses
    uint16_t start = 0xFFFF, end = 0;
    for (size_t i = 0; i < regCount; i++) 
    {
        const RegisterDef* rd = find_register(regs[i]);
        if (!rd) continue;
        if (rd->addr < start) start = rd->addr;
        if (rd->addr > end)   end   = rd->addr;
    }
    *outStart = start;
    *outCount = (end - start) + 1;

    // Build Modbus frame
    uint8_t frame[8];
    frame[0] = slave;
    frame[1] = 0x03; // read holding regs
    frame[2] = (start >> 8) & 0xFF;
    frame[3] = start & 0xFF;
    frame[4] = (*outCount >> 8) & 0xFF;
    frame[5] = *outCount & 0xFF;

    uint16_t crc = calculate_crc(frame, 6);
    frame[6] = crc & 0xFF;
    frame[7] = (crc >> 8) & 0xFF;

    to_hex(frame, 8, frameHex, frameHexSize);
}

// Builds a Modbus write frame for a single register.
static void build_write_frame(uint8_t slave, uint16_t regAddr, uint16_t value, char* frameHex, size_t frameHexSize) 
{
    uint8_t frame[8];
    frame[0] = slave;
    frame[1] = 0x06; // write single register
    frame[2] = (regAddr >> 8) & 0xFF;
    frame[3] = regAddr & 0xFF;
    frame[4] = (value >> 8) & 0xFF;
    frame[5] = value & 0xFF;

    uint16_t crc = calculate_crc(frame, 6);
    frame[6] = crc & 0xFF;
    frame[7] = (crc >> 8) & 0xFF;

    to_hex(frame, 8, frameHex, frameHexSize);
}

// Converts binary data to a hexadecimal string.
static void to_hex(const uint8_t* data, size_t len, char* outBuf, size_t outBufSize) 
{
    outBuf[0] = '\0';
    char buf[3];
    for (size_t i = 0; i < len; i++) 
    {
        snprintf(buf, sizeof(buf), "%02X", data[i]);
        strncat(outBuf, buf, outBufSize - strlen(outBuf) - 1);
    }
}

// Calculates the Modbus CRC16 checksum for the given data.
static uint16_t calculate_crc(const uint8_t* data, int length) 
{
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < length; i++) 
  {
    crc ^= (data[i] & 0xFF);
    for (int j = 0; j < 8; j++) 
    {
      if (crc & 0x0001) 
      {
        crc >>= 1;
        crc ^= 0xA001;
      } 
      else 
      {
        crc >>= 1;
      }
    }
  }
  return crc & 0xFFFF;
}

// Finds a register definition by its ID.
static const RegisterDef* find_register(RegID id) 
{
    for (size_t i = 0; i < REGISTER_COUNT; i++) 
    {
        if (REGISTER_MAP[i].id == id) return &REGISTER_MAP[i];
    }
    return NULL;
}

// Decodes a Modbus read response frame and extracts the requested register values.
static DecodedValues decode_read_response(const char* frameHex, uint16_t startAddr, uint16_t count, const RegID* regs, size_t regCount) 
{
    DecodedValues result;
    result.count = 0;

    if (strlen(frameHex) < 10) return result;

    char temp[3];
    temp[2] = '\0';

    // Function code
    strncpy(temp, frameHex + 2, 2);
    uint8_t func = (uint8_t)strtol(temp, NULL, 16);
    if (func != 0x03) return result;

    // Byte count
    strncpy(temp, frameHex + 4, 2);
    uint8_t byteCount = (uint8_t)strtol(temp, NULL, 16);
    if (byteCount != count * 2) return result;

    uint16_t allRegs[64];
    for (uint16_t i = 0; i < count; i++) 
    {
        strncpy(temp, frameHex + 6 + i*4, 2);
        uint16_t hi = (uint16_t)strtol(temp, NULL, 16);

        strncpy(temp, frameHex + 8 + i*4, 2);
        uint16_t lo = (uint16_t)strtol(temp, NULL, 16);

        allRegs[i] = (hi << 8) | lo;
    }

    for (size_t i = 0; i < regCount; i++) 
    {
        const RegisterDef* rd = find_register(regs[i]);
        if (!rd) 
        {
            result.values[result.count++] = 0;
            continue;
        }
        result.values[result.count++] = allRegs[rd->addr - startAddr];
    }

    return result;
}