#include "ui.h"

#include <Arduino.h>
#include <SPI.h>

#define TOUCH_CS 33

#include <TFT_eSPI.h>

#include "gps.h"

#define TFT_BACKLIGHT 27

static TFT_eSPI tft = TFT_eSPI();

const int SCREEN_W = 480;
const int SCREEN_H = 320;

static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

const uint16_t C_BG          = 0x0000;
const uint16_t C_PANEL       = rgb(24, 34, 24);
const uint16_t C_PANEL_2     = rgb(30, 42, 30);
const uint16_t C_BORDER      = rgb(150, 130, 55);
const uint16_t C_TEXT        = rgb(150, 220, 120);
const uint16_t C_TEXT_DIM    = rgb(95, 130, 85);
const uint16_t C_ACCENT      = rgb(255, 190, 70);
const uint16_t C_ACCENT_DIM  = rgb(130, 95, 35);
const uint16_t C_ALERT       = rgb(255, 90, 60);
const uint16_t C_GOOD        = rgb(90, 220, 120);
const uint16_t C_OFF         = rgb(80, 80, 80);
const uint16_t C_WHITE_SOFT  = rgb(210, 220, 210);
const uint16_t C_TOPO        = rgb(28, 58, 36);
const uint16_t C_TOPO_BOLD   = rgb(42, 86, 52);

struct Button {
  int x;
  int y;
  int w;
  int h;
  const char* label;
};

static Button homeTrail    = {20, 85, 210, 78, "TRAIL"};
static Button homeTruck    = {250, 85, 210, 78, "TRUCK"};
static Button homeBT       = {20, 180, 210, 78, "BT HUB"};
static Button homeSettings = {250, 180, 210, 78, "SETTINGS"};

static Button backButton   = {12, 12, 70, 28, "BACK"};

static Button trailStart   = {18, 248, 138, 52, "START"};
static Button trailMark    = {171, 248, 138, 52, "MARK"};
static Button trailReturn  = {324, 248, 138, 52, "RETURN"};

static Button bt1          = {18, 110, 105, 60, "DEV 1"};
static Button bt2          = {132, 110, 105, 60, "DEV 2"};
static Button bt3          = {246, 110, 105, 60, "DEV 3"};
static Button bt4          = {360, 110, 105, 60, "DEV 4"};

static unsigned long gpsLastUiMs = 0;

void uiInit() {
  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, HIGH);

  tft.init();
  tft.setRotation(1);

  SPI.begin(14, 12, 13);
}

static bool hit(Button b, int x, int y) {
  return x >= b.x && x <= (b.x + b.w) && y >= b.y && y <= (b.y + b.h);
}

static bool hitBackButton(int x, int y) {
  return x >= 0 && x <= 90 && y >= 0 && y <= 56;
}

static uint16_t gpsStatusColor() {
  String status = gpsStatusText();

  if (status == "LOCK") {
    return C_GOOD;
  }

  if (status == "RX") {
    return C_ACCENT;
  }

  return C_ALERT;
}

static void drawTopoLoop(int cx, int cy, int rx, int ry, int rings, uint16_t color) {
  for (int r = 0; r < rings; r++) {
    int shrinkX = r * 9;
    int shrinkY = r * 6;

    int prevX = 0;
    int prevY = 0;
    bool first = true;

    for (int deg = 0; deg <= 360; deg += 12) {
      float a = deg * 0.0174532925f;

      float wobble =
        1.0f +
        0.10f * sin(a * 3.0f + r) +
        0.06f * sin(a * 7.0f + cx * 0.02f);

      int x = cx + cos(a) * (rx - shrinkX) * wobble;
      int y = cy + sin(a) * (ry - shrinkY) * wobble;

      if (!first) {
        tft.drawLine(prevX, prevY, x, y, color);
      }

      prevX = x;
      prevY = y;
      first = false;
    }
  }
}

