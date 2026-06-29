#include "storage.h"

#include <SD.h>
#include <SPI.h>
#include <string.h>

#ifndef TFT_CS
#define TFT_CS 15
#endif

#ifndef TOUCH_CS
#define TOUCH_CS 33
#endif

#define SD_CS 5
#define SD_SCK 18
#define SD_MISO 19
#define SD_MOSI 23

static SPIClass sdSPI(HSPI);
static bool storageReady = false;
static bool storageAttempted = false;

static bool dailyPathForDate(const char* dateKey, char* path, size_t pathLen) {
  if (dateKey == nullptr || dateKey[0] == '\0') {
    return false;
  }

  int written = snprintf(path, pathLen, "/daily/%s.txt", dateKey);
  return written > 0 && (size_t)written < pathLen;
}

static bool ensureDailyDir() {
  if (!storageReady) {
    return false;
  }

  if (SD.exists("/daily")) {
    return true;
  }

  return SD.mkdir("/daily");
}

void storageInit() {
  storageAttempted = true;
  storageReady = false;

  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  delay(10);

  Serial.println("[SD] init vendor pins cs=5 sck=18 miso=19 mosi=23");

  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  storageReady = SD.begin(SD_CS, sdSPI);
  if (storageReady) {
    Serial.print("[SD] card type=");
    Serial.print(SD.cardType());
    Serial.print(" sizeMB=");
    Serial.println((unsigned long)(SD.cardSize() / (1024ULL * 1024ULL)));
    Serial.println("[SD] OK");
  } else {
    Serial.println("[SD] ERR");
  }
}

bool storageIsReady() {
  return storageReady;
}

String storageStatusText() {
  if (storageReady) {
    return "SD OK";
  }

  if (storageAttempted) {
    return "SD ERR";
  }

  return "SD OFF";
}

uint32_t storageLoadDailySeconds(const char* dateKey) {
  if (!storageReady || !ensureDailyDir()) {
    return 0;
  }

  char path[32];
  if (!dailyPathForDate(dateKey, path, sizeof(path)) || !SD.exists(path)) {
    return 0;
  }

  File file = SD.open(path, FILE_READ);
  if (!file) {
    return 0;
  }

  uint32_t seconds = 0;
  while (file.available() > 0) {
    char c = (char)file.read();
    if (c < '0' || c > '9') {
      break;
    }

    uint32_t next = (seconds * 10) + (uint32_t)(c - '0');
    if (next < seconds) {
      break;
    }
    seconds = next;
  }

  file.close();
  return seconds;
}

bool storageSaveDailySeconds(const char* dateKey, uint32_t seconds) {
  if (!storageReady || !ensureDailyDir()) {
    return false;
  }

  char path[32];
  if (!dailyPathForDate(dateKey, path, sizeof(path))) {
    return false;
  }

  if (SD.exists(path)) {
    SD.remove(path);
  }

  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    return false;
  }

  char line[16];
  snprintf(line, sizeof(line), "%lu\n", (unsigned long)seconds);
  size_t written = file.print(line);
  file.close();

  return written > 0;
}
