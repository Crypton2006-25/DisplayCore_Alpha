#include "buttons.h"

#include <Wire.h>

struct I2cCandidate {
  int sda;
  int scl;
};

static const I2cCandidate I2C_CANDIDATES[] = {
  {21, 22},
  {33, 32},
  {25, 26},
  {27, 22},
  {16, 17}
};

static const char* BUTTON_NAMES[] = {
  "START",
  "MARK",
  "RETURN",
  "BACK",
  "PAGE",
  "SELECT",
  "SPARE",
  "SPARE"
};

static bool buttonsReady = false;
static uint8_t detectedAddress = 0;
static int detectedSda = -1;
static int detectedScl = -1;
static uint8_t rawState = 0xFF;

static bool usesUnsafePin(const I2cCandidate& candidate) {
  return candidate.sda == 33 || candidate.scl == 33 ||
         candidate.sda == 27 || candidate.scl == 27;
}

static bool probeAddress(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

static bool writeInputsHigh(uint8_t address) {
  Wire.beginTransmission(address);
  Wire.write(0xFF);
  return Wire.endTransmission() == 0;
}

static bool probeAddressRange(uint8_t firstAddress, uint8_t lastAddress, const I2cCandidate& candidate) {
  for (uint8_t address = firstAddress; address <= lastAddress; address++) {
    if (!probeAddress(address)) {
      continue;
    }

    if (!writeInputsHigh(address)) {
      continue;
    }

    buttonsReady = true;
    detectedAddress = address;
    detectedSda = candidate.sda;
    detectedScl = candidate.scl;

    Serial.print("[BTN] PCF8574 OK addr7=0x");
    if (detectedAddress < 0x10) {
      Serial.print('0');
    }
    Serial.print(detectedAddress, HEX);
    Serial.print(" addr8w=0x");
    Serial.print(detectedAddress << 1, HEX);
    Serial.print(" addr8r=0x");
    Serial.print((detectedAddress << 1) | 1, HEX);
    Serial.print(" sda=");
    Serial.print(detectedSda);
    Serial.print(" scl=");
    Serial.println(detectedScl);

    buttonsPoll();
    return true;
  }

  return false;
}

void buttonsInit() {
  buttonsReady = false;
  detectedAddress = 0;
  detectedSda = -1;
  detectedScl = -1;
  rawState = 0xFF;

  for (size_t i = 0; i < sizeof(I2C_CANDIDATES) / sizeof(I2C_CANDIDATES[0]); i++) {
    const I2cCandidate& candidate = I2C_CANDIDATES[i];

    if (usesUnsafePin(candidate)) {
      Serial.print("[BTN] skipping SDA=");
      Serial.print(candidate.sda);
      Serial.print(" SCL=");
      Serial.print(candidate.scl);
      Serial.println(" used pin");
      continue;
    }

    Serial.print("[BTN] trying SDA=");
    Serial.print(candidate.sda);
    Serial.print(" SCL=");
    Serial.println(candidate.scl);

    Wire.end();
    pinMode(candidate.sda, INPUT_PULLUP);
    pinMode(candidate.scl, INPUT_PULLUP);
    delay(2);

    Serial.print("[BTN] idle SDA=");
    Serial.print(digitalRead(candidate.sda));
    Serial.print(" SCL=");
    Serial.println(digitalRead(candidate.scl));

    Wire.begin(candidate.sda, candidate.scl);
    Wire.setClock(100000);
    delay(10);

    if (probeAddressRange(0x20, 0x27, candidate)) {
      return;
    }

    Serial.print("[BTN] no PCF on SDA=");
    Serial.print(candidate.sda);
    Serial.print(" SCL=");
    Serial.println(candidate.scl);
  }

  Wire.end();
  Serial.println("[BTN] PCF8574 ERR");
}

void buttonsPoll() {
  if (!buttonsReady) {
    return;
  }

  Wire.requestFrom((int)detectedAddress, 1);
  if (Wire.available() > 0) {
    rawState = (uint8_t)Wire.read();
  }
}

bool buttonsAvailable() {
  return buttonsReady;
}

uint8_t buttonsRawState() {
  return rawState;
}

bool buttonIsPressed(uint8_t index) {
  if (index >= 8) {
    return false;
  }

  return (rawState & (1 << index)) == 0;
}

const char* buttonName(uint8_t index) {
  if (index >= 8) {
    return "UNKNOWN";
  }

  return BUTTON_NAMES[index];
}

const char* buttonsStatusText() {
  return buttonsReady ? "OK" : "ERR";
}

uint8_t buttonsAddress() {
  return detectedAddress;
}

int buttonsSdaPin() {
  return detectedSda;
}

int buttonsSclPin() {
  return detectedScl;
}
