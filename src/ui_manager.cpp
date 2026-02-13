#include "ui_manager.h"
#include "font.h"
#include "file_manager.h"
#include "audio_config.h"
#include "settings.h"

UIState currentUIState = UI_FOLDER_SELECT;
M5Canvas sprite1(&M5Cardputer.Display);
M5Canvas sprite2(&M5Cardputer.Display);

bool nextTrackRequest = false;
const uint8_t VISIBLE_FILE_COUNT = 9;

bool isScreenDimmed = false;
constexpr unsigned long SCREEN_DIM_TIMEOUT = 30000;
unsigned long lastActivityTime = 0;

uint8_t sliderPos = 0;
int16_t textPos = 90;
uint8_t graphSpeed = 0;
uint8_t g[14] = {0};
unsigned short grays[18];
unsigned short gray, light;

unsigned long trackStartMillis = 0;
unsigned long playbackTime = 0;

uint8_t volumeStep = 2;
uint8_t brightnessStep = 10;
uint8_t selectedFolderIndex = 0;
uint8_t selectedFileIndex = 0;
uint16_t viewStartIndex = 0; 
bool screenTimeoutEnabled = false;

String popupText = "";
unsigned long popupStart = 0;
const unsigned long POPUP_DURATION = 1000;
enum PressedButton { NONE = 0, BTN_A, BTN_P, BTN_N, BTN_R };
PressedButton pressedBtn = NONE;
unsigned long pressAnimStart = 0;
const unsigned long PRESS_ANIM_DURATION = 120;  // ms - feels snappy
KeyRepeat repeatState;

const unsigned long HOLD_THRESHOLD_MS   = 500;   // ms before repeat starts
const unsigned long REPEAT_INTERVAL_MS  = 120;   // ms between repeats

int16_t marqueePos = 0;               // current scroll offset
unsigned long lastMarqueeUpdate = 0;  // last time we moved it
const unsigned long MARQUEE_INTERVAL = 180;  // ms between steps
const int MARQUEE_SPEED = 2;                 // pixels per step
int playingFileIndex = -1;
int8_t popupMenuIndex = 0;
const char* popupOptions[] = {"Select Folder", "USB Mass Storage", "Settings"};
int stableBat = 0;


void drawPopupMenu() {
    sprite1.fillRoundRect(30, 20, 180, 100, 8, TFT_BLACK); // Dark background
    sprite1.drawRoundRect(30, 20, 180, 100, 8, TFT_WHITE); // White border
    
    sprite1.setTextColor(TFT_YELLOW);
    sprite1.setTextDatum(MC_DATUM); // Middle-Center alignment
    sprite1.drawString("MAIN MENU", 120, 35);

    for (int i = 0; i < 3; i++) {
        if (i == popupMenuIndex) {
            sprite1.fillRect(35, 52 + (i * 20), 170, 18, TFT_GREENYELLOW);
            sprite1.setTextColor(TFT_BLACK); 
        } else {
            sprite1.setTextColor(TFT_LIGHTGREY);
        }
        sprite1.drawString(popupOptions[i], 120, 61 + (i * 20));
    }

    sprite1.pushSprite(0, 0);

    sprite1.setTextDatum(TL_DATUM); 
}

void drawSettingsMenu() {
    sprite1.fillSprite(TFT_BLACK);
    sprite1.setTextColor(TFT_CYAN);
    sprite1.drawString("SETTINGS", 120, 20);
    
    sprite1.setTextColor(TFT_WHITE);
    sprite1.drawString("Boot Folder:", 120, 50);
    sprite1.setTextColor(TFT_GREEN);
    sprite1.drawString(defaultBootFolder, 120, 70);
    
    sprite1.setTextColor(TFT_DARKGREY);
    sprite1.drawString("Enter: Set current folder", 120, 100);
    sprite1.drawString("Esc (`): Back", 120, 115);
    sprite1.pushSprite(0, 0);
}




