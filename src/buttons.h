#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>

void buttonsInit();
void buttonsPoll();
bool buttonsAvailable();
uint8_t buttonsRawState();
bool buttonIsPressed(uint8_t index);
const char* buttonName(uint8_t index);
const char* buttonsStatusText();
uint8_t buttonsAddress();
int buttonsSdaPin();
int buttonsSclPin();

#endif
