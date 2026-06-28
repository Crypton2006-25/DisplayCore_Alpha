#include "app_state.h"

AppState app;

void appStateInit() {
  app.currentPage = PAGE_HOME;
  app.trailRecording = false;
  app.trackbackMode = false;
  app.waypointCount = 0;
  app.selectedBT = 1;
  app.bootTime = millis();
  app.mockTripMiles = 2.47f;
  app.mockFromWP = 0.38f;
  app.mockReturn = 2.44f;
  app.mockHeading = 274.0f;
}
