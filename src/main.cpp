#include <Arduino.h>

#include "app_state.h"
#include "buttons.h"
#include "gps.h"
#include "storage.h"
#include "timezone.h"
#include "touch_input.h"
#include "ui.h"
#include <string.h>

static void currentDailyDateKey(char* out, size_t outLen) {
  if (timezoneLocalDateKey(out, outLen)) {
    return;
  }

  strlcpy(out, "NO_DATE", outLen);
}

static void updateDailyTruckTime() {
  static char loadedDateKey[16] = "";
  static bool dailyLoaded = false;
  static unsigned long lastSaveMs = 0;

  char dateKey[16];
  currentDailyDateKey(dateKey, sizeof(dateKey));

  if (!dailyLoaded || strcmp(dateKey, loadedDateKey) != 0) {
    app.truckTodaySavedSeconds = storageLoadDailySeconds(dateKey);
    strlcpy(loadedDateKey, dateKey, sizeof(loadedDateKey));
    dailyLoaded = true;
    lastSaveMs = millis();

    Serial.print("[DAILY] loaded ");
    Serial.print(loadedDateKey);
    Serial.print(" seconds=");
    Serial.println(app.truckTodaySavedSeconds);
  }

  appUpdateTruckTodayActiveSeconds();

  if (storageIsReady() && millis() - lastSaveMs >= 60000) {
    lastSaveMs = millis();
    if (storageSaveDailySeconds(loadedDateKey, app.truckTodayActiveSeconds)) {
      Serial.print("[DAILY] saved ");
      Serial.print(loadedDateKey);
      Serial.print(" seconds=");
      Serial.println(app.truckTodayActiveSeconds);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  gpsInit();
  appStateInit();
  storageInit();
  timezoneInit();
  buttonsInit();
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
  buttonsPoll();
  updateDailyTruckTime();
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