void initUI() {
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(brightnessStep * 2);
    sprite1.createSprite(240, 135);
    sprite2.createSprite(86, 16);

    uint8_t co = 214;
    for (uint8_t i = 0; i < 18; i++) {
        grays[i] = M5Cardputer.Display.color565(co, co, co + 40);
        co -= 13;
    }
    lastActivityTime = millis();
}

void resetActivityTimer() {
    lastActivityTime = millis();
    if (isScreenDimmed) {
        M5Cardputer.Display.wakeup();
        isScreenDimmed = false;
    }
}

void checkScreenTimeout() {
    if (!screenTimeoutEnabled || isScreenDimmed) return;
    if (millis() - lastActivityTime > SCREEN_DIM_TIMEOUT) {
        M5Cardputer.Display.sleep();
        isScreenDimmed = true;
    }
}

String getPlaybackTimeString() {
    unsigned long elapsed = playbackTime;
    if (isPlaying && !isStoped) elapsed = millis() - trackStartMillis;
    unsigned int sec = (elapsed / 1000) % 60;
    unsigned int min = elapsed / 60000;
    char buf[6];
    sprintf(buf, "%02u:%02u", min, sec);
    return String(buf);
}

void drawFolderSelect() {
    gray = grays[15];
    light = grays[10];

    sprite1.fillRect(0, 0, 240, 135, gray);
    sprite1.drawRoundRect(0, 0, 240, 135, 4, light);

    sprite1.setTextFont(1);
    sprite1.setTextColor(ORANGE, gray);
    sprite1.setTextDatum(4);
    sprite1.drawString("Select Folder", 75, 10);

    sprite1.fillRoundRect(10, 22, 220, 20, 4, grays[12]);
    sprite1.setFont(&fonts::lgfxJapanGothic_12);
    sprite1.setTextColor(WHITE, grays[12]);
    sprite1.setTextDatum(0);
    sprite1.drawString(currentFolder, 16, 27);
    sprite1.setTextFont(1);

    sprite1.setTextFont(0);
    sprite1.setTextColor(GREEN, gray);
    sprite1.setTextDatum(2);
    sprite1.drawString("[Audios: " + String(fileCount) + "]", 230, 12);

    sprite1.setTextFont(0);
    sprite1.setTextDatum(0);
    sprite1.setTextColor(grays[5], gray);
    sprite1.drawString("[;]/[.]-Nav   [ok]-Open   [Esc]-Back", 10, 122);

    sprite1.setFont(&fonts::lgfxJapanGothic_12);

    const int startY = 48;
    const int lineHeight = 14;
    const int maxVisible = 5;

    bool hasParent = (currentFolder != "/");
    int baseParent = hasParent ? 1 : 0;
    int totalItems = baseParent + folderCount + 1;

    if (selectedFolderIndex >= totalItems) selectedFolderIndex = totalItems - 1;

    int scrollStart = selectedFolderIndex - (maxVisible / 2);
    if (scrollStart < 0) scrollStart = 0;
    if (scrollStart + maxVisible > totalItems) scrollStart = max(0, totalItems - maxVisible);

    int y = startY;
    for (int i = 0; i < maxVisible && (scrollStart + i) < totalItems; ++i) {
        int idxGlobal = scrollStart + i;
        bool isSelected = (idxGlobal == selectedFolderIndex);

        bool isParentButton = hasParent && (idxGlobal == 0);
        bool isConfirmButton = (idxGlobal == totalItems - 1);

        String displayName;
        if (isParentButton) displayName = "..";
        else if (isConfirmButton) displayName = " > Select this folder";
        else {
            int folderIndex = idxGlobal - baseParent;
            if (folderIndex >= 0 && folderIndex < folderCount) {
                displayName = availableFolders[folderIndex];
                int lastSlash = displayName.lastIndexOf('/');
                if (lastSlash >= 0) displayName = displayName.substring(lastSlash + 1);
            }
        }

        uint16_t bg = isSelected ? (isConfirmButton ? RED : BLUE) : gray;
        uint16_t fg = isSelected ? WHITE : (isConfirmButton ? RED : GREEN);

        if (isSelected) sprite1.fillRoundRect(8, y - 1, 224, lineHeight + 2, 3, bg);
        sprite1.setTextColor(fg, bg);
        sprite1.drawString(displayName, 14, y + 1);
        y += lineHeight;
    }

    sprite1.setTextFont(0);
    sprite1.pushSprite(0, 0);
}

