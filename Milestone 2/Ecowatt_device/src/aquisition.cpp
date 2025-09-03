#include "aquisition.h"

// ---------------- Internal state ----------------
static AcqSample gBuf[ACQ_BUFFER_SIZE];
static size_t gHead = 0;
static bool   gHasData = false;

static uint8_t  gSlaveId = 0x11;

// Optional write-on-poll
static bool     gDoWrite = true;
static uint16_t gWriteAddr = 8;     // Export power % register (example)
static uint16_t gWriteVal  = 50;    // 50%

// ---------------- Small utilities ----------------
static inline const RegDef* findReg(RegID id) {
  for (size_t i = 0; i < sizeof(REG_MAP)/sizeof(REG_MAP[0]); ++i) {
    if (REG_MAP[i].id == id) return &REG_MAP[i];
  }
  return nullptr;
}

static uint16_t modbusCRC(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int j = 0; j < 8; ++j) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

static String toHex(const uint8_t* data, size_t len) {
  char tmp[3];
  String s;
  for (size_t i = 0; i < len; ++i) {
    sprintf(tmp, "%02X", data[i]);
    s += tmp;
  }
  return s;
}

static bool hexPairToByte(const String& s, int idx, uint8_t& out) {
  if (idx+1 >= s.length()) return false;
  char c1 = s[idx], c2 = s[idx+1];
  if (!isxdigit(c1) || !isxdigit(c2)) return false;
  auto hx = [](char c)->uint8_t {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return 0;
  };
  out = (hx(c1) << 4) | hx(c2);
  return true;
}

// insertion sort for tiny arrays
static void sortU16(uint16_t* a, size_t n) {
  for (size_t i = 1; i < n; ++i) {
    uint16_t key = a[i];
    int j = (int)i - 1;
    while (j >= 0 && a[j] > key) { a[j+1] = a[j]; --j; }
    a[j+1] = key;
  }
}

// group consecutive addresses into [start,count] ranges
struct Range { uint16_t start; uint16_t count; };
static size_t buildRanges(const uint16_t* addrs, size_t n, Range* out, size_t maxOut) {
  if (n == 0) return 0;
  size_t r = 0;
  uint16_t curStart = addrs[0];
  uint16_t curEnd   = addrs[0];
  for (size_t i = 1; i < n; ++i) {
    if (addrs[i] == (uint16_t)(curEnd + 1)) {
      curEnd = addrs[i];
    } else {
      if (r < maxOut) {
        out[r++] = {curStart, (uint16_t)(curEnd - curStart + 1)};
      }
      curStart = curEnd = addrs[i];
    }
  }
  if (r < maxOut) out[r++] = {curStart, (uint16_t)(curEnd - curStart + 1)};
  return r;
}

// ---------------- Frame builders ----------------
static String buildReadFrame(uint8_t slave, uint16_t startAddr, uint16_t count) {
  uint8_t pdu[6];
  pdu[0] = slave;
  pdu[1] = 0x03; // Read Holding Registers
  pdu[2] = (uint8_t)(startAddr >> 8);
  pdu[3] = (uint8_t)(startAddr & 0xFF);
  pdu[4] = (uint8_t)(count >> 8);
  pdu[5] = (uint8_t)(count & 0xFF);
  uint16_t crc = modbusCRC(pdu, 6);
  uint8_t frame[8];
  memcpy(frame, pdu, 6);
  frame[6] = (uint8_t)(crc & 0xFF);        // CRC LSB
  frame[7] = (uint8_t)((crc >> 8) & 0xFF); // CRC MSB
  return toHex(frame, 8);
}

static String buildWriteSingleFrame(uint8_t slave, uint16_t addr, uint16_t value) {
  uint8_t pdu[6];
  pdu[0] = slave;
  pdu[1] = 0x06; // Write Single Register
  pdu[2] = (uint8_t)(addr >> 8);
  pdu[3] = (uint8_t)(addr & 0xFF);
  pdu[4] = (uint8_t)(value >> 8);
  pdu[5] = (uint8_t)(value & 0xFF);
  uint16_t crc = modbusCRC(pdu, 6);
  uint8_t frame[8];
  memcpy(frame, pdu, 6);
  frame[6] = (uint8_t)(crc & 0xFF);
  frame[7] = (uint8_t)((crc >> 8) & 0xFF);
  return toHex(frame, 8);
}

// ---------------- Response decoders ----------------
// Decode JSON {"frame":"..."} and extract hex string
static bool extractFrameHex(const String& json, String& outHex) {
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return false;
  outHex = doc["frame"] | "";
  return outHex.length() >= 6;
}

// Decode a 0x03 response frame into 16-bit words (big-endian).
// Returns number of 16-bit registers decoded, 0 on error/exception.
static size_t decode03Words(const String& frameHex, uint16_t* outWords, size_t maxWords) {
  if (frameHex.length() < 10) return 0; // SA FC BC CRC...
  uint8_t sa, fc, bc;
  if (!hexPairToByte(frameHex, 0, sa)) return 0;
  if (!hexPairToByte(frameHex, 2, fc)) return 0;
  if (fc & 0x80) return 0; // exception
  if (fc != 0x03) return 0;
  if (!hexPairToByte(frameHex, 4, bc)) return 0;
  int dataHexStart = 6;
  int dataHexLen   = bc * 2;
  if ((int)frameHex.length() < dataHexStart + dataHexLen + 4) return 0; // +CRC
  size_t words = bc / 2;
  if (words > maxWords) words = maxWords;

  for (size_t i = 0; i < words; ++i) {
    uint8_t hi, lo;
    int off = dataHexStart + (int)i * 4;
    if (!hexPairToByte(frameHex, off, hi))  return i;
    if (!hexPairToByte(frameHex, off+2, lo)) return i;
    outWords[i] = ((uint16_t)hi << 8) | lo;
  }
  return words;
}