static void drawTopoBackground() {
  for (int band = 0; band < 9; band++) {
    int baseY = 38 + band * 34;

    int prevX = 0;
    int prevY = baseY;

    for (int x = 0; x <= SCREEN_W; x += 10) {
      int y =
        baseY +
        10 * sin((x * 0.035f) + band * 1.7f) +
        6  * sin((x * 0.013f) + band * 3.1f);

      tft.drawLine(prevX, prevY, x, y, C_TOPO);

      prevX = x;
      prevY = y;
    }
  }

  drawTopoLoop(88, 120, 78, 48, 5, C_TOPO);
  drawTopoLoop(365, 132, 96, 58, 6, C_TOPO);
  drawTopoLoop(240, 255, 120, 48, 5, C_TOPO);

  drawTopoLoop(88, 120, 52, 32, 2, C_TOPO_BOLD);
  drawTopoLoop(365, 132, 64, 38, 2, C_TOPO_BOLD);
}

static void drawOuterFrame() {
  tft.drawRect(2, 2, SCREEN_W - 4, SCREEN_H - 4, C_BORDER);
  tft.drawRect(4, 4, SCREEN_W - 8, SCREEN_H - 8, C_ACCENT_DIM);
}

static void drawHeader(const char* title, const char* subtitle, bool showText) {
  tft.fillRect(0, 0, SCREEN_W, 50, C_BG);
  tft.drawFastHLine(0, 50, SCREEN_W, C_BORDER);

  if (!showText) {
    return;
  }

  tft.setTextColor(C_ACCENT, C_BG);
  tft.setTextSize(2);
  tft.setCursor(16, 10);
  tft.print(title);

  tft.setTextColor(C_TEXT_DIM, C_BG);
  tft.setTextSize(1);
  tft.setCursor(18, 34);
  tft.print(subtitle);
}

static void drawPageLabel(const char* title, const char* subtitle) {
  tft.fillRect(8, 52, SCREEN_W - 16, 10, C_BG);

  tft.setTextSize(1);
  tft.setTextColor(C_ACCENT, C_BG);
  tft.setCursor(18, 54);
  tft.print(title);

  tft.setTextColor(C_TEXT_DIM, C_BG);
  tft.print(" / ");
  tft.print(subtitle);
}

static void drawStatusChip(int x, int y, int w, const String& label, const String& value, uint16_t valueColor) {
  tft.fillRect(x, y, w, 22, C_PANEL);
  tft.drawRect(x, y, w, 22, C_ACCENT_DIM);

  tft.setTextColor(C_TEXT_DIM, C_PANEL);
  tft.setTextSize(1);
  tft.setCursor(x + 6, y + 6);
  tft.print(label);

  tft.setTextColor(valueColor, C_PANEL);
  tft.setCursor(x + w - 40, y + 6);
  tft.print(value);
}

void drawFooter() {
  unsigned long uptime = (millis() - app.bootTime) / 1000;

  tft.fillRect(0, 302, SCREEN_W, 18, C_BG);
  tft.drawFastHLine(0, 301, SCREEN_W, C_ACCENT_DIM);

  tft.setTextColor(C_TEXT_DIM, C_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 307);
  tft.print("SYS:NOMINAL  UPTIME:");
  tft.print(uptime);
  tft.print("s");
}

static void drawPanel(int x, int y, int w, int h, const String& title) {
  tft.fillRect(x, y, w, h, C_PANEL);
  tft.drawRect(x, y, w, h, C_BORDER);

  tft.drawFastHLine(x + 2, y + 2, 14, C_ACCENT);
  tft.drawFastVLine(x + 2, y + 2, 14, C_ACCENT);
  tft.drawFastHLine(x + w - 16, y + 2, 14, C_ACCENT);
  tft.drawFastVLine(x + w - 3, y + 2, 14, C_ACCENT);

  tft.drawFastHLine(x + 2, y + h - 3, 14, C_ACCENT_DIM);
  tft.drawFastVLine(x + 2, y + h - 16, 14, C_ACCENT_DIM);
  tft.drawFastHLine(x + w - 16, y + h - 3, 14, C_ACCENT_DIM);
  tft.drawFastVLine(x + w - 3, y + h - 16, 14, C_ACCENT_DIM);

  tft.setTextColor(C_ACCENT, C_PANEL);
  tft.setTextSize(1);
  tft.setCursor(x + 8, y + 6);
  tft.print(title);
}

