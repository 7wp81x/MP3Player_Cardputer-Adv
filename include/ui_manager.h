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

enum PressedButton { 
    NONE = 0, 
    BTN_A, 
    BTN_P, 
    BTN_N, 
    BTN_R 
};

struct KeyRepeat {
    char key = 0;
    unsigned long firstPressTime = 0;
    unsigned long lastActionTime = 0;
    bool isRepeating = false;
};

extern UIState currentUIState;
extern KeyRepeat repeatState;
extern PressedButton pressedBtn;
extern bool isPlaying;
extern bool isStoped;
extern bool nextTrackRequest;

extern M5Canvas sprite1;
extern M5Canvas sprite2;
extern String popupText;
extern int8_t popupMenuIndex;
extern unsigned long popupStart;
extern const char* popupOptions[];
extern const uint8_t VISIBLE_FILE_COUNT;

extern int16_t marqueePos;
extern int16_t textPos;
extern uint8_t sliderPos;
extern uint8_t g[14];
extern unsigned short grays[18];
extern unsigned short gray, light;

extern String currentFolder;
extern uint8_t selectedFolderIndex;
extern uint8_t selectedFileIndex;
extern uint16_t viewStartIndex;
extern int playingFileIndex;

extern unsigned long trackStartMillis;
extern unsigned long playbackTime;
extern bool screenTimeoutEnabled;
extern bool isScreenDimmed;
extern int stableBat;

extern const unsigned long HOLD_THRESHOLD_MS;
extern const unsigned long REPEAT_INTERVAL_MS;
extern const unsigned long POPUP_DURATION;

void initUI();
void draw();
void handleKeyPress(char key);
void drawPopupMenu();
void drawSettingsMenu();
String getPlaybackTimeString();
void resetActivityTimer();

#endif