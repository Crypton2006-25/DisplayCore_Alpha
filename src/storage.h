#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>

void storageInit();
bool storageIsReady();
String storageStatusText();
uint32_t storageLoadDailySeconds(const char* dateKey);
bool storageSaveDailySeconds(const char* dateKey, uint32_t seconds);

#endif