static void drawButton(const Button& b, const String& label, bool selected = false, bool alert = false) {
  uint16_t fill   = selected ? C_PANEL_2 : C_PANEL;
  uint16_t border = selected ? C_ACCENT : C_ACCENT_DIM;
  uint16_t text   = selected ? C_WHITE_SOFT : C_TEXT;

  if (alert) {
    border = C_ALERT;
    text = C_ALERT;
  }

  tft.fillRect(b.x, b.y, b.w, b.h, fill);
  tft.drawRect(b.x, b.y, b.w, b.h, border);

  tft.drawFastHLine(b.x + 2, b.y + 2, 10, border);
  tft.drawFastVLine(b.x + 2, b.y + 2, 10, border);

  tft.drawFastHLine(b.x + b.w - 12, b.y + 2, 10, border);
  tft.drawFastVLine(b.x + b.w - 3, b.y + 2, 10, border);

  tft.drawFastHLine(b.x + 2, b.y + b.h - 3, 10, border);
  tft.drawFastVLine(b.x + 2, b.y + b.h - 12, 10, border);

  tft.drawFastHLine(b.x + b.w - 12, b.y + b.h - 3, 10, border);
  tft.drawFastVLine(b.x + b.w - 3, b.y + b.h - 12, 10, border);

  tft.setTextColor(text, fill);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(label, b.x + b.w / 2, b.y + b.h / 2);
  tft.setTextDatum(TL_DATUM);
}

static void drawValue(int x, int y, const String& label, const String& value, uint16_t valueColor = C_TEXT) {
  tft.setTextColor(C_TEXT_DIM, C_PANEL);
  tft.setTextSize(1);
  tft.setCursor(x, y);
  tft.print(label);

  tft.setTextColor(valueColor, C_PANEL);
  tft.setTextSize(2);
  tft.setCursor(x, y + 12);
  tft.print(value);
}

static void drawTrailGpsNoteLine() {
  tft.fillRect(30, 212, 410, 12, C_PANEL);
  tft.setTextColor(gpsCoordsAreValid() ? C_GOOD : C_ACCENT, C_PANEL);
  tft.setTextSize(1);
  tft.setCursor(30, 214);
  tft.print("> ");
  tft.print(gpsCoordText());
}

static void beginPage(const char* title, const char* subtitle, bool showHeaderText = true) {
  tft.fillScreen(C_BG);
  drawTopoBackground();
  drawOuterFrame();
  drawHeader(title, subtitle, showHeaderText);
  drawFooter();
}

static void drawHome() {
  app.currentPage = PAGE_HOME;
  beginPage("DISPLAYCORE ALPHA", "TACTICAL VEHICLE INTERFACE");

  drawStatusChip(18, 58, 110, "GPS", gpsStatusText(), gpsStatusColor());
  drawStatusChip(136, 58, 110, "IMU", "OFF", C_OFF);
  drawStatusChip(254, 58, 110, "CAN", "OFF", C_OFF);
  drawStatusChip(372, 58, 90, "PWR", "USB", C_GOOD);

  drawButton(homeTrail, "TRAIL", false);
  drawButton(homeTruck, "TRUCK", false);
  drawButton(homeBT, "BT HUB", false);
  drawButton(homeSettings, "SETTINGS", false);

  tft.setTextColor(C_TEXT_DIM, C_BG);
  tft.setTextSize(1);
  tft.setCursor(20, 270);
  tft.print("MISSION APPS: NAVIGATION / VEHICLE / AUDIO / SYSTEM");
}

