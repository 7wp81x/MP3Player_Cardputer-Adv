#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <Arduino.h>
#include "M5Cardputer.h"

enum UIState {
    UI_FOLDER_SELECT,
    UI_PLAYER,
    UI_POPUP_MENU,
    UI_SETTINGS
};

// Hold / repeat handling
struct KeyRepeat {
    char key = 0;
    unsigned long firstPressTime = 0;
    unsigned long lastActionTime = 0;
    bool isRepeating = false;
};

extern KeyRepeat repeatState;

extern const unsigned long HOLD_THRESHOLD_MS;
extern const unsigned long REPEAT_INTERVAL_MS;



extern UIState currentUIState;
extern M5Canvas sprite1;
extern M5Canvas sprite2;
extern M5Canvas overlaySprite;

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

extern String currentFolder;
extern uint8_t fileCount;
extern uint8_t currentFileIndex;
extern uint8_t selectedFileIndex;
extern uint16_t viewStartIndex;
extern int8_t popupMenuIndex;

extern bool isPlaying;
extern bool isStoped;

extern bool screenTimeoutEnabled;
extern String popupText;
extern unsigned long popupStart;
extern const unsigned long POPUP_DURATION;
extern const uint8_t VISIBLE_FILE_COUNT;

void initUI();
void draw();
void handleKeyPress(char key);
void drawPopupMenu();
void drawSettingsMenu();

String getPlaybackTimeString();
extern void resetActivityTimer();

#endif