void drawPlayer() {
    if (graphSpeed == 0) {
        gray = grays[15];
        light = grays[11];
        sprite1.fillRect(0, 0, 240, 135, gray);
        sprite1.fillRect(4, 8, 130, 122, BLACK);
        sprite1.fillRect(129, 8, 5, 122, 0x0841);

        if (fileCount > 0) {
            sliderPos = map(selectedFileIndex, 0, max(1, fileCount - 1), 8, 110);
        } else {
            sliderPos = 8;
        }
        sprite1.fillRect(129, sliderPos, 5, 20, grays[2]);
        sprite1.fillRect(131, sliderPos + 4, 1, 12, grays[16]);

        sprite1.fillRect(4, 2, 50, 2, ORANGE);
        sprite1.fillRect(84, 2, 50, 2, ORANGE);
        sprite1.fillRect(190, 2, 45, 2, ORANGE);
        sprite1.fillRect(190, 6, 45, 3, grays[4]);
        
        sprite1.drawFastVLine(3, 9, 120, light);
        sprite1.drawFastVLine(134, 9, 120, light);
        sprite1.drawFastHLine(3, 129, 130, light);
        sprite1.drawFastHLine(0, 0, 240, light);
        sprite1.drawFastHLine(0, 134, 240, light);
        
        sprite1.fillRect(139, 0, 3, 135, BLACK);
        sprite1.fillRect(148, 14, 86, 42, BLACK);
        sprite1.fillRect(148, 59, 86, 16, BLACK);

        sprite1.fillTriangle(162, 18, 162, 26, 168, 22, GREEN);
        sprite1.fillRect(162, 30, 6, 6, RED);
        
        sprite1.drawFastVLine(143, 0, 135, light);
        sprite1.drawFastVLine(238, 0, 135, light);
        sprite1.drawFastVLine(138, 0, 135, light);
        sprite1.drawFastVLine(148, 14, 42, light);
        sprite1.drawFastHLine(148, 14, 86, light);

        for (int i = 0; i < 4; i++)
            sprite1.fillRoundRect(148 + (i * 22), 94, 18, 18, 3, grays[4]);

        sprite1.fillRect(220, 104, 8, 2, grays[13]);
        sprite1.fillRect(220, 108, 8, 2, grays[13]);
        sprite1.fillTriangle(228, 102, 228, 106, 231, 105, grays[13]);
        sprite1.fillTriangle(220, 106, 220, 110, 217, 109, grays[13]);
        
        if (!isStoped) {
            sprite1.fillRect(152, 104, 3, 6, grays[13]);
            sprite1.fillRect(157, 104, 3, 6, grays[13]);
        } else {
            sprite1.fillTriangle(156, 102, 156, 110, 160, 106, grays[13]);
        }

        const int32_t volBarX = 172;
        const int32_t volBarY = 82;
        const int32_t volBarWidth = 60;
        const int32_t volSliderWidth = 10;
        
        sprite1.fillRoundRect(volBarX, volBarY, volBarWidth, 3, 2, YELLOW);

        int volSliderX = volBarX + map(volume, 0, 21, 0, volBarWidth - volSliderWidth);
        sprite1.fillRoundRect(volSliderX, volBarY - 2, volSliderWidth, 8, 2, grays[2]);
        sprite1.fillRoundRect(volSliderX + 2, volBarY, 6, 4, 2, grays[10]);

        const int32_t brigBarX = 172;
        const int32_t brigBarY = 124;
        const int32_t brigBarWidth = 30;
        const int32_t brigSliderWidth = 10;
        
        sprite1.fillRoundRect(brigBarX, brigBarY, brigBarWidth, 3, 2, MAGENTA);

        int32_t brigSliderX = brigBarX + map(M5Cardputer.Display.getBrightness(), 0, 255, 0, brigBarWidth - brigSliderWidth);
        sprite1.fillRoundRect(brigSliderX, brigBarY - 2, brigSliderWidth, 8, 2, grays[2]);
        sprite1.fillRoundRect(brigSliderX + 2, brigBarY, 6, 4, 2, grays[10]);

        sprite1.drawRect(206, 119, 28, 12, GREEN);
        sprite1.fillRect(234, 122, 3, 6, GREEN);

        for (int i = 0; i < 14; i++) {
            if (isPlaying && !isStoped) {
                g[i] = random(1, 5); 
            } else {
                g[i] = 1; 
            }

            for (int j = 0; j < g[i]; j++) {
                sprite1.fillRect(172 + (i * 4), 50 - j * 3, 3, 2, grays[4]);
            }
        }

        sprite1.setTextFont(0);
        sprite1.setTextDatum(0);
        

        if (fileCount == 0) {
            sprite1.setTextColor(RED, BLACK);
            sprite1.drawString("No files found!", 8, 50);
        } else {
            static uint8_t lastPlaying  = 255;
            static uint8_t lastSelected = 255;

            bool playingChanged  = (currentFileIndex != lastPlaying);
            bool selectedChanged = (selectedFileIndex != lastSelected);

            if (playingChanged || selectedChanged) {
                if (playingChanged) {
                    if (fileCount <= VISIBLE_FILE_COUNT) {
                        viewStartIndex = 0;
                    } else {
                        int ideal = currentFileIndex - (VISIBLE_FILE_COUNT / 2);
                        viewStartIndex = max(0, min(fileCount - VISIBLE_FILE_COUNT, ideal));
                    }
                    selectedFileIndex = currentFileIndex;  // sync cursor to playing
                    lastPlaying = currentFileIndex;
                }

                if (selectedChanged) {
                    marqueePos = 0;
                    lastMarqueeUpdate = millis();  // start fresh
                }

                lastSelected = selectedFileIndex;
            }

            // Safety clamp
            if (viewStartIndex > fileCount - VISIBLE_FILE_COUNT) {
                viewStartIndex = fileCount - VISIBLE_FILE_COUNT;
            }

            int startIdx = viewStartIndex;

            sprite1.setFont(&fonts::lgfxJapanGothic_12);

            static unsigned long lastMarqueeUpdate = 0;
            const unsigned long MARQUEE_INTERVAL = 180;
            const int MARQUEE_SPEED = 2;
            const int LIST_LEFT = 8;
            const int LIST_WIDTH = 122;

            unsigned long now = millis();

            if (selectedFileIndex < fileCount && now - lastMarqueeUpdate >= MARQUEE_INTERVAL) {
                marqueePos -= MARQUEE_SPEED;
                lastMarqueeUpdate = now;
            }

            for (int i = 0; i < VISIBLE_FILE_COUNT && (startIdx + i) < fileCount; i++) {
                int idx = startIdx + i;

                bool isPlaying  = (idx == currentFileIndex);
                bool isSelected = (idx == selectedFileIndex);

                uint16_t textColor = isPlaying ? WHITE : (isSelected ? YELLOW : GREEN);
                sprite1.setTextColor(textColor, BLACK);

                String name = getFileName(idx);
                int y = 12 + (i * 12);   // top margin

                if (isSelected) {
                    sprite1.drawString(">", 2, y);
                    int nameWidth = name.length() * 7; 

                    if (nameWidth > LIST_WIDTH) {
                        marqueePos -= 1;
                        
                        if (marqueePos < -nameWidth) {
                            marqueePos = LIST_WIDTH;
                        }
                    } else {
                        marqueePos = 0;
                    }

                    sprite1.setClipRect(LIST_LEFT, y - 2, LIST_WIDTH, 14);
                    sprite1.drawString(name, 12 + marqueePos, y);
                    sprite1.clearClipRect();
                } else {
                    sprite1.setClipRect(LIST_LEFT, y - 2, LIST_WIDTH - 1, 14);

                    if (isPlaying && (i + viewStartIndex == playingFileIndex)) {
                        sprite1.drawString("*", 2, y); 
                        sprite1.drawString(name, 12, y);
                    } else {
                        sprite1.drawString(name, 8, y);
                    }

                    sprite1.clearClipRect();
                }
            }

            sprite1.setTextFont(0);
        }


        sprite1.setTextColor(grays[1], gray);
        sprite1.drawString("MP3 ADV", 150, 4);
        
        sprite1.setTextColor(grays[2], gray);
        sprite1.drawString("LIST", 58, 0);
        sprite1.setTextColor(grays[4], gray);
        sprite1.drawString("VOL", 150, 80);
        sprite1.setTextColor(grays[4], gray);
        sprite1.drawString("LIG", 150, 122);

        if (isPlaying) {
            sprite1.setTextColor(grays[8], BLACK);
            sprite1.drawString("P", 152, 18);
            sprite1.drawString("L", 152, 27);
            sprite1.drawString("A", 152, 36);
            sprite1.drawString("Y", 152, 45);
        } else {
            sprite1.setTextColor(TFT_YELLOW, BLACK);
            sprite1.drawString("S", 152, 18);
            sprite1.drawString("T", 152, 27);
            sprite1.drawString("O", 152, 36);
            sprite1.drawString("P", 152, 45);
        }

        sprite1.setTextColor(GREEN, BLACK);
        sprite1.setFont(&DSEG7_Classic_Mini_Regular_16);
        if (!isStoped) sprite1.drawString(getPlaybackTimeString(), 172, 18);
        sprite1.setTextFont(0);

        sprite1.setTextDatum(3);
        int newLevel = M5Cardputer.Power.getBatteryLevel();

        if (abs(newLevel - stableBat) > 1 || stableBat == 0) {
            stableBat = newLevel;
        }

        sprite1.drawString(String(stableBat) + "%", 220, 121);

        bool animActive = (pressedBtn != NONE) && (millis() - pressAnimStart < PRESS_ANIM_DURATION);

        uint16_t labelColor = BLACK;

        if (animActive) {
            // Quick flash: bright → softer highlight → normal
            unsigned long elapsed = millis() - pressAnimStart;
            if (elapsed < PRESS_ANIM_DURATION / 2) {
                labelColor = TFT_WHITE;           // strongest flash
            } else if (elapsed < PRESS_ANIM_DURATION) {
                labelColor = TFT_LIGHTGREY;       // brief afterglow
            }
        }

        sprite1.setTextColor(pressedBtn == BTN_A && animActive ? labelColor : BLACK, grays[4]);
        sprite1.drawString("A", 154, 96);

        sprite1.setTextColor(pressedBtn == BTN_P && animActive ? labelColor : BLACK, grays[4]);
        sprite1.drawString("P", 176, 96);

        sprite1.setTextColor(pressedBtn == BTN_N && animActive ? labelColor : BLACK, grays[4]);
        sprite1.drawString("N", 198, 96);

        sprite1.setTextColor(pressedBtn == BTN_R && animActive ? labelColor : BLACK, grays[4]);
        sprite1.drawString("R", 220, 96);

        if (!animActive) {
            pressedBtn = NONE;
        }

        sprite1.setTextColor(BLACK, grays[5]);
        sprite1.drawString(">>", 202, 103);
        sprite1.drawString("<<", 180, 103);

        sprite2.fillSprite(BLACK);
        sprite2.setFont(&fonts::lgfxJapanGothic_12);
        sprite2.setTextColor(GREEN, BLACK);
        if (!isStoped && fileCount > 0) {
            sprite2.drawString(getFileName(currentFileIndex), textPos, 4);
        }

        if (popupText != "" && millis() - popupStart < POPUP_DURATION) {
            sprite1.fillRoundRect(40, 90, 160, 30, 8, TFT_BLACK);
            sprite1.drawRoundRect(40, 90, 160, 30, 8, TFT_WHITE);
            
            sprite1.setTextColor(TFT_WHITE);
            sprite1.setTextFont(2);
            sprite1.setTextDatum(MC_DATUM);
            sprite1.drawString(popupText, 120, 105);
            
            sprite1.setTextDatum(TL_DATUM);
            sprite1.setTextFont(0);
        } else if (popupText != "" && millis() - popupStart >= POPUP_DURATION) {
            popupText = "";
        }

    if (isPlaying && !isStoped) {
        textPos -= 2;
        if (textPos < -200) { 
            textPos = 120;
        }
    }
        
        sprite2.pushSprite(&sprite1, 148, 59);
        sprite1.pushSprite(0, 0);
    }
    
    graphSpeed++;
    if (graphSpeed == 4) graphSpeed = 0;
}

