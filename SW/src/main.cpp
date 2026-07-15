#include <Arduino.h>
#include <Wire.h>
#include <Ethernet.h>
#include "24AA256UID.h"
#include "user_interface.h"
#include "config.h"
#include "LP5521.h"
#include <Adafruit_MS8607.h>
#include <Adafruit_Sensor.h>

#ifdef INTERFACE_VXI11
#include "rpc_bind_server.h"
#include "vxi_server.h"
#endif

// ---------------- Pin/Board Config ----------------
#define LATCH_ROW0 0b00000001
#define LATCH_COL0 0b00000010
#define LATCH_ROW1 0b00000100
#define LATCH_COL1 0b00001000
#define LATCH_ROW2 0b00010000
#define LATCH_COL2 0b00100000
#define LATCH_ROW3 0b01000000
#define LATCH_COL3 0b10000000

const uint8_t latchPin = 11;
const uint8_t clockPin = 10;
const uint8_t dataPin  = 9;

#define LP5521_ADDR 0x32

// ---------------- Drivers/Peripherals ----------------
LP5521 ledDriver;
Adafruit_MS8607 ms8607;               

// ---------------- Shift Register / LED Matrix ----------------
const uint8_t numBoards    = 4;                 // 4 x 16ch boards
const uint8_t numRegisters = numBoards * 4;     // 4 regs per board
uint8_t leds[numRegisters] = {0};

_24AA256UID eeprom(0x50, true);

// ---------------- Shift Register Helpers ----------------
void updateShiftRegister() {
  digitalWrite(latchPin, LOW);
  for (int i = 0; i < numRegisters; i++) {
    shiftOut(dataPin, clockPin, LSBFIRST, leds[i]);
  }
  digitalWrite(latchPin, HIGH);
}

void setLED(int8_t bot, int8_t top) {
  memset(leds, 0, sizeof(leds));

  if (top >= 0 && top < (numBoards * 16)) {
    uint8_t idx = (numBoards * 16 - 1) - top;
    uint8_t reg = idx / 4;
    uint8_t bit = (idx % 4) + 4;
    bitSet(leds[reg], bit);
  }

  if (bot >= 0 && bot < (numBoards * 16)) {
    uint8_t idx = (numBoards * 16 - 1) - bot;
    uint8_t reg = idx / 4;
    uint8_t bit = idx % 4;
    bitSet(leds[reg], bit);
  }

  updateShiftRegister();
}

void latchByte(uint8_t value, uint8_t latchPinMask) {
  VPORTD.OUT = value;
  VPORTC.OUT |= latchPinMask;
  delayMicroseconds(1);
  VPORTC.OUT &= ~latchPinMask;
}

const uint8_t ROW_L_MAP[4] = { 1, 3, 5, 7 };
const uint8_t ROW_H_MAP[4] = { 0, 2, 4, 6 };

uint8_t makeRowData(uint8_t row_h_mask, uint8_t row_l_mask) {
  uint8_t result = 0;
  for (uint8_t i = 0; i < 4; i++) {
    if (bitRead(row_h_mask, i)) bitSet(result, ROW_H_MAP[i]);
    if (bitRead(row_l_mask, i)) bitSet(result, ROW_L_MAP[i]);
  }
  return result;
}

uint8_t getLatchRowMask(uint8_t relay) {
  if (relay < 16) return LATCH_ROW0;
  else if (relay < 32) return LATCH_ROW1;
  else if (relay < 48) return LATCH_ROW2;
  else return LATCH_ROW3;
}

uint8_t getLatchColMask(uint8_t relay) {
  if (relay < 16) return LATCH_COL0;
  else if (relay < 32) return LATCH_COL1;
  else if (relay < 48) return LATCH_COL2;
  else return LATCH_COL3;
}

void setRelay(uint8_t row, uint8_t col, uint8_t relayNum) {
  uint8_t rowMask = getLatchRowMask(relayNum);
  uint8_t colMask = getLatchColMask(relayNum);

  latchByte(makeRowData(0x0F, 0x00), rowMask);
  delayMicroseconds(5);
  latchByte(~(1 << col), colMask);
  delayMicroseconds(10);
  latchByte(makeRowData(0x0F, (1 << row)), rowMask);
  delay(10);
  latchByte(makeRowData(0x0F, 0x00), rowMask);
  latchByte(0, colMask);
}

void resetRelay(uint8_t row, uint8_t col, uint8_t relayNum) {
  uint8_t rowMask = getLatchRowMask(relayNum);
  uint8_t colMask = getLatchColMask(relayNum);

  latchByte(makeRowData(0x0F, 0x00), rowMask);
  delayMicroseconds(5);
  latchByte((1 << col), colMask);
  delayMicroseconds(10);
  latchByte(makeRowData(~(1 << row), 0x00), rowMask);
  delay(10);
  latchByte(makeRowData(0x0F, 0x00), rowMask);
  latchByte(0, colMask);
}

