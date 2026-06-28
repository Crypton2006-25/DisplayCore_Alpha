#include "touch_input.h"

#include <Arduino.h>
#include <XPT2046_Touchscreen.h>

#define TOUCH_CS 33
#define TOUCH_IRQ 36

const int RAW_X_LEFT   = 3708;
const int RAW_X_RIGHT  = 342;
const int RAW_Y_TOP    = 3602;
const int RAW_Y_BOTTOM = 330;

const int SCREEN_W = 480;
const int SCREEN_H = 320;

static XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
static bool wasTouched = false;

void touchInit() {
  touch.begin();
  touch.setRotation(1);
}

int touchToScreenX(int rawX) {
  int x = map(rawX, RAW_X_LEFT, RAW_X_RIGHT, 0, SCREEN_W - 1);
  return constrain(x, 0, SCREEN_W - 1);
}

int touchToScreenY(int rawY) {
  int y = map(rawY, RAW_Y_TOP, RAW_Y_BOTTOM, 0, SCREEN_H - 1);
  return constrain(y, 0, SCREEN_H - 1);
}

bool readTouchPress(int& x, int& y) {
  bool isTouched = touch.touched();

  if (isTouched && !wasTouched) {
    TS_Point p = touch.getPoint();
    x = touchToScreenX(p.x);
    y = touchToScreenY(p.y);
    wasTouched = isTouched;
    return true;
  }

  wasTouched = isTouched;
  return false;
}
