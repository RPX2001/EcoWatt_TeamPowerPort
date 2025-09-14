/** 
 * @file acquisition.h
 * @brief Header for acquisition module to interact with inverter registers.
 *
 * @author Ravin, Yasith
 * @version 1.1
 * @date 2025-10-01
 * 
 * @par Revision history
 * - 1.0 (2025-09-09, Ravin): Original file.
 * - 1.1 (2025-10-01, Yasith): Added detailed documentation, made portable.
 */

#ifndef AQUISITION_H
#define AQUISITION_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "protocol_adapter.h"
#include "debug_out.h"
#include "delay.h"

// Register identifiers
typedef enum 
{
  REG_VAC1,
  REG_IAC1,
  REG_FAC1,
  REG_VPV1,
  REG_VPV2,
  REG_IPV1,
  REG_IPV2,
  REG_TEMP,
  REG_POW,
  REG_PAC
} 
RegID;

typedef struct 
{
    RegID id;
    uint16_t addr;        // Modbus register address
    const char* name;     // Identification name
} 
RegisterDef;

//Save the decoded register values in this struct
typedef struct 
{
    uint16_t values[10];  // Holds decoded register values
    size_t count;         // Number of valid values
} 
DecodedValues;


// Lookup table
static const RegisterDef REGISTER_MAP[] = 
{
    {REG_VAC1, 0, "Vac1"},
    {REG_IAC1, 1, "Iac1"},
    {REG_FAC1, 2, "Fac1"},
    {REG_VPV1, 3, "Vpv1"},
    {REG_VPV2, 4, "Vpv2"},
    {REG_IPV1, 5, "Ipv1"},
    {REG_IPV2, 6, "Ipv2"},
    {REG_TEMP, 7, "Temp"},
    {REG_POW,  8, "Pow"},
    {REG_PAC,  9, "Pac"}
};

// number of registers in the map
static const size_t REGISTER_COUNT = sizeof(REGISTER_MAP) / sizeof(REGISTER_MAP[0]);

/**
 * @defgroup Acquisition Acquisition Module
 * @brief Functions for reading and writing inverter registers.
 * @{
 */

/**
 * @fn bool set_power(uint16_t powerValue)
 * @brief Sets the power value on the inverter.
 * 
 * @param powerValue The power value to set (uint16_t).
 * 
 * @return bool Returns true if the power was set successfully, false otherwise.
 */
bool set_power(uint16_t powerValue);

/**
 * @fn DecodedValues read_request(const RegID* regs, size_t regCount)
 * @brief Reads the specified registers from the inverter.
 * 
 * @param regs An array of RegID specifying which registers to read.
 * @param regCount The number of registers to read (size_t).
 * 
 * @return DecodedValues A struct containing the decoded register values and count.
 */
DecodedValues read_request(const RegID* regs, size_t regCount);

/** @} */ // end of Acquisition

#endif