#include "timezone.h"

#include <string.h>

#include "gps.h"
#include "storage.h"

struct LocalDateTime {
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
};

static TimezoneMode selectedTimezone = TZ_AZ;

static const char* TZ_NAMES[] = {
  "AZ",
  "PACIFIC",
  "MOUNTAIN",
  "CENTRAL",
  "EASTERN",
  "UTC"
};

static const char* TZ_LABELS[] = {
  "AZ",
  "PT",
  "MT",
  "CT",
  "ET",
  "UTC"
};

static bool isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int daysInMonth(int year, int month) {
  static const int DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  if (month == 2 && isLeapYear(year)) {
    return 29;
  }

  return DAYS[month - 1];
}

static int dayOfWeek(int year, int month, int day) {
  static const int OFFSETS[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};

  if (month < 3) {
    year--;
  }

  return (year + year / 4 - year / 100 + year / 400 + OFFSETS[month - 1] + day) % 7;
}

static int nthSunday(int year, int month, int occurrence) {
  int firstDayDow = dayOfWeek(year, month, 1);
  int firstSunday = 1 + ((7 - firstDayDow) % 7);
  return firstSunday + ((occurrence - 1) * 7);
}

static int compareUtcDateTime(const LocalDateTime& value, int month, int day, int hour) {
  if (value.month != month) {
    return value.month < month ? -1 : 1;
  }
  if (value.day != day) {
    return value.day < day ? -1 : 1;
  }
  if (value.hour != hour) {
    return value.hour < hour ? -1 : 1;
  }
  return 0;
}

static int standardUtcOffsetHours(TimezoneMode mode) {
  switch (mode) {
    case TZ_PACIFIC:  return -8;
    case TZ_MOUNTAIN: return -7;
    case TZ_CENTRAL:  return -6;
    case TZ_EASTERN:  return -5;
    case TZ_UTC:      return 0;
    case TZ_AZ:
    default:          return -7;
  }
}

static bool timezoneUsesDst(TimezoneMode mode) {
  return mode == TZ_PACIFIC || mode == TZ_MOUNTAIN || mode == TZ_CENTRAL || mode == TZ_EASTERN;
}

static bool isDstInEffectUtc(const LocalDateTime& utc, TimezoneMode mode) {
  if (!timezoneUsesDst(mode)) {
    return false;
  }

  int standardOffset = standardUtcOffsetHours(mode);
  int dstOffset = standardOffset + 1;
  int startDay = nthSunday(utc.year, 3, 2);
  int endDay = nthSunday(utc.year, 11, 1);
  int startUtcHour = 2 - standardOffset;
  int endUtcHour = 2 - dstOffset;

  return compareUtcDateTime(utc, 3, startDay, startUtcHour) >= 0 &&
         compareUtcDateTime(utc, 11, endDay, endUtcHour) < 0;
}

static void addHours(LocalDateTime& value, int hours) {
  value.hour += hours;

  while (value.hour < 0) {
    value.hour += 24;
    value.day--;
    if (value.day < 1) {
      value.month--;
      if (value.month < 1) {
        value.month = 12;
        value.year--;
      }
      value.day = daysInMonth(value.year, value.month);
    }
  }

  while (value.hour >= 24) {
    value.hour -= 24;
    value.day++;
    if (value.day > daysInMonth(value.year, value.month)) {
      value.day = 1;
      value.month++;
      if (value.month > 12) {
        value.month = 1;
        value.year++;
      }
    }
  }
}

static bool parseGpsUtc(LocalDateTime& out) {
  if (!gpsDateTimeValid()) {
    return false;
  }

  const char* date = gpsUtcDateCStr();
  const char* time = gpsUtcTimeCStr();

  if (strlen(date) != 10 || strlen(time) != 8) {
    return false;
  }

  out.year = atoi(date);
  out.month = atoi(date + 5);
  out.day = atoi(date + 8);
  out.hour = atoi(time);
  out.minute = atoi(time + 3);
  out.second = atoi(time + 6);

  return out.year >= 2000 &&
         out.month >= 1 && out.month <= 12 &&
         out.day >= 1 && out.day <= daysInMonth(out.year, out.month) &&
         out.hour >= 0 && out.hour <= 23 &&
         out.minute >= 0 && out.minute <= 59 &&
         out.second >= 0 && out.second <= 59;
}

static bool localDateTime(LocalDateTime& out) {
  LocalDateTime utc;
  if (!parseGpsUtc(utc)) {
    return false;
  }

  int offset = standardUtcOffsetHours(selectedTimezone);
  if (isDstInEffectUtc(utc, selectedTimezone)) {
    offset++;
  }

  out = utc;
  addHours(out, offset);
  return true;
}

static TimezoneMode timezoneFromName(const char* name) {
  for (int i = 0; i <= TZ_UTC; i++) {
    if (strcmp(name, TZ_NAMES[i]) == 0) {
      return (TimezoneMode)i;
    }
  }

  return TZ_AZ;
}

void timezoneInit() {
  selectedTimezone = TZ_AZ;

  char savedTimezone[16];
  if (storageLoadTimezone(savedTimezone, sizeof(savedTimezone))) {
    selectedTimezone = timezoneFromName(savedTimezone);
  }
}

void timezoneCycleAndSave() {
  selectedTimezone = (TimezoneMode)((selectedTimezone + 1) % (TZ_UTC + 1));

  if (storageIsReady()) {
    storageSaveTimezone(timezoneName());
  }
}

const char* timezoneName() {
  return TZ_NAMES[selectedTimezone];
}

const char* timezoneLabel() {
  return TZ_LABELS[selectedTimezone];
}

bool timezoneFormatLocalDateTime(char* out, size_t outLen) {
  LocalDateTime local;
  if (!localDateTime(local)) {
    return false;
  }

  int hour12 = local.hour % 12;
  if (hour12 == 0) {
    hour12 = 12;
  }

  snprintf(out, outLen, "%02d-%02d-%02d %d:%02d %s %s",
           local.month,
           local.day,
           local.year % 100,
           hour12,
           local.minute,
           local.hour < 12 ? "AM" : "PM",
           timezoneLabel());
  return true;
}

bool timezoneLocalDateKey(char* out, size_t outLen) {
  LocalDateTime local;
  if (!localDateTime(local)) {
    return false;
  }

  snprintf(out, outLen, "%04d-%02d-%02d", local.year, local.month, local.day);
  return true;
}
