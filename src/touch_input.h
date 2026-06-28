#ifndef TOUCH_INPUT_H
#define TOUCH_INPUT_H

void touchInit();
int touchToScreenX(int rawX);
int touchToScreenY(int rawY);
bool readTouchPress(int& x, int& y);

#endif
