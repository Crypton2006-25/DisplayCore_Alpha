#ifndef GPS_H
#define GPS_H

#include <Arduino.h>

void gpsInit();
void gpsPoll();
void gpsPollPinScanner();
void gpsReportDiagnostics();

String gpsStatusText();
bool gpsCoordsAreValid();
String gpsCoordText();
bool gpsDateTimeValid();
String gpsUtcDateText();
String gpsUtcTimeText();
const char* gpsUtcDateCStr();
const char* gpsUtcTimeCStr();

int gpsActiveRxPin();
int gpsActiveTxPin();
int gpsBaud();

#endif
