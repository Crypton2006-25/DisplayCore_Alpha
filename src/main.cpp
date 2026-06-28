#include <Arduino.h>

#include "app_state.h"
#include "gps.h"
#include "touch_input.h"
#include "ui.h"

void setup() {
  Serial.begin(115200);
  delay(500);

  gpsInit();
  appStateInit();
  uiInit();
  touchInit();

  goToPage(PAGE_HOME);

  Serial.println("DisplayCore Alpha tactical topo UI ready");
  Serial.print("GPS UART listening on RX=");
  Serial.print(gpsActiveRxPin());
  Serial.print(" TX=");
  Serial.print(gpsActiveTxPin());
  Serial.print(" baud=");
  Serial.println(gpsBaud());
}

void loop() {
  gpsPoll();
  gpsPollPinScanner();
  updateGpsStatusUi();
  gpsReportDiagnostics();

  static unsigned long lastMockUpdate = 0;
  if (millis() - lastMockUpdate > 1000) {
    lastMockUpdate = millis();

    if (app.trailRecording && !app.trackbackMode) {
      app.mockTripMiles += 0.01f;
      app.mockFromWP += 0.01f;
      app.mockReturn = app.mockTripMiles;
      app.mockHeading += 1.2f;

      if (app.mockHeading >= 360.0f) {
        app.mockHeading -= 360.0f;
      }

      if (app.currentPage == PAGE_TRAIL) {
        updateTrailDynamicFields();
      } else {
        drawFooter();
      }
    } else {
      drawFooter();
    }
  }

  int touchX = 0;
  int touchY = 0;
  if (readTouchPress(touchX, touchY)) {
    handleTouch(touchX, touchY);

    delay(120);
  }

  delay(20);
}
