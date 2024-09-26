// McpDigitalPot 2-channel Digital Potentiometer
// ww1.microchip.com/downloads/en/DeviceDoc/22059b.pdf

#ifndef McpDigitalPot_h
#define McpDigitalPot_h

#include <Arduino.h>

class McpDigitalPot
{
public:
  // You must at least specify the slave select pin
  McpDigitalPot(uint8_t slave_select);

  // Read potentiometer values
  unsigned int getPosition(unsigned int wiperIndex);

  void setPosition(unsigned int wiperIndex, unsigned int position);
  void writePosition(unsigned int wiperIndex, unsigned int position);

protected:
  uint8_t slave_select_pin;

  const static uint8_t kADR_WIPER0 = B00000000;
  const static uint8_t kADR_WIPER1 = B00010000;

  const static uint8_t kCMD_READ = B00001111;
  const static uint8_t kCMD_WRITE = B00000000;

  const static uint8_t kADR_VOLATILE = B00000000;
  const static uint8_t kADR_NON_VOLATILE = B00100000;

  const static uint8_t kTCON_REGISTER = B01000000;
  const static uint8_t kSTATUS_REGISTER = B01010000;

  uint16_t byte2uint16(byte high_byte, byte low_byte);

  void initSpi(uint8_t slave_select_pin);
  void initResistance(float rAB_ohms, float rW_ohms);

  uint16_t spiRead(byte cmd_byte);
  void spiWrite(byte cmd_byte, byte data_byte);
  void internalSetWiperPosition(byte wiperAddress, unsigned int position, bool isNonVolatile);
};

#endif // McpDigitalPot_h
