#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <Arduino.h>
#include "M5Cardputer.h"

enum UIState {
    UI_FOLDER_SELECT,
    UI_PLAYER
};

extern UIState currentUIState;
extern M5Canvas sprite;
extern M5Canvas spr;

extern bool nextTrackRequest;

extern uint8_t sliderPos;
extern int16_t textPos;
extern uint8_t graphSpeed;
extern uint8_t g[14];
extern unsigned short grays[18];
extern unsigned short gray;
extern unsigned short light;
extern unsigned long trackStartMillis;
extern unsigned long playbackTime; 

void initUI();
void draw();
void handleKeyPress(char key);

#endif