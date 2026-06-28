#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <string.h>

#define TFT_BACKLIGHT 27
#define TOUCH_CS 33
#define TOUCH_IRQ 36

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

TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

// ==========================================================
// Touch calibration
// ==========================================================
const int RAW_X_LEFT   = 3708;
const int RAW_X_RIGHT  = 342;
const int RAW_Y_TOP    = 3602;
const int RAW_Y_BOTTOM = 330;

const int SCREEN_W = 480;
const int SCREEN_H = 320;

// ==========================================================
// Color helper
// ==========================================================
uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Tactical / military theme colors
const uint16_t C_BG          = 0x0000;                 // black
const uint16_t C_PANEL       = rgb(24, 34, 24);       // dark olive
const uint16_t C_PANEL_2     = rgb(30, 42, 30);       // slightly brighter olive
const uint16_t C_BORDER      = rgb(150, 130, 55);     // amber-ish
const uint16_t C_TEXT        = rgb(150, 220, 120);    // tactical green
const uint16_t C_TEXT_DIM    = rgb(95, 130, 85);      // dim green
const uint16_t C_ACCENT      = rgb(255, 190, 70);     // amber
const uint16_t C_ACCENT_DIM  = rgb(130, 95, 35);      // dim amber
const uint16_t C_ALERT       = rgb(255, 90, 60);      // alert red/orange
const uint16_t C_GOOD        = rgb(90, 220, 120);     // green
const uint16_t C_OFF         = rgb(80, 80, 80);       // grey
const uint16_t C_WHITE_SOFT  = rgb(210, 220, 210);    // soft white
const uint16_t C_TOPO        = rgb(28, 58, 36);       // dark topo green
const uint16_t C_TOPO_BOLD   = rgb(42, 86, 52);       // brighter topo

// ==========================================================
// UI state
// ==========================================================
enum Page {
  PAGE_HOME,
  PAGE_TRAIL,
  PAGE_TRUCK,
  PAGE_BT,
  PAGE_SETTINGS
};

Page currentPage = PAGE_HOME;

bool trailRecording = false;
bool trackbackMode = false;
int waypointCount = 0;
int selectedBT = 1;

bool wasTouched = false;
unsigned long bootTime = 0;

char gpsLine[128];
uint8_t gpsLineLen = 0;
unsigned long gpsCharCount = 0;
unsigned long gpsSentenceCount = 0;
unsigned long gpsLastByteMs = 0;
unsigned long gpsLastSentenceMs = 0;
unsigned long gpsLastUiMs = 0;
unsigned long gpsLastReportMs = 0;
unsigned long gpsScanWindowStartMs = 0;
unsigned long gpsScanWindowChars = 0;
unsigned long gpsScanWindowSentences = 0;
bool gpsHasFix = false;
bool gpsScanLocked = false;
int gpsSatellites = -1;
bool gpsCoordsValid = false;
double gpsLatitude = 0.0;
double gpsLongitude = 0.0;

const int GPS_RX_CANDIDATES[] = {
  GPS_RX_PIN, 16, 17, 4, 5, 18, 19, 21, 22, 23, 25, 26, 27, 32, 34, 35, 39
};

uint8_t gpsRxCandidateIndex = 0;

int activeGpsRxPin() {
#if GPS_PIN_SCAN
  return GPS_RX_CANDIDATES[gpsRxCandidateIndex];
#else
  return GPS_RX_PIN;
#endif
}

int activeGpsTxPin() {
#if GPS_PIN_SCAN
  return -1;
#else
  return GPS_TX_PIN;
#endif
}

void beginGpsUart() {
  if (activeGpsRxPin() == TFT_BACKLIGHT) {
    pinMode(TFT_BACKLIGHT, INPUT);
  } else {
    pinMode(TFT_BACKLIGHT, OUTPUT);
    digitalWrite(TFT_BACKLIGHT, HIGH);
  }

  Serial2.begin(GPS_BAUD, SERIAL_8N1, activeGpsRxPin(), activeGpsTxPin());
  gpsScanWindowStartMs = millis();
  gpsScanWindowChars = 0;
  gpsScanWindowSentences = 0;
}

