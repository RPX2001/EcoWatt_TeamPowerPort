#ifndef AQUISITION_H
#define AQUISITION_H

#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "protocol_adapter.h"   // your existing adapter

// ---- REGISTER CATALOG ----
// Add/adjust addresses to match your inverter's map.
enum RegID : uint8_t {
  REG_VAC1 = 0,     // L1 voltage
  REG_IAC1 = 1,     // L1 current
  REG_FAC1 = 2,     // L1 frequency
  REG_VPV1 = 3,     // PV1 voltage
  REG_VPV2 = 4,     // PV2 voltage
  REG_IPV1 = 5,     // PV1 current
  REG_IPV2 = 6,     // PV2 current
  REG_TEMP = 7,     // Internal temp
  REG_EXPORT_PCT = 8, // Export power %, RW (example write)
  REG_PAC  = 9,     // AC power
  // ...extend with more as needed
};

// Describe a register (name used for debug printing)
struct RegDef {
  RegID id;
  uint16_t addr;
  const char* name;
};

// Update addresses/names as per your acquisition table
static const RegDef REG_MAP[] = {
  {REG_VAC1,        0, "Vac1"},
  {REG_IAC1,        1, "Iac1"},
  {REG_FAC1,        2, "Fac1"},
  {REG_VPV1,        3, "Vpv1"},
  {REG_VPV2,        4, "Vpv2"},
  {REG_IPV1,        5, "Ipv1"},
  {REG_IPV2,        6, "Ipv2"},
  {REG_TEMP,        7, "Temp"},
  {REG_EXPORT_PCT,  8, "Export%"},
  {REG_PAC,         9, "Pac"},
};

// ---- SAMPLE BUFFER (ring) ----
#ifndef ACQ_MAX_CHANNELS
#define ACQ_MAX_CHANNELS 16
#endif
#ifndef ACQ_BUFFER_SIZE
#define ACQ_BUFFER_SIZE  32
#endif

struct AcqSample {
  uint32_t timestamp;
  uint8_t  count;
  RegID    ids[ACQ_MAX_CHANNELS];
  uint16_t values[ACQ_MAX_CHANNELS]; // raw 16-bit words
};

// Access latest N samples (copy out)
size_t acqCopySamples(AcqSample* out, size_t maxOut);

// Optional: inspect last sample quickly
bool acqGetLast(AcqSample& out);

// Configure default slave ID used in frames (default 0x11)
void acqSetSlaveId(uint8_t slave);

// Configure write command that runs each poll (optional)
void acqSetWriteCommand(bool enable, uint16_t addr, uint16_t value);

// Core: build frames, call adapter, decode & store sample
// selection: array of RegID, selectionCount: how many selected regs
// Returns true if a sample was stored.
bool pollInverter(ProtocolAdapter& adapter, const RegID* selection, size_t selectionCount);



#endif