// ---------------- Public API ----------------
void acqSetSlaveId(uint8_t slave) { gSlaveId = slave; }

void acqSetWriteCommand(bool enable, uint16_t addr, uint16_t value) {
  gDoWrite   = enable;
  gWriteAddr = addr;
  gWriteVal  = value;
}

size_t acqCopySamples(AcqSample* out, size_t maxOut) {
  if (!gHasData || maxOut == 0) return 0;
  // Copy newest -> oldest until buffer end
  size_t copied = 0;
  size_t idx = gHead;
  size_t total = gHasData ? ACQ_BUFFER_SIZE : gHead; // when full, ACQ_BUFFER_SIZE valid entries
  for (size_t i = 0; i < total && copied < maxOut; ++i) {
    if (idx == 0) idx = ACQ_BUFFER_SIZE;
    idx--;
    out[copied++] = gBuf[idx];
  }
  return copied;
}

bool acqGetLast(AcqSample& out) {
  if (!gHasData && gHead == 0) return false;
  size_t idx = (gHead == 0) ? (ACQ_BUFFER_SIZE - 1) : (gHead - 1);
  out = gBuf[idx];
  return true;
}

bool pollInverter(ProtocolAdapter& adapter, const RegID* selection, size_t selectionCount) {
  if (!selection || selectionCount == 0) return false;
  if (selectionCount > ACQ_MAX_CHANNELS) selectionCount = ACQ_MAX_CHANNELS;

  // Map RegID -> addresses array
  uint16_t addrs[ACQ_MAX_CHANNELS];
  for (size_t i = 0; i < selectionCount; ++i) {
    const RegDef* rd = findReg(selection[i]);
    if (!rd) return false;
    addrs[i] = rd->addr;
  }

  // Sort addresses and group into consecutive ranges
  sortU16(addrs, selectionCount);
  Range ranges[ACQ_MAX_CHANNELS];
  size_t rCount = buildRanges(addrs, selectionCount, ranges, ACQ_MAX_CHANNELS);

  // Temp store for all fetched (addr->value) pairs
  struct KV { uint16_t addr; uint16_t val; };
  KV kvs[ACQ_MAX_CHANNELS];
  size_t kvn = 0;

  // For each contiguous range, build a single read frame, call adapter, decode values
  for (size_t r = 0; r < rCount; ++r) {
    const uint16_t start = ranges[r].start;
    const uint16_t count = ranges[r].count;

    // Build and send read frame
    String readFrame = buildReadFrame(gSlaveId, start, count);
    String jsonResp  = adapter.readRegister(readFrame);

    // Extract hex, decode 0x03 data words
    String frameHex;
    if (!extractFrameHex(jsonResp, frameHex)) {
      Serial.println(F("acq: JSON/hex extract failed"));
      continue; // try next range
    }

    uint16_t words[ACQ_MAX_CHANNELS];
    size_t got = decode03Words(frameHex, words, ACQ_MAX_CHANNELS);
    if (got < count) {
      Serial.println(F("acq: decode words failed/short"));
      continue;
    }

    // Store into kvs with physical addresses
    for (uint16_t i = 0; i < count && kvn < ACQ_MAX_CHANNELS; ++i) {
      kvs[kvn++] = { (uint16_t)(start + i), words[i] };
    }
  }

  // Build the sample in the original selection order
  AcqSample s; s.timestamp = millis(); s.count = (uint8_t)selectionCount;
  for (size_t i = 0; i < selectionCount; ++i) {
    s.ids[i] = selection[i];
    const RegDef* rd = findReg(selection[i]);
    uint16_t val = 0;
    // find in kvs
    for (size_t k = 0; k < kvn; ++k) {
      if (kvs[k].addr == rd->addr) { val = kvs[k].val; break; }
    }
    s.values[i] = val;
  }

  // Push to ring buffer
  gBuf[gHead] = s;
  gHead = (gHead + 1) % ACQ_BUFFER_SIZE;
  if (gHead == 0) gHasData = true;

  // Optional write each poll (example: export power %)
  if (gDoWrite) {
    String w = buildWriteSingleFrame(gSlaveId, gWriteAddr, gWriteVal);
    (void)adapter.writeRegister(w); // ignore content; adapter already logs/validates
  }

  // Debug print
  Serial.print(F("acq: stored sample @"));
  Serial.print(s.timestamp);
  Serial.print(F(" ms -> "));
  for (size_t i = 0; i < s.count; ++i) {
    const RegDef* rd = findReg(s.ids[i]);
    Serial.print(rd ? rd->name : "?");
    Serial.print('=');
    Serial.print(s.values[i]);
    Serial.print(' ');
  }
  Serial.println();

  return true;
}
