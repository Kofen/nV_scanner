#include "FRAM.h"
#include "AR488_ComPorts.h"

// device address range of i2C FRAM is 0x50 to 0x57

#define PAGE_SIZE 64


_FRAM::_FRAM(uint8_t address, bool pinswap, bool debug) : deviceAddress(address), pinswap(pinswap), debugEnabled(debug) {}

void _FRAM::begin() {
    //Wire.swap(pinswap);
    Wire.begin();
}                                                                                          

uint8_t _FRAM::readByte(uint16_t address) {
    Wire.beginTransmission(deviceAddress);
    Wire.write((address >> 8) & 0xFF);
    Wire.write(address & 0xFF);
    Wire.endTransmission();
    Wire.requestFrom(deviceAddress, (size_t)1);
    printDebug("Byte read.");
    return Wire.available() ? Wire.read() : 0xFF; // Return 0xFF if no data is available
}

void _FRAM::writeByte(uint16_t address, uint8_t data) {
    Wire.beginTransmission(deviceAddress);
    Wire.write((address >> 8) & 0xFF);
    Wire.write(address & 0xFF);
    Wire.write(data & 0xFF);
    Wire.endTransmission();
    delay(10);
    printDebug("Byte written.");
}

void _FRAM::readBytes(uint16_t address, uint8_t* buffer, size_t length) {
    Wire.beginTransmission(deviceAddress);
    Wire.write((address >> 8) & 0xFF);
    Wire.write(address & 0xFF);
    Wire.endTransmission();
    Wire.requestFrom(deviceAddress, (size_t)length);
    for (int i = 0; i < length && Wire.available(); i++) {
        buffer[i] = Wire.read();
    }
    printDebug("Bytes read.");
}

void _FRAM::writeBytes(uint16_t address, const uint8_t* buffer, size_t length) {
    Wire.beginTransmission(deviceAddress);
    Wire.write((address >> 8) & 0xFF);
    Wire.write(address & 0xFF);
    for (int i = 0; i < length; i++) {
        Wire.write(buffer[i] & 0xFF);
    }
    Wire.endTransmission();
    delay(10);
    printDebug("Bytes written.");
}

void _FRAM::writePage(uint16_t address, const uint8_t* data, size_t length) {
    if (length > PAGE_SIZE) {
        printDebug("Error: Length exceeds page size!");
        return;
    }

    uint16_t endAddress = address + length - 1;
    if ((address / PAGE_SIZE) != (endAddress / PAGE_SIZE)) {
        printDebug("Error: Write operation crosses page boundary!");
        return;
    }

    Wire.beginTransmission(deviceAddress);
    Wire.write((address >> 8) & 0xFF);
    Wire.write(address & 0xFF);
    for (uint8_t i = 0; i < length; i++) {
        Wire.write(data[i]);
    }
    Wire.endTransmission();
    delay(10);
    printDebug("Page written within boundary.");
}

void _FRAM::readPage(uint16_t address, uint8_t* data) {
    if (address % PAGE_SIZE != 0) {
        printDebug("Error: Address is not page aligned!");
        return;
    }

    Wire.beginTransmission(deviceAddress);
    Wire.write((address >> 8) & 0xFF);
    Wire.write(address & 0xFF);
    Wire.endTransmission();
    Wire.requestFrom(deviceAddress, (size_t)PAGE_SIZE);
    for (uint8_t i = 0; i < PAGE_SIZE && Wire.available(); i++) {
        data[i] = Wire.read();
    }
    printDebug("Page read.");
}

void _FRAM::printDebug(const String& message) {
    if (debugEnabled) {
        DB_RAW_PRINTLN(message);
    }
}

