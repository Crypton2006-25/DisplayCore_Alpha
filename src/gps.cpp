#include "gps.h"

#include <string.h>

#define TFT_BACKLIGHT 27

#ifndef GPS_RX_PIN
#define GPS_RX_PIN 35
#endif

#ifndef GPS_TX_PIN
#define GPS_TX_PIN -1
#endif

#ifndef GPS_BAUD
#define GPS_BAUD 9600
#endif

#ifndef GPS_PIN_SCAN
#define GPS_PIN_SCAN 0
#endif

#ifndef GPS_UART0_SNIFF
#define GPS_UART0_SNIFF 0
#endif

static char gpsLine[128];
static uint8_t gpsLineLen = 0;
static unsigned long gpsCharCount = 0;
static unsigned long gpsSentenceCount = 0;
static unsigned long gpsLastByteMs = 0;
static unsigned long gpsLastSentenceMs = 0;
static unsigned long gpsLastReportMs = 0;
static unsigned long gpsScanWindowStartMs = 0;
static unsigned long gpsScanWindowChars = 0;
static unsigned long gpsScanWindowSentences = 0;
static bool gpsHasFix = false;
static bool gpsScanLocked = false;
static int gpsSatellites = -1;
static bool gpsCoordsValid = false;
static double gpsLatitude = 0.0;
static double gpsLongitude = 0.0;
static bool gpsUtcDateTimeValid = false;
static char gpsUtcTime[9] = "--:--:--";
static char gpsUtcDate[11] = "0000-00-00";

static const int GPS_RX_CANDIDATES[] = {
  GPS_RX_PIN, 16, 17, 4, 5, 18, 19, 21, 22, 23, 25, 26, 27, 32, 34, 35, 39
};

static uint8_t gpsRxCandidateIndex = 0;

int gpsActiveRxPin() {
#if GPS_PIN_SCAN
  return GPS_RX_CANDIDATES[gpsRxCandidateIndex];
#else
  return GPS_RX_PIN;
#endif
}

int gpsActiveTxPin() {
#if GPS_PIN_SCAN
  return -1;
#else
  return GPS_TX_PIN;
#endif
}

int gpsBaud() {
  return GPS_BAUD;
}

void gpsInit() {
  if (gpsActiveRxPin() == TFT_BACKLIGHT) {
    pinMode(TFT_BACKLIGHT, INPUT);
  } else {
    pinMode(TFT_BACKLIGHT, OUTPUT);
    digitalWrite(TFT_BACKLIGHT, HIGH);
  }

  Serial2.begin(GPS_BAUD, SERIAL_8N1, gpsActiveRxPin(), gpsActiveTxPin());
  gpsScanWindowStartMs = millis();
  gpsScanWindowChars = 0;
  gpsScanWindowSentences = 0;
}

String gpsStatusText() {
  unsigned long now = millis();

#if GPS_PIN_SCAN
  if (!gpsScanLocked) {
    return "SCAN";
  }
#endif

  if (gpsCharCount == 0) {
    return "NO RX";
  }

  if (now - gpsLastByteMs > 3000) {
    return "LOST";
  }

  if (gpsHasFix) {
    return "LOCK";
  }

  return "RX";
}

bool gpsCoordsAreValid() {
  return gpsCoordsValid;
}

String gpsCoordText() {
  if (!gpsCoordsValid) {
    return "GPS COORDS: WAITING FOR NMEA";
  }

  return "GPS: " + String(gpsLatitude, 6) + ", " + String(gpsLongitude, 6);
}

bool gpsDateTimeValid() {
  return gpsUtcDateTimeValid;
}

String gpsUtcDateText() {
  return String(gpsUtcDate);
}

String gpsUtcTimeText() {
  return String(gpsUtcTime);
}

const char* gpsUtcDateCStr() {
  return gpsUtcDate;
}

const char* gpsUtcTimeCStr() {
  return gpsUtcTime;
}

static String nmeaField(const char* line, int wantedField) {
  int field = 0;
  const char* start = line;

  for (const char* p = line; ; p++) {
    if (*p == ',' || *p == '*' || *p == '\0') {
      if (field == wantedField) {
        return String(start).substring(0, p - start);
      }

      if (*p == '\0' || *p == '*') {
        break;
      }

      field++;
      start = p + 1;
    }
  }

  return "";
}

static bool nmeaTypeIs(const char* line, const char* type) {
  return line[0] == '$' && strlen(line) >= 6 && strncmp(line + 3, type, 3) == 0;
}

static double nmeaCoordToDecimal(const String& raw, const String& hemisphere) {
  if (raw.length() < 4 || hemisphere.length() == 0) {
    return 0.0;
  }

  double value = raw.toDouble();
  int degrees = (int)(value / 100.0);
  double minutes = value - (degrees * 100.0);
  double decimal = degrees + (minutes / 60.0);

  if (hemisphere[0] == 'S' || hemisphere[0] == 'W') {
    decimal *= -1.0;
  }

  return decimal;
}

static void updateGpsCoords(const String& lat, const String& ns, const String& lon, const String& ew) {
  if (lat.length() == 0 || ns.length() == 0 || lon.length() == 0 || ew.length() == 0) {
    return;
  }

  gpsLatitude = nmeaCoordToDecimal(lat, ns);
  gpsLongitude = nmeaCoordToDecimal(lon, ew);
  gpsCoordsValid = true;
}