// ---------------- Relay State & Helpers ----------------
int8_t currentRelayA = -1;
int8_t currentRelayB = -1;

void relayAtoCoord(uint8_t relay, uint8_t &col, uint8_t &row) {
  uint8_t relayInBoard = relay % 16;
  col = (relayInBoard / 4) * 2;
  row = relayInBoard % 4;
}

void relayBtoCoord(uint8_t relay, uint8_t &col, uint8_t &row) {
  uint8_t relayInBoard = relay % 16;
  col = (relayInBoard / 4) * 2 + 1;
  row = relayInBoard % 4;
}

void deactivateRelayB();

void activateRelayA(uint8_t relay) {
  if (relay == currentRelayA) return;  // Already active

  if (currentRelayA != -1) {
    uint8_t col, row;
    relayAtoCoord(currentRelayA, col, row);
    resetRelay(row, col, currentRelayA);
    delay(50);
  }

  uint8_t col, row;
  relayAtoCoord(relay, col, row);
  setRelay(row, col, relay);
  currentRelayA = relay;
  setLED(currentRelayB, relay);
}

void deactivateRelayA() {
  if (currentRelayA != -1) {
    uint8_t col, row;
    relayAtoCoord(currentRelayA, col, row);
    resetRelay(row, col, currentRelayA);
    currentRelayA = -1;
    setLED(currentRelayB, -1);
  }
}

void activateRelayB(uint8_t relay) {
  if (relay == currentRelayB) return;  // Already active

  if (currentRelayB != -1) {
    uint8_t col, row;
    relayBtoCoord(currentRelayB, col, row);
    resetRelay(row, col, currentRelayB);
    delay(50);
  }

  uint8_t col, row;
  relayBtoCoord(relay, col, row);
  setRelay(row, col, relay);
  currentRelayB = relay;
  setLED(relay, currentRelayA);
}

void deactivateRelayB() {
  if (currentRelayB != -1) {
    uint8_t col, row;
    relayBtoCoord(currentRelayB, col, row);
    resetRelay(row, col, currentRelayB);
    currentRelayB = -1;
    setLED(-1, currentRelayA);
  }
}

void resetAllRelays() {
  for (uint8_t relay = 0; relay < 64; relay++) {
    uint8_t col, row;
    if (relay % 2 == 0)
      relayAtoCoord(relay, col, row);
    else
      relayBtoCoord(relay, col, row);

    resetRelay(row, col, relay);
  }

  currentRelayA = -1;
  currentRelayB = -1;
}

// ---------------- Sensor Helper ----------------
void get_env(sensors_event_t &temp, sensors_event_t &pressure, sensors_event_t &humidity) {
  // Adafruit_MS8607::getEvent signature: (pressure, temp, humidity)
  ms8607.getEvent(&pressure, &temp, &humidity);
}

// ---------------- SCPI Handler ----------------
class SCPI_handler : public SCPI_handler_interface {
public:
  void write(int address, const char *data, size_t len) override {
    (void)address; // accept any address
    if (!data || len == 0) return;

    char buf[96];
    size_t n = (len < sizeof(buf) - 1) ? len : (sizeof(buf) - 1);
    memcpy(buf, data, n);
    buf[n] = '\0';
    while (n && (buf[n - 1] == '\r' || buf[n - 1] == '\n' || buf[n - 1] == ' ' || buf[n - 1] == '\t')) {
      buf[--n] = '\0';
    }
    if (n == 0) return;

    if (!strcasecmp(buf, "*IDN?")) {
      setResponse("Kofotronic,EnvMux,1.0\r\n");
      return;
    }
    if (!strcasecmp(buf, "*OPC?")) {
      setResponse("1\r\n");
      return;
    }

    if (!strcasecmp(buf, "TEMP?")) {
      sensors_event_t t, p, h;
      get_env(t, p, h);
      setResponseDouble(t.temperature, 6);
      return;
    }
    if (!strcasecmp(buf, "HUMID?") || !strcasecmp(buf, "HUMIDITY?")) {
      sensors_event_t t, p, h;
      get_env(t, p, h);
      setResponseDouble(h.relative_humidity, 6);
      return;
    }
    if (!strcasecmp(buf, "PRESS?") || !strcasecmp(buf, "PRESSURE?")) {
      sensors_event_t t, p, h;
      get_env(t, p, h);
      setResponseDouble(p.pressure, 6);   // hPa from MS8607 lib
      return;
    }

    handleLocalSCPICommand(String(buf));
  }