static void drawTrailPage() {
  app.currentPage = PAGE_TRAIL;
  beginPage("TRAIL OPS", "BREADCRUMB / TRACKBACK MODULE", false);

  drawButton(backButton, "BACK", false);

  drawStatusChip(92, 12, 90, "GPS", gpsStatusText(), gpsStatusColor());
  drawStatusChip(190, 12, 90, "MODE", app.trackbackMode ? "RTN" : "EXP", app.trackbackMode ? C_ACCENT : C_GOOD);
  drawStatusChip(288, 12, 84, "REC", app.trailRecording ? "ON" : "OFF", app.trailRecording ? C_GOOD : C_ALERT);
  drawStatusChip(380, 12, 82, "HDG", String((int)app.mockHeading), C_TEXT);
  drawPageLabel("TRAIL OPS", "BREADCRUMB / TRACKBACK MODULE");

  drawPanel(18, 64, 140, 82, "TRIP");
  drawValue(28, 86, "TOTAL MILES", String(app.mockTripMiles, 2), C_GOOD);

  drawPanel(170, 64, 140, 82, "WAYPOINT");
  drawValue(180, 86, "COUNT", String(app.waypointCount), C_TEXT);
  drawValue(180, 114, "FROM LAST", String(app.mockFromWP, 2), C_ACCENT);

  drawPanel(322, 64, 140, 82, "RETURN");
  drawValue(332, 86, "HOME DIST", String(app.mockReturn, 2), C_TEXT);
  drawValue(332, 114, "STATE", app.trackbackMode ? "ACTIVE" : "STBY", app.trackbackMode ? C_ACCENT : C_TEXT_DIM);

  drawPanel(18, 158, 444, 76, "TRAIL NOTES");
  tft.setTextColor(C_TEXT, C_PANEL);
  tft.setTextSize(1);
  tft.setCursor(30, 182);
  tft.print("> PRESS START TO ARM TRAIL RECORDING");
  tft.setCursor(30, 198);
  tft.print("> PRESS MARK TO DROP WPxxx EVENT POINTS");
  drawTrailGpsNoteLine();

  drawButton(trailStart, app.trailRecording ? "PAUSE" : "START", app.trailRecording);
  drawButton(trailMark, "MARK");
  drawButton(trailReturn, app.trackbackMode ? "CANCEL" : "RETURN", app.trackbackMode, app.trackbackMode);
}

void updateGpsStatusUi() {
  if (millis() - gpsLastUiMs < 1000) {
    return;
  }

  gpsLastUiMs = millis();

  if (app.currentPage == PAGE_HOME) {
    drawStatusChip(18, 58, 110, "GPS", gpsStatusText(), gpsStatusColor());
  } else if (app.currentPage == PAGE_TRAIL) {
    drawStatusChip(92, 12, 90, "GPS", gpsStatusText(), gpsStatusColor());
    drawTrailGpsNoteLine();
  }
}

static void drawTruckPage() {
  app.currentPage = PAGE_TRUCK;
  beginPage("TRUCK DATA", "VEHICLE / OBD / CAN MONITOR", false);

  drawButton(backButton, "BACK", false);

  drawStatusChip(92, 12, 110, "OBD", "OFF", C_OFF);
  drawStatusChip(210, 12, 110, "CAN", "OFF", C_OFF);
  drawStatusChip(328, 12, 134, "LOGGER", "STBY", C_TEXT_DIM);
  drawPageLabel("TRUCK DATA", "VEHICLE / OBD / CAN MONITOR");

  drawPanel(18, 64, 140, 90, "TRIP");
  drawValue(30, 86, "DISTANCE", "--.--", C_TEXT);
  drawValue(30, 114, "AVG MPH", "--.-", C_TEXT);

  drawPanel(170, 64, 140, 90, "FUEL");
  drawValue(182, 86, "INST MPG", "--.-", C_TEXT);
  drawValue(182, 114, "AVG MPG", "--.-", C_TEXT);

  drawPanel(322, 64, 140, 90, "ENGINE");
  drawValue(334, 86, "COOLANT", "---", C_TEXT);
  drawValue(334, 114, "RPM", "----", C_TEXT);

  drawPanel(18, 168, 444, 98, "FUTURE FEATURE SET");
  tft.setTextColor(C_TEXT, C_PANEL);
  tft.setTextSize(1);
  tft.setCursor(30, 190);
  tft.print("> OBD-II TRIP METERS");
  tft.setCursor(30, 206);
  tft.print("> ESTIMATED FUEL USAGE / RANGE");
  tft.setCursor(30, 222);
  tft.print("> RADIATOR / FAN / ENGINEERING DATA LOGGING");
  tft.setCursor(30, 238);
  tft.print("> SUPRA AUX DISPLAY / CAN STREAMING");
}

