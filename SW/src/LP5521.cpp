#include "LP5521.h"

LP5521::LP5521(uint8_t addr) {
  _addr = addr;
}

bool LP5521::begin() {
  Wire.begin();

  enableChip();

  // Enable internal clock and charge pump in 1.5x mode
  writeRegister(CONFIG_REG, 0x11);

  directMode();

  // Set all LEDs off
  ledAllOff();

  // Set default current levels
  setLedCurrent(0xAF, 0xAF, 0xAF);

  return true;
}

void LP5521::directMode() {
  writeRegister(OP_MODE_REG, 0x3F);  // All engines in direct control mode
}

void LP5521::enableChip() {
  writeRegister(ENABLE_REG, 0x40);  // CHIP_EN = 1
}

void LP5521::disableChip() {
  writeRegister(ENABLE_REG, 0x00);
}

void LP5521::setLedCurrent(uint8_t r, uint8_t g, uint8_t b) {
  writeRegister(R_CURRENT_REG, r);
  writeRegister(G_CURRENT_REG, g);
  writeRegister(B_CURRENT_REG, b);
}

void LP5521::setPwm(uint8_t r, uint8_t g, uint8_t b) {
  writeRegister(R_PWM_REG, r);
  writeRegister(G_PWM_REG, g);
  writeRegister(B_PWM_REG, b);
}

void LP5521::ledOn(uint8_t channel) {
  switch (channel) {
    case 0: writeRegister(R_PWM_REG, 0xFF); break;
    case 1: writeRegister(G_PWM_REG, 0xFF); break;
    case 2: writeRegister(B_PWM_REG, 0xFF); break;
  }
}

void LP5521::ledOff(uint8_t channel) {
  switch (channel) {
    case 0: writeRegister(R_PWM_REG, 0x00); break;
    case 1: writeRegister(G_PWM_REG, 0x00); break;
    case 2: writeRegister(B_PWM_REG, 0x00); break;
  }
}

void LP5521::ledAllOff() {
  writeRegister(R_PWM_REG, 0x00);
  writeRegister(G_PWM_REG, 0x00);
  writeRegister(B_PWM_REG, 0x00);
}

void LP5521::selectEngineMode(uint8_t engine, uint8_t mode) {
  // mode: 0 = off, 1 = load, 2 = run, 3 = hold
  uint8_t enableVal = 0x40;  // Always keep CHIP_EN set

  switch (engine) {
    case 0: enableVal |= (mode & 0x03) << 4; break; // R engine
    case 1: enableVal |= (mode & 0x03) << 2; break; // G engine
    case 2: enableVal |= (mode & 0x03);      break; // B engine
    default: return;
  }

  writeRegister(ENABLE_REG, enableVal);
}

void LP5521::loadHeartbeat(uint8_t channel) {
  // Set the selected engine to program mode (OP_MODE)
  uint8_t opmode = 0x00;
  switch (channel) {
    case 0: opmode = 0x01; break; // Engine R
    case 1: opmode = 0x04; break; // Engine G
    case 2: opmode = 0x10; break; // Engine B
    default: return;
  }
  writeRegister(OP_MODE_REG, opmode);

  // Heartbeat-like sequence: fade up, wait, fade down, wait, loop
  static const uint8_t heartbeat_prog[] = {
    0xAD, 0x3F,   // RAMP_UP to PWM=63
    0xAE, 0x10,   // WAIT short
    0xAC, 0x00,   // RAMP_DOWN
    0xAE, 0x20,   // WAIT longer
    0xFE, 0x00    // GOTO start
  };

  writeEngineProgram(channel, heartbeat_prog, sizeof(heartbeat_prog));
  delay(5);

  // Start engine in RUN mode
  selectEngineMode(channel, 2);  // 2 = run
}

void LP5521::writeEngineProgram(uint8_t engine, const uint8_t *prog, uint8_t length) {
  uint8_t base;
  switch (engine) {
    case 0: base = PROG_R_START; break;
    case 1: base = PROG_G_START; break;
    case 2: base = PROG_B_START; break;
    default: return;
  }

  for (uint8_t i = 0; i < length; i += 2) {
    writeRegister(base + i,     prog[i]);     // MSB
    writeRegister(base + i + 1, prog[i + 1]); // LSB
  }
}

void LP5521::writeRegister(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(_addr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}
