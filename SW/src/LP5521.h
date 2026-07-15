#ifndef LP5521_H
#define LP5521_H

#include <Arduino.h>
#include <Wire.h>

#define LP5521_ADDR 0x32  // 7-bit I2C address

#define ENABLE_REG         0x00
#define OP_MODE_REG        0x01

#define R_PWM_REG          0x02
#define G_PWM_REG          0x03
#define B_PWM_REG          0x04

#define R_CURRENT_REG      0x05
#define G_CURRENT_REG      0x06
#define B_CURRENT_REG      0x07

#define CONFIG_REG         0x08

#define R_PC_REG           0x09
#define G_PC_REG           0x0A
#define B_PC_REG           0x0B

#define STATUS_REG         0x0C
#define RESET_REG          0x0D
#define GPO_CTRL_REG       0x0E

// Program memory base addresses (per engine)
#define PROG_R_START       0x10  // R engine: 0x10–0x2F
#define PROG_G_START       0x30  // G engine: 0x30–0x4F
#define PROG_B_START       0x50  // B engine: 0x50–0x6F

class LP5521 {
public:
  LP5521(uint8_t addr = LP5521_ADDR);
  bool begin();

  void enableChip();
  void disableChip();
  void directMode();

  void setLedCurrent(uint8_t r, uint8_t g, uint8_t b);
  void setPwm(uint8_t r, uint8_t g, uint8_t b);
  void ledOn(uint8_t channel);   // 0=R, 1=G, 2=B
  void ledOff(uint8_t channel);
  void ledAllOff();

  void loadHeartbeat(uint8_t channel);

private:
  uint8_t _addr;

  void writeRegister(uint8_t reg, uint8_t val);
  void writeEngineProgram(uint8_t engine, const uint8_t* prog, uint8_t length);
  void selectEngineMode(uint8_t engine, uint8_t mode);
};

#endif