void draw() {
    checkScreenTimeout();
    if (currentUIState == UI_FOLDER_SELECT) drawFolderSelect();
    else drawPlayer();
}

void handleKeyPress(char key) {
    resetActivityTimer();
    
    if (key == 'k') {
        uint8_t b = M5Cardputer.Display.getBrightness();
        b = (b > brightnessStep) ? b - brightnessStep : 0;
        M5Cardputer.Display.setBrightness(b);
    } else if (key == 'l') {
        uint8_t b = M5Cardputer.Display.getBrightness();
        b = (b < 255 - brightnessStep) ? b + brightnessStep : 255;
        M5Cardputer.Display.setBrightness(b);
    } else if (key == 'c') {
        changeVolume(-volumeStep);
    } else if (key == 'v') {
        changeVolume(volumeStep);
    } else if (key == 't') {
        screenTimeoutEnabled = !screenTimeoutEnabled;
        popupText = "Screen Timeout: " + String(screenTimeoutEnabled ? "ON" : "OFF");
        popupStart = millis();
    }

    if (currentUIState == UI_POPUP_MENU) {
        if (key == ';') popupMenuIndex = (popupMenuIndex <= 0) ? 2 : popupMenuIndex - 1;
        else if (key == '.') popupMenuIndex = (popupMenuIndex >= 2) ? 0 : popupMenuIndex + 1;
        else if (key == '\n') { // Enter
            if (popupMenuIndex == 0) {
                currentUIState = UI_FOLDER_SELECT;
            } else if (popupMenuIndex == 1) {
                isMassStorageMode = true; 
                currentUIState = UI_PLAYER; // Move away from menu state
            } else if (popupMenuIndex == 2) {
                currentUIState = UI_SETTINGS;
            }
        }
        marqueePos = 0;
        return;
    } else if (currentUIState == UI_SETTINGS) {
        if (key == '\n') {
            defaultBootFolder = currentFolder;
            saveSettings();
            popupText = "Default Saved!";
            popupStart = millis();
        }
        return;
    } else if (currentUIState == UI_FOLDER_SELECT) {
        const bool hasParent = (currentFolder != "/");
        const int baseParent = hasParent ? 1 : 0;
        const int totalItems = baseParent + folderCount + 1;
        const int confirmButtonIndex = totalItems - 1;

        if (key == ';') {
            selectedFolderIndex--;
            if (selectedFolderIndex < 0) selectedFolderIndex = totalItems - 1;
            marqueePos = 0;
        } else if (key == '.') {
            selectedFolderIndex++;
            if (selectedFolderIndex >= totalItems) selectedFolderIndex = 0;
            marqueePos = 0;
        } else if (key == '\n') {
            if (selectedFolderIndex == confirmButtonIndex) {
                scanDirectory(currentFolder);
                currentFileIndex = 0;
                currentUIState = UI_PLAYER;
                selectedFileIndex = currentFileIndex;               // ← already here, good
                if (selectedFileIndex >= fileCount) selectedFileIndex = 0;
                if (fileCount <= VISIBLE_FILE_COUNT) viewStartIndex = 0;
                else viewStartIndex = max(0, (int)currentFileIndex - (VISIBLE_FILE_COUNT / 2));
                isPlaying = true;
                isStoped = false;
                textPos = 90;
                trackStartMillis = millis();
                playbackTime = 0;
            } else if (hasParent && selectedFolderIndex == 0) {
                int lastSlash = currentFolder.lastIndexOf('/');
                currentFolder = (lastSlash > 0) ? currentFolder.substring(0, lastSlash) : "/";
                scanDirectory(currentFolder);
                selectedFolderIndex = 0;
            } else {
                int folderIndex = selectedFolderIndex - baseParent;
                if (folderIndex >= 0 && folderIndex < folderCount) {
                    currentFolder = availableFolders[folderIndex];
                    scanDirectory(currentFolder);
                    selectedFolderIndex = 0;
                }
            }
        } else if (key == '`' || key == '\b') {
            if (currentFolder != "/") {
                int lastSlash = currentFolder.lastIndexOf('/');
                currentFolder = (lastSlash > 0) ? currentFolder.substring(0, lastSlash) : "/";
                scanDirectory(currentFolder);
                selectedFolderIndex = 0;
            }
        }
    } else {
        if (key == '`' || key == '\b') {
            audio.stopSong();
            playbackTime = 0;
            isPlaying = false;
            isStoped = true;
            currentUIState = UI_FOLDER_SELECT;
            currentFolder = "/";
            scanDirectory(currentFolder);
            selectedFolderIndex = 0;
            resetActivityTimer();               // force wake + reset timer
        } else if (key == 'a' || key == ' ') {
            if (isPlaying) {
                playbackTime = millis() - trackStartMillis; 
                isPlaying = false;
            } else {
                trackStartMillis = millis() - playbackTime;
                isPlaying = true;
            }
            
            pressedBtn = BTN_A;
            pressAnimStart = millis();

        } else if (key == 'n' || key == '/' || key == 'p' || key == ',' || key == 'r' || key == '\n') {
            if (fileCount == 0) return;
            if (key == 'n' || key == '/') {
                currentFileIndex = (currentFileIndex + 1) % fileCount;
                pressedBtn = BTN_N;
            } else if (key == 'p' || key == ',') {
                currentFileIndex = (currentFileIndex == 0) ? (fileCount - 1) : (currentFileIndex - 1);
                pressedBtn = BTN_P;
            } else if (key == 'r'){
                currentFileIndex = random(0, fileCount);
                pressedBtn = BTN_R; 
            }
            else if (key == '\n') {
                if (selectedFileIndex < fileCount) currentFileIndex = selectedFileIndex;
                playingFileIndex = selectedFileIndex;
                pressedBtn = BTN_A;
                
            }
            pressAnimStart = millis();
            trackStartMillis = millis();
            playbackTime = 0;
            isPlaying = true;
            isStoped = false;
            textPos = 0;
            nextTrackRequest = true;
        } else if (key == ';' || key == '.') {
            if (fileCount > 0) {
                if (key == ';') selectedFileIndex = (selectedFileIndex == 0) ? (fileCount - 1) : (selectedFileIndex - 1);
                else selectedFileIndex = (selectedFileIndex + 1) % fileCount;
                marqueePos = 0;
                if (fileCount <= VISIBLE_FILE_COUNT) viewStartIndex = 0;
                else {
                    if (selectedFileIndex < viewStartIndex) viewStartIndex = selectedFileIndex;
                    else if (selectedFileIndex >= viewStartIndex + VISIBLE_FILE_COUNT) viewStartIndex = selectedFileIndex - VISIBLE_FILE_COUNT + 1;
                }
            }
        }
    }
}