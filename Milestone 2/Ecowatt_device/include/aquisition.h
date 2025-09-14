#ifndef AQUISITION_H
#define AQUISITION_H

#pragma once
#include <Arduino.h>

// Register identifiers
enum RegID : uint8_t {
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
};

// register definition
struct RegisterDef {
  RegID id;
  uint16_t addr;       // Modbus register address
  const char* name;    // identification name
};


//Save the decoded register values in this struct
struct DecodedValues {
  uint16_t values[10];   // holds decoded register values
  size_t count;          // number of valid values
};


// Lookup table
static const RegisterDef REGISTER_MAP[] = {
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
static const size_t REGISTER_COUNT = sizeof(REGISTER_MAP)/sizeof(REGISTER_MAP[0]);


// Helper to find register definition
const RegisterDef* findRegister(RegID id);


/*

Build a Modbus read frame for a given selection of registers.
slave     - inverter address (usually 0x11)
regs[]    - list of requested registers
regCount  - how many registers requested
outStart  - start address of contiguous block
outCount  - number of registers in block
Returns frame as HEX string (ready to send via protocol adapter).

*/

String buildReadFrame(uint8_t slave, const RegID* regs, size_t regCount,
                      uint16_t& outStart, uint16_t& outCount);


// Build a Modbus write frame to set a single register                   
String buildWriteFrame(uint8_t slave, uint16_t regAddr, uint16_t value);


//Set the power output
bool setPower(uint16_t powerValue);

/*

Decode a Modbus response frame.
frameHex  - inverter response (HEX string)
startAddr - start of requested block
count     - number of registers read in block
regs[]    - list of requested registers
regCount  - how many registers requested
Returns DecodedValues struct containing only the requested registers.

*/
DecodedValues decodeReadResponse(const String& frameHex,
                                 uint16_t startAddr,
                                 uint16_t count,
                                 const RegID* regs,
                                 size_t regCount);


                                 
// Poll the inverter for a set of registers and return decoded results
DecodedValues readRequest(const RegID* regs, size_t regCount);
#endif