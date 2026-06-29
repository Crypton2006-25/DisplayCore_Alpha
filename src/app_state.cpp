#include "app_state.h"

AppState app;

void appStateInit() {
  app.currentPage = PAGE_HOME;
  app.trailRecording = false;
  app.trackbackMode = false;
  app.waypointCount = 0;
  app.selectedBT = 1;
  app.bootTime = millis();
  app.truckTodaySavedSeconds = 0;
  app.truckTodayActiveSeconds = 0;
  app.mockTripMiles = 2.47f;
  app.mockFromWP = 0.38f;
  app.mockReturn = 2.44f;
  app.mockHeading = 274.0f;
}

unsigned long appTripSeconds() {
  return (millis() - app.bootTime) / 1000;
}

unsigned long appUpdateTruckTodayActiveSeconds() {
  app.truckTodayActiveSeconds = app.truckTodaySavedSeconds + (uint32_t)appTripSeconds();
  return app.truckTodayActiveSeconds;
}
