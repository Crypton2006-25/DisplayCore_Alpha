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

int gpsActiveRxPin();
int gpsActiveTxPin();
int gpsBaud();

#endif
