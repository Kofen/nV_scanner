#ifndef _FRAM_H
#define _FRAM_H

#include <Arduino.h>
#include <Wire.h>

class _FRAM {
    
public:
    _FRAM(uint8_t address = 0x51, bool pinswap = false, bool debug = false);

    void begin();


    uint8_t readByte(uint16_t address);
    void writeByte(uint16_t address, uint8_t data);

    void readBytes(uint16_t address, uint8_t* buffer, size_t length);
    void writeBytes(uint16_t address, const uint8_t* buffer, size_t length);

    void writePage(uint16_t address, const uint8_t* data, size_t length);
    void readPage(uint16_t address, uint8_t* data);
private:


    uint8_t deviceAddress;
    bool debugEnabled;
    bool pinswap;

    void printDebug(const String& message);
};

#endif

