#ifndef SingleDS18B20_h
#define SingleDS18B20_h

#include <OneWire.h>

class SingleDS18B20: public OneWire {

  private:
    bool _tempSensorFound = false;
    byte _owROMCode[8];

    boolean readScratchPad(byte addr[], byte data[]);
    void writeScratchPad(byte addr[], byte th, byte tl, byte cfg);
    void copyScratchPad(byte addr[]);
    void startConvertT(byte addr[]);

  public:
    SingleDS18B20(uint8_t owPin);
    bool GetReady();
    float ReadTemp();


};

#endif
