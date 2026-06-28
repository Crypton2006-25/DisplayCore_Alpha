#ifndef UI_H
#define UI_H

#include "app_state.h"

void uiInit();
void goToPage(Page p);
void handleTouch(int x, int y);
void drawFooter();
void updateTrailDynamicFields();
void updateGpsStatusUi();

#endif
