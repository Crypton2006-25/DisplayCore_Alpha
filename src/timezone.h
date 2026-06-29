#ifndef TIMEZONE_H
#define TIMEZONE_H

#include <Arduino.h>

enum TimezoneMode {
  TZ_AZ,
  TZ_PACIFIC,
  TZ_MOUNTAIN,
  TZ_CENTRAL,
  TZ_EASTERN,
  TZ_UTC
};

void timezoneInit();
void timezoneCycleAndSave();
const char* timezoneName();
const char* timezoneLabel();
bool timezoneFormatLocalDateTime(char* out, size_t outLen);
bool timezoneLocalDateKey(char* out, size_t outLen);

#endif