static void drawBTPage() {
  app.currentPage = PAGE_BT;
  beginPage("BT HUB", "DEVICE SELECTOR / AUDIO CONTROL", false);

  drawButton(backButton, "BACK", false);

  drawStatusChip(92, 12, 130, "STEREO", "OFFLINE", C_ALERT);
  drawStatusChip(230, 12, 110, "MODE", "MUSIC", C_TEXT);
  drawStatusChip(348, 12, 114, "ACTIVE", String(app.selectedBT), C_GOOD);
  drawPageLabel("BT HUB", "DEVICE SELECTOR / AUDIO CONTROL");

  drawPanel(18, 64, 444, 40, "SELECT DEVICE");
  tft.setTextColor(C_TEXT_DIM, C_PANEL);
  tft.setTextSize(1);
  tft.setCursor(30, 82);
  tft.print("> FUTURE KVM-STYLE BLUETOOTH SELECTOR");

  drawButton(bt1, "PHONE 1", app.selectedBT == 1);
  drawButton(bt2, "PHONE 2", app.selectedBT == 2);
  drawButton(bt3, "PHONE 3", app.selectedBT == 3);
  drawButton(bt4, "PHONE 4", app.selectedBT == 4);

  drawPanel(18, 190, 444, 76, "STATUS");
  tft.setTextColor(C_TEXT, C_PANEL);
  tft.setTextSize(1);
  tft.setCursor(30, 212);
  tft.print("SELECTED DEVICE SLOT: ");
  tft.print(app.selectedBT);
  tft.setCursor(30, 228);
  tft.print("TRANSMIT PATH: NOT INSTALLED");
  tft.setCursor(30, 244);
  tft.print("USE CASE: TRUCK BLUETOOTH HUB / AUDIO SELECTOR");
}

static void drawSettingsPage() {
  app.currentPage = PAGE_SETTINGS;
  beginPage("SYSTEM", "MODULE STATUS / CONFIG", false);

  drawButton(backButton, "BACK", false);
  drawPageLabel("SYSTEM", "MODULE STATUS / CONFIG");

  drawPanel(18, 64, 215, 96, "INSTALLED");
  tft.setTextColor(C_TEXT, C_PANEL);
  tft.setTextSize(1);
  tft.setCursor(30, 88);
  tft.print("> ESP32 SCREEN BOARD");
  tft.setCursor(30, 104);
  tft.print("> TFT DISPLAY");
  tft.setCursor(30, 120);
  tft.print("> TOUCH INPUT");
  tft.setCursor(30, 136);
  tft.print("> GPS MODULE");

  drawPanel(247, 64, 215, 96, "PENDING");
  tft.setTextColor(C_TEXT, C_PANEL);
  tft.setCursor(259, 88);
  tft.print("> UI APP SHELL");
  tft.setCursor(259, 104);
  tft.print("> IMU MODULE");
  tft.setCursor(259, 120);
  tft.print("> PCF8574 BUTTON BOARD");
  tft.setCursor(259, 136);
  tft.print("> CAN / OBD MODULES");

  drawPanel(18, 176, 444, 90, "DESIGN LANGUAGE");
  tft.setTextColor(C_TEXT, C_PANEL);
  tft.setCursor(30, 200);
  tft.print("> DARK TACTICAL UI");
  tft.setCursor(30, 216);
  tft.print("> TOPOGRAPHIC BACKGROUND SYSTEM");
  tft.setCursor(30, 232);
  tft.print("> LARGE OPERABLE PANELS FOR VEHICLE USE");
}

