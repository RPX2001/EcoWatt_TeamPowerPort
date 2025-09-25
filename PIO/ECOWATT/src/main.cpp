#include <Arduino.h>
#include "peripheral/acquisition.h"
#include "peripheral/print.h"
#include "peripheral/arduino_wifi.h"
#include "application/ringbuffer.h"
#include "application/compression.h"

RingBuffer<CompressedData, 450> ringBuffer;
Arduino_Wifi Wifi;

void Wifi_init();
void poll_and_save();
void upload_data();

hw_timer_t *poll_timer = NULL;
volatile bool poll_token = false;

void IRAM_ATTR set_poll_token() 
{
  poll_token = true;
}

hw_timer_t *upload_timer = NULL;
volatile bool upload_token = false;

void IRAM_ATTR set_upload_token() 
{
  upload_token = true;
}

// Define registers to read
const RegID test_selection[] = {REG_VAC1, REG_IAC1, REG_IPV1, REG_PAC};

void setup() {
  print_init();
  print("Starting ECOWATT\n");

  Wifi_init();

  poll_timer = timerBegin(0, 80, true); // Timer 0, prescaler 80 (1us per tick)
  timerAttachInterrupt(poll_timer,  &set_poll_token, true);
  timerAlarmWrite(poll_timer, 2000000, true); // 2 seconds
  timerAlarmEnable(poll_timer); // Enable the alarm

  upload_timer = timerBegin(1, 80, true); // Timer 1, prescaler 80 (1us per tick)
  timerAttachInterrupt(upload_timer,  &set_upload_token, true);
  timerAlarmWrite(upload_timer, 900000000, true); // 15 minutes
  timerAlarmEnable(upload_timer); // Enable the alarm

  //set power to 50W out
  bool ok = setPower(50); // set Pac = 50W
  if (ok) 
  {
    print("Output power register updated!\n");
  }
  else 
  {
    print("Failed to set output power register!\n");
  }

  while(true) 
  {
    if (poll_token) 
    {
      poll_token = false;
      poll_and_save();
    }
    
    if (upload_token) 
    {
      upload_token = false;
      upload_data();
    }
  }
}

void loop() {}

void Wifi_init()
{
  Wifi.setSSID("YasithsRedmi");
  Wifi.setPassword("xnbr2615");
  Wifi.begin();
}

void poll_and_save()
{
  // poll inverter
  DecodedValues values = readRequest(test_selection, 4);
  // print results from the inverter sim
  print("Decoded Values:\n");
  for (size_t i = 0; i < values.count; i++) 
  {
    print("  [%d] = %u\n", i, values.values[i]);
  }

  char compressed[128];
  DataCompression::compressRegisterData(values.values, values.count, compressed, sizeof(compressed), true);
  size_t originalSize = values.count * sizeof(uint16_t);
  size_t compressedSize = strlen(compressed);
  CompressedData data(compressed, true, values.count);
  ringBuffer.push(data);

  print("Original values: [ %u, %u, %u, %u ]\n", values.values[0], values.values[1], values.values[2], values.values[3]);
  // print("Compressed: " + compressed);
  DataCompression::printCompressionStats("Delta", originalSize, compressedSize);
}

void upload_data()
{

}