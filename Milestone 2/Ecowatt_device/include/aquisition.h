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

// One register definition
struct RegisterDef {
  RegID id;
  uint16_t addr;       // Modbus register address
  const char* name;    // For debug/logging
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

static const size_t REGISTER_COUNT = sizeof(REGISTER_MAP)/sizeof(REGISTER_MAP[0]);

// Helper to find register definition
const RegisterDef* findRegister(RegID id);


#endif