  bool read(int address, char *data, size_t *len, size_t max_len) override {
    (void)address;
    if (!data || !len || max_len == 0) return false;

    if (respLen == 0) {
      *len = snprintf(data, max_len, "\r\n");
      return true;
    }
    size_t n = (respLen < max_len) ? respLen : (max_len - 1);
    memcpy(data, respBuf, n);
    data[n] = '\0';
    *len = n;

    respLen = 0;
    respBuf[0] = '\0';
    return true;
  }

  bool claim_control() override { return true; }
  void release_control() override {}

private:
  char   respBuf[96] = {0};
  size_t respLen     = 0;

  void setResponse(const char *s) {
    size_t n = strlcpy(respBuf, s, sizeof(respBuf));
    respLen = (n < sizeof(respBuf)) ? n : (sizeof(respBuf) - 1);
  }

  void setResponseDouble(double v, uint8_t prec) {
    char num[40];
    dtostrf(v, 0, prec, num);    
    size_t n = strlcpy(respBuf, num, sizeof(respBuf));
    if (n + 2 < sizeof(respBuf)) {
      respBuf[n++] = '\r';
      respBuf[n++] = '\n';
      respBuf[n]   = '\0';
    } else if (n + 1 < sizeof(respBuf)) {
      respBuf[n++] = '\n';
      respBuf[n]   = '\0';
    }
    respLen = strlen(respBuf);
  }

  void handleLocalSCPICommand(const String &cmd) {
    if (cmd.equalsIgnoreCase("ROUT:A:OFF")) {
      deactivateRelayA();
    } else if (cmd.equalsIgnoreCase("ROUT:B:OFF")) {
      deactivateRelayB();
    } else if (cmd.equalsIgnoreCase("ROUT:ALL:OFF")) {
      deactivateRelayA();
      deactivateRelayB();
    } else if (cmd.startsWith("ROUT:A:")) {
      String tail = cmd.substring(7); tail.trim();
      int spaceIdx = tail.indexOf(' ');
      if (spaceIdx > 0) {
        int relayNum = tail.substring(0, spaceIdx).toInt() - 1;
        String action = tail.substring(spaceIdx + 1); action.trim();
        if (action.equalsIgnoreCase("OFF") && relayNum == currentRelayA) {
          deactivateRelayA();
        } else if (action.equalsIgnoreCase("ON")) {
          if (relayNum >= 0 && relayNum < 16) activateRelayA(relayNum);
        }
      } else {
        int relayNum = tail.toInt() - 1;
        if (relayNum >= 0 && relayNum < 16) activateRelayA(relayNum);
      }
    } else if (cmd.startsWith("ROUT:B:")) {
      String tail = cmd.substring(7); tail.trim();
      int spaceIdx = tail.indexOf(' ');
      if (spaceIdx > 0) {
        int relayNum = tail.substring(0, spaceIdx).toInt() - 1;
        String action = tail.substring(spaceIdx + 1); action.trim();
        if (action.equalsIgnoreCase("OFF") && relayNum == currentRelayB) {
          deactivateRelayB();
        } else if (action.equalsIgnoreCase("ON")) {
          if (relayNum >= 0 && relayNum < 16) activateRelayB(relayNum);
        }
      } else {
        int relayNum = tail.toInt() - 1;
        if (relayNum >= 0 && relayNum < 16) activateRelayB(relayNum);
      }
    }
  }
};

#ifdef INTERFACE_VXI11
static SCPI_handler scpi_handler;
static VXI_Server vxi_server(scpi_handler);
static RPC_Bind_Server rpc_bind_server(vxi_server);
#endif

// ---------------- LP5521 Helpers (debug prints) ----------------
void lp5521_write(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(LP5521_ADDR);
  Wire.write(reg);
  Wire.write(val);
  byte err = Wire.endTransmission();
  Serial.print("Write 0x"); Serial.print(reg, HEX);
  Serial.print(" = 0x"); Serial.print(val, HEX);
  Serial.print(" | status: "); Serial.println(err == 0 ? "OK" : "ERR");
}

uint8_t lp5521_read(uint8_t reg) {
  Wire.beginTransmission(LP5521_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);  // Restart
  Wire.requestFrom(LP5521_ADDR, 1);
  if (Wire.available()) {
    uint8_t val = Wire.read();
    Serial.print("Read 0x"); Serial.print(reg, HEX);
    Serial.print(" = 0x"); Serial.println(val, HEX);
    return val;
  } else {
    Serial.println("Read failed");
    return 0xFF;
  }
}