static void updateGpsDateTime(const String& utcTimeRaw, const String& utcDateRaw) {
  if (utcTimeRaw.length() < 6 || utcDateRaw.length() != 6) {
    return;
  }

  int day = utcDateRaw.substring(0, 2).toInt();
  int month = utcDateRaw.substring(2, 4).toInt();
  int year = 2000 + utcDateRaw.substring(4, 6).toInt();
  int hour = utcTimeRaw.substring(0, 2).toInt();
  int minute = utcTimeRaw.substring(2, 4).toInt();
  int second = utcTimeRaw.substring(4, 6).toInt();

  if (day < 1 || day > 31 || month < 1 || month > 12 || hour > 23 || minute > 59 || second > 59) {
    return;
  }

  snprintf(gpsUtcDate, sizeof(gpsUtcDate), "%04d-%02d-%02d", year, month, day);
  snprintf(gpsUtcTime, sizeof(gpsUtcTime), "%02d:%02d:%02d", hour, minute, second);
  gpsUtcDateTimeValid = true;
}

static void handleGpsSentence(const char* line) {
  gpsSentenceCount++;
  gpsScanWindowSentences++;
  gpsLastSentenceMs = millis();

  Serial.print("[GPS] ");
  Serial.println(line);

  if (nmeaTypeIs(line, "GGA")) {
    int fixQuality = nmeaField(line, 6).toInt();
    gpsSatellites = nmeaField(line, 7).toInt();
    gpsHasFix = fixQuality > 0;
    updateGpsCoords(nmeaField(line, 2), nmeaField(line, 3), nmeaField(line, 4), nmeaField(line, 5));

    Serial.print("[GPS STATUS] fix=");
    Serial.print(gpsHasFix ? "YES" : "NO");
    Serial.print(" sats=");
    Serial.print(gpsSatellites);
    Serial.print(" sentences=");
    Serial.println(gpsSentenceCount);
  } else if (nmeaTypeIs(line, "RMC")) {
    String validity = nmeaField(line, 2);
    if (validity.length() > 0) {
      gpsHasFix = validity[0] == 'A';
    }
    updateGpsCoords(nmeaField(line, 3), nmeaField(line, 4), nmeaField(line, 5), nmeaField(line, 6));
    updateGpsDateTime(nmeaField(line, 1), nmeaField(line, 9));
  }
}

static void processGpsChar(char c) {
  gpsCharCount++;
  gpsScanWindowChars++;
  gpsLastByteMs = millis();

  if (c == '\r') {
    return;
  }

  if (c == '\n') {
    if (gpsLineLen > 0) {
      gpsLine[gpsLineLen] = '\0';
      handleGpsSentence(gpsLine);
      gpsLineLen = 0;
    }
    return;
  }

  if (gpsLineLen < sizeof(gpsLine) - 1) {
    gpsLine[gpsLineLen++] = c;
  } else {
    gpsLineLen = 0;
  }
}

void gpsPoll() {
  while (Serial2.available() > 0) {
    processGpsChar((char)Serial2.read());
  }

#if GPS_UART0_SNIFF
  while (Serial.available() > 0) {
    processGpsChar((char)Serial.read());
  }
#endif
}

void gpsPollPinScanner() {
#if GPS_PIN_SCAN
  if (gpsScanLocked) {
    return;
  }

  if (gpsScanWindowChars >= 16 || gpsScanWindowSentences > 0) {
    gpsScanLocked = true;
    Serial.print("[GPS SCAN] activity found, holding RX=");
    Serial.print(gpsActiveRxPin());
    Serial.print(" chars=");
    Serial.print(gpsScanWindowChars);
    Serial.print(" sentences=");
    Serial.println(gpsScanWindowSentences);
    return;
  }

  if (millis() - gpsScanWindowStartMs < 3000) {
    return;
  }

  Serial.print("[GPS SCAN] tested rx=");
  Serial.print(gpsActiveRxPin());
  Serial.print(" chars=");
  Serial.print(gpsScanWindowChars);
  Serial.print(" sentences=");
  Serial.println(gpsScanWindowSentences);

  gpsRxCandidateIndex++;
  if (gpsRxCandidateIndex >= (sizeof(GPS_RX_CANDIDATES) / sizeof(GPS_RX_CANDIDATES[0]))) {
    gpsRxCandidateIndex = 0;
  }

  Serial2.end();
  delay(10);
  gpsInit();

  Serial.print("[GPS SCAN] listening rx=");
  Serial.print(gpsActiveRxPin());
  Serial.print(" baud=");
  Serial.println(GPS_BAUD);
#endif
}

void gpsReportDiagnostics() {
  if (millis() - gpsLastReportMs < 5000) {
    return;
  }

  gpsLastReportMs = millis();

  Serial.print("[GPS WAIT] status=");
  Serial.print(gpsStatusText());
  Serial.print(" chars=");
  Serial.print(gpsCharCount);
  Serial.print(" sentences=");
  Serial.print(gpsSentenceCount);
  Serial.print(" sats=");
  Serial.print(gpsSatellites);
  Serial.print(" rx=");
  Serial.print(gpsActiveRxPin());
#if GPS_UART0_SNIFF
  Serial.print(" +uart0rx");
#endif
  Serial.print(" baud=");
  Serial.println(GPS_BAUD);
}