// Mock values for UI feel
float mockTripMiles = 2.47f;
float mockFromWP    = 0.38f;
float mockReturn    = 2.44f;
float mockHeading   = 274.0f;

// ==========================================================
// Button structs
// ==========================================================
struct Button {
  int x;
  int y;
  int w;
  int h;
  const char* label;
};

Button homeTrail    = {20, 85, 210, 78, "TRAIL"};
Button homeTruck    = {250, 85, 210, 78, "TRUCK"};
Button homeBT       = {20, 180, 210, 78, "BT HUB"};
Button homeSettings = {250, 180, 210, 78, "SETTINGS"};

Button backButton   = {12, 12, 70, 28, "BACK"};

Button trailStart   = {18, 248, 138, 52, "START"};
Button trailMark    = {171, 248, 138, 52, "MARK"};
Button trailReturn  = {324, 248, 138, 52, "RETURN"};

Button bt1          = {18, 110, 105, 60, "DEV 1"};
Button bt2          = {132, 110, 105, 60, "DEV 2"};
Button bt3          = {246, 110, 105, 60, "DEV 3"};
Button bt4          = {360, 110, 105, 60, "DEV 4"};

// ==========================================================
// Touch mapping
// ==========================================================
int touchToScreenX(int rawX) {
  int x = map(rawX, RAW_X_LEFT, RAW_X_RIGHT, 0, SCREEN_W - 1);
  return constrain(x, 0, SCREEN_W - 1);
}

int touchToScreenY(int rawY) {
  int y = map(rawY, RAW_Y_TOP, RAW_Y_BOTTOM, 0, SCREEN_H - 1);
  return constrain(y, 0, SCREEN_H - 1);
}

bool hit(Button b, int x, int y) {
  return x >= b.x && x <= (b.x + b.w) && y >= b.y && y <= (b.y + b.h);
}