void updateTrailDynamicFields() {
  tft.startWrite();

  drawStatusChip(190, 12, 90, "MODE", app.trackbackMode ? "RTN" : "EXP", app.trackbackMode ? C_ACCENT : C_GOOD);
  drawStatusChip(288, 12, 84, "REC", app.trailRecording ? "ON" : "OFF", app.trailRecording ? C_GOOD : C_ALERT);
  drawStatusChip(380, 12, 82, "HDG", String((int)app.mockHeading), C_TEXT);

  tft.fillRect(28, 84, 118, 52, C_PANEL);
  drawValue(28, 86, "TOTAL MILES", String(app.mockTripMiles, 2), C_GOOD);

  tft.fillRect(180, 84, 118, 52, C_PANEL);
  drawValue(180, 86, "COUNT", String(app.waypointCount), C_TEXT);
  drawValue(180, 114, "FROM LAST", String(app.mockFromWP, 2), C_ACCENT);

  tft.fillRect(332, 84, 118, 52, C_PANEL);
  drawValue(332, 86, "HOME DIST", String(app.mockReturn, 2), C_TEXT);
  drawValue(332, 114, "STATE", app.trackbackMode ? "ACTIVE" : "STBY", app.trackbackMode ? C_ACCENT : C_TEXT_DIM);

  drawFooter();

  tft.endWrite();
}

void goToPage(Page p) {
  tft.startWrite();

  switch (p) {
    case PAGE_HOME:     drawHome(); break;
    case PAGE_TRAIL:    drawTrailPage(); break;
    case PAGE_TRUCK:    drawTruckPage(); break;
    case PAGE_BT:       drawBTPage(); break;
    case PAGE_SETTINGS: drawSettingsPage(); break;
  }

  tft.endWrite();
}

static void handleHomeTouch(int x, int y) {
  if (hit(homeTrail, x, y)) {
    goToPage(PAGE_TRAIL);
  } else if (hit(homeTruck, x, y)) {
    goToPage(PAGE_TRUCK);
  } else if (hit(homeBT, x, y)) {
    goToPage(PAGE_BT);
  } else if (hit(homeSettings, x, y)) {
    goToPage(PAGE_SETTINGS);
  }
}

static void handleTrailTouch(int x, int y) {
  if (hitBackButton(x, y)) {
    goToPage(PAGE_HOME);
    return;
  }

  if (hit(trailStart, x, y)) {
    app.trailRecording = !app.trailRecording;
    if (app.trailRecording && app.mockTripMiles < 0.1f) {
      app.mockTripMiles = 0.12f;
    }
    goToPage(PAGE_TRAIL);
  } else if (hit(trailMark, x, y)) {
    if (app.trailRecording) {
      app.waypointCount++;
      app.mockFromWP = 0.00f;
    }
    goToPage(PAGE_TRAIL);
  } else if (hit(trailReturn, x, y)) {
    app.trackbackMode = !app.trackbackMode;
    goToPage(PAGE_TRAIL);
  }
}

static void handleTruckTouch(int x, int y) {
  if (hitBackButton(x, y)) {
    goToPage(PAGE_HOME);
  }
}

static void handleBTTouch(int x, int y) {
  if (hitBackButton(x, y)) {
    goToPage(PAGE_HOME);
    return;
  }

  bool changed = false;

  if (hit(bt1, x, y)) {
    app.selectedBT = 1;
    changed = true;
  }
  if (hit(bt2, x, y)) {
    app.selectedBT = 2;
    changed = true;
  }
  if (hit(bt3, x, y)) {
    app.selectedBT = 3;
    changed = true;
  }
  if (hit(bt4, x, y)) {
    app.selectedBT = 4;
    changed = true;
  }

  if (changed) {
    goToPage(PAGE_BT);
  }
}

static void handleSettingsTouch(int x, int y) {
  if (hitBackButton(x, y)) {
    goToPage(PAGE_HOME);
  }
}

void handleTouch(int x, int y) {
  Serial.print("Touch X=");
  Serial.print(x);
  Serial.print(" Y=");
  Serial.println(y);

  switch (app.currentPage) {
    case PAGE_HOME:     handleHomeTouch(x, y); break;
    case PAGE_TRAIL:    handleTrailTouch(x, y); break;
    case PAGE_TRUCK:    handleTruckTouch(x, y); break;
    case PAGE_BT:       handleBTTouch(x, y); break;
    case PAGE_SETTINGS: handleSettingsTouch(x, y); break;
  }
}
