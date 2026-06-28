#include <Arduino.h>
#include <TFT_eSPI.h>

#define TFT_BACKLIGHT 27
#define GPS_RX_PIN 35
#define GPS_BAUD 9600

TFT_eSPI tft = TFT_eSPI();
HardwareSerial gpsSerial(1);

const int SCREEN_W = 480;
const int SCREEN_H = 320;

char lineBuf[128];
uint8_t lineLen = 0;

String recentLines[8];
uint8_t recentIndex = 0;

unsigned long charCount = 0;
unsigned long sentenceCount = 0;
unsigned long lastByteMs = 0;
unsigned long lastDrawMs = 0;

bool hasFix = false;
int satellites = -1;

uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

String fieldFromNmea(const char* line, int wantedField) {
  int field = 0;
  const char* start = line;

  for (const char* p = line; ; p++) {
    if (*p == ',' || *p == '*' || *p == '\0') {
      if (field == wantedField) {
        return String(start).substring(0, p - start);
      }

      if (*p == '*' || *p == '\0') {
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

void addRecentLine(const String& line) {
  recentLines[recentIndex] = line;
  recentIndex = (recentIndex + 1) % 8;
}

String gpsStatus() {
  if (charCount == 0) {
    return "NO DATA";
  }

  if (millis() - lastByteMs > 3000) {
    return "STALE";
  }

  if (hasFix) {
    return "FIX";
  }

  return "NMEA";
}

void drawScreen() {
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(rgb(255, 190, 70), TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(12, 10);
  tft.print("GPS IO35 TEST");

  tft.setTextColor(rgb(150, 220, 120), TFT_BLACK);
  tft.setCursor(12, 42);
  tft.print("STATUS: ");
  tft.print(gpsStatus());

  tft.setTextSize(1);
  tft.setCursor(12, 74);
  tft.print("RX PIN: IO");
  tft.print(GPS_RX_PIN);
  tft.print("  BAUD: ");
  tft.print(GPS_BAUD);

  tft.setCursor(12, 92);
  tft.print("CHARS: ");
  tft.print(charCount);
  tft.print("  SENTENCES: ");
  tft.print(sentenceCount);

  tft.setCursor(12, 110);
  tft.print("FIX: ");
  tft.print(hasFix ? "YES" : "NO");
  tft.print("  SATS: ");
  tft.print(satellites);

  tft.setTextColor(rgb(210, 220, 210), TFT_BLACK);
  tft.setCursor(12, 140);
  tft.print("LAST NMEA:");

  for (int i = 0; i < 8; i++) {
    int idx = (recentIndex + i) % 8;
    String line = recentLines[idx];
    if (line.length() > 58) {
      line = line.substring(0, 58);
    }

    tft.setCursor(12, 160 + i * 18);
    tft.print(line);
  }
}

void handleSentence(const char* line) {
  sentenceCount++;
  addRecentLine(line);

  Serial.print("[GPS] ");
  Serial.println(line);

  if (nmeaTypeIs(line, "GGA")) {
    hasFix = fieldFromNmea(line, 6).toInt() > 0;
    satellites = fieldFromNmea(line, 7).toInt();
  } else if (nmeaTypeIs(line, "RMC")) {
    String valid = fieldFromNmea(line, 2);
    if (valid.length() > 0) {
      hasFix = valid[0] == 'A';
    }
  }
}

void readGps() {
  while (gpsSerial.available() > 0) {
    char c = (char)gpsSerial.read();
    charCount++;
    lastByteMs = millis();

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      if (lineLen > 0) {
        lineBuf[lineLen] = '\0';
        handleSentence(lineBuf);
        lineLen = 0;
      }
      continue;
    }

    if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = c;
    } else {
      lineLen = 0;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, HIGH);

  tft.init();
  tft.setRotation(1);

  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, -1);

  addRecentLine("Wire GPS TX -> CYD IO35");
  addRecentLine("Leave GPS RX disconnected");
  drawScreen();

  Serial.println("GPS IO35 screen test ready");
}

void loop() {
  readGps();

  if (millis() - lastDrawMs > 500) {
    lastDrawMs = millis();
    drawScreen();
  }
}