// ==========================================================
// Topographic background
// ==========================================================
void drawTopoLoop(int cx, int cy, int rx, int ry, int rings, uint16_t color) {
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

void drawTopoBackground() {
  // Long contour lines
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

  // Topographic "hill" clusters
  drawTopoLoop(88, 120, 78, 48, 5, C_TOPO);
  drawTopoLoop(365, 132, 96, 58, 6, C_TOPO);
  drawTopoLoop(240, 255, 120, 48, 5, C_TOPO);

  // Bolder contour details
  drawTopoLoop(88, 120, 52, 32, 2, C_TOPO_BOLD);
  drawTopoLoop(365, 132, 64, 38, 2, C_TOPO_BOLD);
}

// ==========================================================
// Drawing helpers
// ==========================================================
void drawOuterFrame() {
  tft.drawRect(2, 2, SCREEN_W - 4, SCREEN_H - 4, C_BORDER);
  tft.drawRect(4, 4, SCREEN_W - 8, SCREEN_H - 8, C_ACCENT_DIM);
}

void drawHeader(const char* title, const char* subtitle) {
  tft.fillRect(0, 0, SCREEN_W, 50, C_BG);
  tft.drawFastHLine(0, 50, SCREEN_W, C_BORDER);

  tft.setTextColor(C_ACCENT, C_BG);
  tft.setTextSize(2);
  tft.setCursor(16, 10);
  tft.print(title);

  tft.setTextColor(C_TEXT_DIM, C_BG);
  tft.setTextSize(1);
  tft.setCursor(18, 34);
  tft.print(subtitle);
}

void drawStatusChip(int x, int y, int w, const String& label, const String& value, uint16_t valueColor) {
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

uint16_t gpsStatusColor() {
  String status = gpsStatusText();

  if (status == "LOCK") {
    return C_GOOD;
  }

  if (status == "RX") {
    return C_ACCENT;
  }

  return C_ALERT;
}

String nmeaField(const char* line, int wantedField) {
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

bool nmeaTypeIs(const char* line, const char* type) {
  return line[0] == '$' && strlen(line) >= 6 && strncmp(line + 3, type, 3) == 0;
}

double nmeaCoordToDecimal(const String& raw, const String& hemisphere) {
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

void updateGpsCoords(const String& lat, const String& ns, const String& lon, const String& ew) {
  if (lat.length() == 0 || ns.length() == 0 || lon.length() == 0 || ew.length() == 0) {
    return;
  }

  gpsLatitude = nmeaCoordToDecimal(lat, ns);
  gpsLongitude = nmeaCoordToDecimal(lon, ew);
  gpsCoordsValid = true;
}

void handleGpsSentence(const char* line) {
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
  }
}

void processGpsChar(char c) {
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

void pollGps() {
  while (Serial2.available() > 0) {
    processGpsChar((char)Serial2.read());
  }

#if GPS_UART0_SNIFF
  while (Serial.available() > 0) {
    processGpsChar((char)Serial.read());
  }
#endif
}

void pollGpsPinScanner() {
#if GPS_PIN_SCAN
  if (gpsScanLocked) {
    return;
  }

  if (gpsScanWindowChars >= 16 || gpsScanWindowSentences > 0) {
    gpsScanLocked = true;
    Serial.print("[GPS SCAN] activity found, holding RX=");
    Serial.print(activeGpsRxPin());
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
  Serial.print(activeGpsRxPin());
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
  beginGpsUart();

  Serial.print("[GPS SCAN] listening rx=");
  Serial.print(activeGpsRxPin());
  Serial.print(" baud=");
  Serial.println(GPS_BAUD);
#endif
}

void reportGpsDiagnostics() {
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
  Serial.print(activeGpsRxPin());
#if GPS_UART0_SNIFF
  Serial.print(" +uart0rx");
#endif
  Serial.print(" baud=");
  Serial.println(GPS_BAUD);
}

void drawFooter() {
  unsigned long uptime = (millis() - bootTime) / 1000;

  tft.fillRect(0, 302, SCREEN_W, 18, C_BG);
  tft.drawFastHLine(0, 301, SCREEN_W, C_ACCENT_DIM);

  tft.setTextColor(C_TEXT_DIM, C_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 307);
  tft.print("SYS:NOMINAL  UPTIME:");
  tft.print(uptime);
  tft.print("s");
}

void drawPanel(int x, int y, int w, int h, const String& title) {
  tft.fillRect(x, y, w, h, C_PANEL);
  tft.drawRect(x, y, w, h, C_BORDER);

  // Corner accents
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

void drawButton(const Button& b, const String& label, bool selected = false, bool alert = false) {
  uint16_t fill   = selected ? C_PANEL_2 : C_PANEL;
  uint16_t border = selected ? C_ACCENT : C_ACCENT_DIM;
  uint16_t text   = selected ? C_WHITE_SOFT : C_TEXT;

  if (alert) {
    border = C_ALERT;
    text = C_ALERT;
  }

  tft.fillRect(b.x, b.y, b.w, b.h, fill);
  tft.drawRect(b.x, b.y, b.w, b.h, border);

  // Tactical corner marks
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

void drawValue(int x, int y, const String& label, const String& value, uint16_t valueColor = C_TEXT) {
  tft.setTextColor(C_TEXT_DIM, C_PANEL);
  tft.setTextSize(1);
  tft.setCursor(x, y);
  tft.print(label);

  tft.setTextColor(valueColor, C_PANEL);
  tft.setTextSize(2);
  tft.setCursor(x, y + 12);
  tft.print(value);
}

String gpsCoordText() {
  if (!gpsCoordsValid) {
    return "GPS COORDS: WAITING FOR NMEA";
  }

  return "GPS: " + String(gpsLatitude, 6) + ", " + String(gpsLongitude, 6);
}

void drawTrailGpsNoteLine() {
  tft.fillRect(30, 212, 410, 12, C_PANEL);
  tft.setTextColor(gpsCoordsValid ? C_GOOD : C_ACCENT, C_PANEL);
  tft.setTextSize(1);
  tft.setCursor(30, 214);
  tft.print("> ");
  tft.print(gpsCoordText());
}

void beginPage(const char* title, const char* subtitle) {
  tft.fillScreen(C_BG);
  drawTopoBackground();
  drawOuterFrame();
  drawHeader(title, subtitle);
  drawFooter();
}

// ==========================================================
// Pages
// ==========================================================
void drawHome() {
  currentPage = PAGE_HOME;
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

void drawTrailPage() {
  currentPage = PAGE_TRAIL;
  beginPage("TRAIL OPS", "BREADCRUMB / TRACKBACK MODULE");

  drawButton(backButton, "BACK", false);

  drawStatusChip(92, 12, 90, "GPS", gpsStatusText(), gpsStatusColor());
  drawStatusChip(190, 12, 90, "MODE", trackbackMode ? "RTN" : "EXP", trackbackMode ? C_ACCENT : C_GOOD);
  drawStatusChip(288, 12, 84, "REC", trailRecording ? "ON" : "OFF", trailRecording ? C_GOOD : C_ALERT);
  drawStatusChip(380, 12, 82, "HDG", String((int)mockHeading), C_TEXT);

  drawPanel(18, 64, 140, 82, "TRIP");
  drawValue(28, 86, "TOTAL MILES", String(mockTripMiles, 2), C_GOOD);

  drawPanel(170, 64, 140, 82, "WAYPOINT");
  drawValue(180, 86, "COUNT", String(waypointCount), C_TEXT);
  drawValue(180, 114, "FROM LAST", String(mockFromWP, 2), C_ACCENT);

  drawPanel(322, 64, 140, 82, "RETURN");
  drawValue(332, 86, "HOME DIST", String(mockReturn, 2), C_TEXT);
  drawValue(332, 114, "STATE", trackbackMode ? "ACTIVE" : "STBY", trackbackMode ? C_ACCENT : C_TEXT_DIM);

  drawPanel(18, 158, 444, 76, "TACTICAL NOTES");
  tft.setTextColor(C_TEXT, C_PANEL);
  tft.setTextSize(1);
  tft.setCursor(30, 182);
  tft.print("> PRESS START TO ARM TRAIL RECORDING");
  tft.setCursor(30, 198);
  tft.print("> PRESS MARK TO DROP WPxxx EVENT POINTS");
  drawTrailGpsNoteLine();

  drawButton(trailStart, trailRecording ? "PAUSE" : "START", trailRecording);
  drawButton(trailMark, "MARK");
  drawButton(trailReturn, trackbackMode ? "CANCEL" : "RETURN", trackbackMode, trackbackMode);
}

void updateGpsStatusUi() {
  if (millis() - gpsLastUiMs < 1000) {
    return;
  }

  gpsLastUiMs = millis();

  if (currentPage == PAGE_HOME) {
    drawStatusChip(18, 58, 110, "GPS", gpsStatusText(), gpsStatusColor());
  } else if (currentPage == PAGE_TRAIL) {
    drawStatusChip(92, 12, 90, "GPS", gpsStatusText(), gpsStatusColor());
    drawTrailGpsNoteLine();
  }
}

void drawTruckPage() {
  currentPage = PAGE_TRUCK;
  beginPage("TRUCK DATA", "VEHICLE / OBD / CAN MONITOR");

  drawButton(backButton, "BACK", false);

  drawStatusChip(92, 12, 110, "OBD", "OFF", C_OFF);
  drawStatusChip(210, 12, 110, "CAN", "OFF", C_OFF);
  drawStatusChip(328, 12, 134, "LOGGER", "STBY", C_TEXT_DIM);

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

void drawBTPage() {
  currentPage = PAGE_BT;
  beginPage("BT HUB", "DEVICE SELECTOR / AUDIO CONTROL");

  drawButton(backButton, "BACK", false);

  drawStatusChip(92, 12, 130, "STEREO", "OFFLINE", C_ALERT);
  drawStatusChip(230, 12, 110, "MODE", "MUSIC", C_TEXT);
  drawStatusChip(348, 12, 114, "ACTIVE", String(selectedBT), C_GOOD);

  drawPanel(18, 64, 444, 40, "SELECT DEVICE");
  tft.setTextColor(C_TEXT_DIM, C_PANEL);
  tft.setTextSize(1);
  tft.setCursor(30, 82);
  tft.print("> FUTURE KVM-STYLE BLUETOOTH SELECTOR");

  drawButton(bt1, "PHONE 1", selectedBT == 1);
  drawButton(bt2, "PHONE 2", selectedBT == 2);
  drawButton(bt3, "PHONE 3", selectedBT == 3);
  drawButton(bt4, "PHONE 4", selectedBT == 4);

  drawPanel(18, 190, 444, 76, "STATUS");
  tft.setTextColor(C_TEXT, C_PANEL);
  tft.setTextSize(1);
  tft.setCursor(30, 212);
  tft.print("SELECTED DEVICE SLOT: ");
  tft.print(selectedBT);
  tft.setCursor(30, 228);
  tft.print("TRANSMIT PATH: NOT INSTALLED");
  tft.setCursor(30, 244);
  tft.print("USE CASE: TRUCK BLUETOOTH HUB / AUDIO SELECTOR");
}

void drawSettingsPage() {
  currentPage = PAGE_SETTINGS;
  beginPage("SYSTEM", "MODULE STATUS / CONFIG");

  drawButton(backButton, "BACK", false);

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
  tft.print("> UI APP SHELL");

  drawPanel(247, 64, 215, 96, "PENDING");
  tft.setTextColor(C_TEXT, C_PANEL);
  tft.setCursor(259, 88);
  tft.print("> GPS MODULE");
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

// ==========================================================
// Dynamic partial updates
// ==========================================================
void updateTrailDynamicFields() {
  tft.startWrite();

  drawStatusChip(190, 12, 90, "MODE", trackbackMode ? "RTN" : "EXP", trackbackMode ? C_ACCENT : C_GOOD);
  drawStatusChip(288, 12, 84, "REC", trailRecording ? "ON" : "OFF", trailRecording ? C_GOOD : C_ALERT);
  drawStatusChip(380, 12, 82, "HDG", String((int)mockHeading), C_TEXT);

  // TRIP value area
  tft.fillRect(28, 84, 118, 52, C_PANEL);
  drawValue(28, 86, "TOTAL MILES", String(mockTripMiles, 2), C_GOOD);

  // WAYPOINT value area
  tft.fillRect(180, 84, 118, 52, C_PANEL);
  drawValue(180, 86, "COUNT", String(waypointCount), C_TEXT);
  drawValue(180, 114, "FROM LAST", String(mockFromWP, 2), C_ACCENT);

  // RETURN value area
  tft.fillRect(332, 84, 118, 52, C_PANEL);
  drawValue(332, 86, "HOME DIST", String(mockReturn, 2), C_TEXT);
  drawValue(332, 114, "STATE", trackbackMode ? "ACTIVE" : "STBY", trackbackMode ? C_ACCENT : C_TEXT_DIM);

  drawFooter();

  tft.endWrite();
}

// ==========================================================
// Page routing
// ==========================================================
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

// ==========================================================
// Touch handlers
// ==========================================================
void handleHomeTouch(int x, int y) {
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

void handleTrailTouch(int x, int y) {
  if (hit(backButton, x, y)) {
    goToPage(PAGE_HOME);
    return;
  }

  if (hit(trailStart, x, y)) {
    trailRecording = !trailRecording;
    if (trailRecording && mockTripMiles < 0.1f) {
      mockTripMiles = 0.12f;
    }
    goToPage(PAGE_TRAIL);
  } else if (hit(trailMark, x, y)) {
    if (trailRecording) {
      waypointCount++;
      mockFromWP = 0.00f;
    }
    goToPage(PAGE_TRAIL);
  } else if (hit(trailReturn, x, y)) {
    trackbackMode = !trackbackMode;
    goToPage(PAGE_TRAIL);
  }
}

void handleTruckTouch(int x, int y) {
  if (hit(backButton, x, y)) {
    goToPage(PAGE_HOME);
  }
}

void handleBTTouch(int x, int y) {
  if (hit(backButton, x, y)) {
    goToPage(PAGE_HOME);
    return;
  }

  bool changed = false;

  if (hit(bt1, x, y)) {
    selectedBT = 1;
    changed = true;
  }
  if (hit(bt2, x, y)) {
    selectedBT = 2;
    changed = true;
  }
  if (hit(bt3, x, y)) {
    selectedBT = 3;
    changed = true;
  }
  if (hit(bt4, x, y)) {
    selectedBT = 4;
    changed = true;
  }

  if (changed) {
    goToPage(PAGE_BT);
  }
}

void handleSettingsTouch(int x, int y) {
  if (hit(backButton, x, y)) {
    goToPage(PAGE_HOME);
  }
}

void handleTouch(int x, int y) {
  Serial.print("Touch X=");
  Serial.print(x);
  Serial.print(" Y=");
  Serial.println(y);

  switch (currentPage) {
    case PAGE_HOME:     handleHomeTouch(x, y); break;
    case PAGE_TRAIL:    handleTrailTouch(x, y); break;
    case PAGE_TRUCK:    handleTruckTouch(x, y); break;
    case PAGE_BT:       handleBTTouch(x, y); break;
    case PAGE_SETTINGS: handleSettingsTouch(x, y); break;
  }
}

// ==========================================================
// Setup / loop
// ==========================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  beginGpsUart();

  bootTime = millis();

  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, HIGH);

  tft.init();
  tft.setRotation(1);

  SPI.begin(14, 12, 13);

  touch.begin();
  touch.setRotation(1);

  goToPage(PAGE_HOME);

  Serial.println("DisplayCore Alpha tactical topo UI ready");
  Serial.print("GPS UART listening on RX=");
  Serial.print(activeGpsRxPin());
  Serial.print(" TX=");
  Serial.print(activeGpsTxPin());
  Serial.print(" baud=");
  Serial.println(GPS_BAUD);
}

void loop() {
  pollGps();
  pollGpsPinScanner();
  updateGpsStatusUi();
  reportGpsDiagnostics();

  // Mock trail updates so the Trail page feels alive
  static unsigned long lastMockUpdate = 0;
  if (millis() - lastMockUpdate > 1000) {
    lastMockUpdate = millis();

    if (trailRecording && !trackbackMode) {
      mockTripMiles += 0.01f;
      mockFromWP += 0.01f;
      mockReturn = mockTripMiles;
      mockHeading += 1.2f;

      if (mockHeading >= 360.0f) {
        mockHeading -= 360.0f;
      }

      if (currentPage == PAGE_TRAIL) {
        updateTrailDynamicFields();
      } else {
        drawFooter();
      }
    } else {
      drawFooter();
    }
  }

  bool isTouched = touch.touched();

  if (isTouched && !wasTouched) {
    TS_Point p = touch.getPoint();

    int sx = touchToScreenX(p.x);
    int sy = touchToScreenY(p.y);

    handleTouch(sx, sy);

    delay(120); // crude debounce
  }

  wasTouched = isTouched;

  delay(20);
}