// ---------------- Setup ----------------
void setup() {
  setup_serial_ui_and_led(F("Relay Controller"));

  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin,  OUTPUT);

  // Configure VPORTD/C for row/col latches
  VPORTD.DIR = 0xFF;  VPORTD.OUT = 0;
  VPORTC.DIR = 0xFF;  VPORTC.OUT = 0;

  resetAllRelays();
  setLED(-1, -1);

  Wire.begin();  // Ensure I2C is up before ms8607/EEPROM/LP5521 use

  eeprom.begin();
  uint8_t mac[6];
  eeprom.getMACAddress(mac);

  IPAddress ip;
  eeprom.getIPAddress(ip);
  Ethernet.init(7);
  if (ip == IPAddress(0, 0, 0, 0)) Ethernet.begin(mac);
  else Ethernet.begin(mac, ip);

  setup_ipaddress_surveillance_and_show_address();

#ifdef INTERFACE_VXI11
  vxi_server.begin(VXI11_PORT, false);
  rpc_bind_server.begin(false);
#endif

  ledDriver.begin();
  //ledDriver.setLedCurrent(0x3f, 0x3f, 0x3f);
  //ledDriver.loadHeartbeat(0);
  //ledDriver.loadHeartbeat(1);
  //ledDriver.loadHeartbeat(2);
  ledDriver.ledOn(0); // R
  //ledDriver.ledOn(1); // G
  //ledDriver.ledOn(2); // B

  // MS8607 init
  if (!ms8607.begin()) {
    Serial.println("Failed to find MS8607 chip");
    while (1) { delay(10); }
  }
  Serial.println("MS8607 Found!");
  ms8607.setHumidityResolution(MS8607_HUMIDITY_RESOLUTION_OSR_8b);

  end_of_setup();
}

// ---------------- Front Panel Switch Scan ----------------
const int NUM_ADCS = 8;
const int adcPins[NUM_ADCS] = { A8, A9, A10, A11, A12, A13, A14, A15 };
const int debounceDelay = 50;

int lastStableSwitch = -1;
int lastReadSwitch   = -1;
int lastRow          = -1; // 0 = B, 1 = A
int lastStableRow    = -1;
unsigned long lastDebounceTime = 0;

enum RelayState { RELAY_A_NONE, RELAY_B_NONE, RELAY_A, RELAY_B };
RelayState relayStatesA[64] = { RELAY_A_NONE };
RelayState relayStatesB[64] = { RELAY_B_NONE };

int getSwitchFromADC(int adcValue) {
  if      (adcValue <=  23) return 0;
  else if (adcValue <=  77) return 1;
  else if (adcValue <= 145) return 2;
  else if (adcValue <= 214) return 3;
  else if (adcValue <= 284) return 4;
  else if (adcValue <= 360) return 5;
  else if (adcValue <= 433) return 6;
  else if (adcValue <= 512) return 7;
  else if (adcValue <= 590) return 8;
  else if (adcValue <= 655) return 9;
  else if (adcValue <= 713) return 10;
  else if (adcValue <= 763) return 11;
  else if (adcValue <= 805) return 12;
  else if (adcValue <= 841) return 13;
  else if (adcValue <= 870) return 14;
  else if (adcValue <= 894) return 15;
  else return -1;
}

// ---------------- Main Loop ----------------
void loop() {
#ifdef INTERFACE_VXI11
  rpc_bind_server.loop();
  vxi_server.loop();
#endif

  int detectedSwitch = -1;
  int detectedRow    = -1;

  for (int i = 0; i < NUM_ADCS; i++) {
    int value = analogRead(adcPins[i]);
    int sw = getSwitchFromADC(value);
    if (sw != -1) {
      detectedSwitch = (i / 2) * 16 + sw;
      detectedRow    = (i % 2 == 0) ? 0 : 1;  // 0 = B, 1 = A
      break;
    }
  }

  if (detectedSwitch != lastReadSwitch || detectedRow != lastRow) {
    lastDebounceTime = millis();
    lastReadSwitch   = detectedSwitch;
    lastRow          = detectedRow;
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (lastStableSwitch != detectedSwitch || lastStableRow != detectedRow) {
      lastStableSwitch = detectedSwitch;
      lastStableRow    = detectedRow;

      if (detectedSwitch >= 0 && detectedSwitch < 64) {
        if (detectedRow == 1) {
          // A row
          if (relayStatesA[detectedSwitch] == RELAY_A_NONE) {
            activateRelayA(detectedSwitch);
            relayStatesA[detectedSwitch] = RELAY_A;
          } else {
            deactivateRelayA();
            relayStatesA[detectedSwitch] = RELAY_A_NONE;
          }
        } else if (detectedRow == 0) {
          // B row
          if (relayStatesB[detectedSwitch] == RELAY_B_NONE) {
            activateRelayB(detectedSwitch);
            relayStatesB[detectedSwitch] = RELAY_B;
          } else {
            deactivateRelayB();
            relayStatesB[detectedSwitch] = RELAY_B_NONE;
          }
        }
      }
    }
  }

  loop_serial_ui_and_led(0);
}
