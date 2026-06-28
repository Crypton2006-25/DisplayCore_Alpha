#ifndef APP_STATE_H
#define APP_STATE_H

#include <Arduino.h>

enum Page {
  PAGE_HOME,
  PAGE_TRAIL,
  PAGE_TRUCK,
  PAGE_BT,
  PAGE_SETTINGS
};

struct AppState {
  Page currentPage;
  bool trailRecording;
  bool trackbackMode;
  int waypointCount;
  int selectedBT;
  unsigned long bootTime;
  float mockTripMiles;
  float mockFromWP;
  float mockReturn;
  float mockHeading;
};

extern AppState app;

void appStateInit();

#endif
