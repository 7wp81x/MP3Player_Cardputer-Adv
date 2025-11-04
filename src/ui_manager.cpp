#include "ui_manager.h"
#include "font.h"
#include "battery.h"
#include "file_manager.h"
#include "audio_config.h"

UIState currentUIState = UI_FOLDER_SELECT;
M5Canvas sprite(&M5Cardputer.Display);
M5Canvas spr(&M5Cardputer.Display);

bool nextTrackRequest = false;

constexpr unsigned long HOLD_DELAY = 1500;
constexpr int SCROLL_SPEED = 1;

uint8_t sliderPos = 0;
int16_t textPos = 90;
uint8_t graphSpeed = 0;
uint8_t g[14] = {0};
unsigned short grays[18];
unsigned short gray;
unsigned short light;
unsigned long trackStartMillis = 0;
unsigned long playbackTime = 0; 

static uint8_t volumeStep = 4;
static uint8_t brightnessStep = 64;
static bool inFolder = false;
static short int selectedFolderIndex = 0;
static short int folderConfirmIndex = 0;
static short int selectedFileIndex = 0;

void initUI() {
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(brightnessStep*2);
    sprite.createSprite(240, 135);
    spr.createSprite(86, 16);

    int co = 214;
    for (int i = 0; i < 18; i++) {
        grays[i] = M5Cardputer.Display.color565(co, co, co + 40);
        co = co - 13;
    }

    trackStartMillis = millis();
    
    Serial.println("UI initialized");
}

String getPlaybackTimeString() {
    unsigned long elapsed = playbackTime;
    if (isPlaying && !isStoped) elapsed = millis() - trackStartMillis;
    
    unsigned int seconds = (elapsed / 1000) % 60;
    unsigned int minutes = (elapsed / 1000) / 60;

    char buf[6];
    sprintf(buf, "%02u:%02u", minutes, seconds);
    return String(buf);
}

struct MarqueeState {
    unsigned long startTime = 0;
    int offset = 0; 
    bool active = false;
};
MarqueeState marquee;

void updateMarquee(bool active, const String& text) {
    unsigned long now = millis();

    if (active) {
        if (!marquee.active) {
            marquee.startTime = now;
            marquee.offset = 0;
            marquee.active = true;
        } else if (now - marquee.startTime > HOLD_DELAY) {
            marquee.offset += SCROLL_SPEED;
            if (marquee.offset > text.length() * 6) {
                marquee.offset = 0;
            }
        }
    } else {
        marquee.active = false;
        marquee.offset = 0;
    }
}

void drawFolderSelect() {
    gray = grays[15];
    light = grays[10];

    sprite.fillRect(0, 0, 240, 135, gray);
    sprite.drawRoundRect(0, 0, 240, 135, 4, light);

    sprite.setTextFont(1);
    sprite.setTextColor(ORANGE, gray);
    sprite.setTextDatum(4);
    sprite.drawString("Select Folder", 120, 10);

    sprite.fillRoundRect(10, 22, 220, 20, 4, grays[12]);
    sprite.setTextFont(1);
    sprite.setTextColor(WHITE, grays[12]);
    sprite.setTextDatum(0);
    sprite.drawString(currentFolder, 16, 27);

    sprite.setTextFont(0);
    sprite.setTextDatum(0);
    sprite.setTextColor(grays[5], gray);
    sprite.drawString("[;]/[.]-Nav  [ok]-Open  [Esc]-Back", 10, 122);

    sprite.setTextFont(1);

    const int startY = 48;
    const int lineHeight = 14;
    const int maxVisible = 5;

    const bool hasParent = (currentFolder != "/");
    const int baseParent = hasParent ? 1 : 0;
    const int totalItems = baseParent + folderCount + 1;

    if (selectedFolderIndex < 0) selectedFolderIndex = 0;
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
        bool isFolderItem = !isParentButton && !isConfirmButton;

        String displayName;
        if (isParentButton) {
            displayName = "..";
        } else if (isConfirmButton) {
            displayName = " > Select this folder";
        } else {
            int folderIndex = idxGlobal - baseParent;
            if (folderIndex >= 0 && folderIndex < folderCount) {
                displayName = availableFolders[folderIndex];
                int lastSlash = displayName.lastIndexOf('/');
                if (lastSlash >= 0) displayName = displayName.substring(lastSlash + 1);
            } else {
                displayName = "(?)";
            }
        }

        if (isSelected) {
            uint16_t bg = isConfirmButton ? RED : BLUE;
            
            const int boxWidth = 224;
            const int boxY = y - 1;
            const int boxHeight = lineHeight + 2;

            sprite.fillRoundRect(8, boxY, boxWidth, boxHeight, 3, bg);
            sprite.setTextColor(WHITE, bg);
        } else {
            uint16_t fg = isConfirmButton ? RED : GREEN;
            sprite.setTextColor(fg, gray);
        }

        bool cursorOnItem = isSelected;
        updateMarquee(cursorOnItem, displayName);
        int drawX = 14 - marquee.offset;
        int textY = y + 1;

        sprite.drawString(displayName, drawX, textY);

        y += lineHeight;
    }

    sprite.pushSprite(0, 0);
}


void drawPlayer() {
    if (graphSpeed == 0) {
        gray = grays[15];
        light = grays[11];
        sprite.fillRect(0, 0, 240, 135, gray);
        sprite.fillRect(4, 8, 130, 122, BLACK);
        sprite.fillRect(129, 8, 5, 122, 0x0841);

        if (fileCount > 0) {
            sliderPos = map(currentFileIndex, 0, max(1, fileCount - 1), 8, 110);
        } else {
            sliderPos = 8;
        }
        sprite.fillRect(129, sliderPos, 5, 20, grays[2]);
        sprite.fillRect(131, sliderPos + 4, 1, 12, grays[16]);

        sprite.fillRect(4, 2, 50, 2, ORANGE);
        sprite.fillRect(84, 2, 50, 2, ORANGE);
        sprite.fillRect(190, 2, 45, 2, ORANGE);
        sprite.fillRect(190, 6, 45, 3, grays[4]);
        
        sprite.drawFastVLine(3, 9, 120, light);
        sprite.drawFastVLine(134, 9, 120, light);
        sprite.drawFastHLine(3, 129, 130, light);
        sprite.drawFastHLine(0, 0, 240, light);
        sprite.drawFastHLine(0, 134, 240, light);
        
        sprite.fillRect(139, 0, 3, 135, BLACK);
        sprite.fillRect(148, 14, 86, 42, BLACK);
        sprite.fillRect(148, 59, 86, 16, BLACK);

        sprite.fillTriangle(162, 18, 162, 26, 168, 22, GREEN);
        sprite.fillRect(162, 30, 6, 6, RED);
        
        sprite.drawFastVLine(143, 0, 135, light);
        sprite.drawFastVLine(238, 0, 135, light);
        sprite.drawFastVLine(138, 0, 135, light);
        sprite.drawFastVLine(148, 14, 42, light);
        sprite.drawFastHLine(148, 14, 86, light);

        for (int i = 0; i < 4; i++)
            sprite.fillRoundRect(148 + (i * 22), 94, 18, 18, 3, grays[4]);

        sprite.fillRect(220, 104, 8, 2, grays[13]);
        sprite.fillRect(220, 108, 8, 2, grays[13]);
        sprite.fillTriangle(228, 102, 228, 106, 231, 105, grays[13]);
        sprite.fillTriangle(220, 106, 220, 110, 217, 109, grays[13]);
        
        if (!isStoped) {
            sprite.fillRect(152, 104, 3, 6, grays[13]);
            sprite.fillRect(157, 104, 3, 6, grays[13]);
        } else {
            sprite.fillTriangle(156, 102, 156, 110, 160, 106, grays[13]);
        }

        sprite.fillRoundRect(172, 82, 60, 3, 2, YELLOW); // 60 - width
        sprite.fillRoundRect(155 + ((volume * (60 - 10)) / 20), 80, 10, 8, 2, grays[2]);
        sprite.fillRoundRect(157 + ((volume * (60 - 10)) / 20), 82, 6, 4, 2, grays[10]);

        sprite.fillRoundRect(172, 124, 30, 3, 2, MAGENTA); // 30 - width
        sprite.fillRoundRect(172 + (M5Cardputer.Display.getBrightness() * 30) / 255, 122, 10, 8, 2, grays[2]);
        sprite.fillRoundRect(174 + (M5Cardputer.Display.getBrightness() * 30) / 255, 124, 6, 4, 2, grays[10]);

        sprite.drawRect(206, 119, 28, 12, GREEN);
        sprite.fillRect(234, 122, 3, 6, GREEN);

        for (int i = 0; i < 14; i++) {
            if (!isStoped)
                g[i] = random(1, 5);
            for (int j = 0; j < g[i]; j++)
                sprite.fillRect(172 + (i * 4), 50 - j * 3, 3, 2, grays[4]);
        }

        sprite.setTextFont(0);
        sprite.setTextDatum(0);
        
        if (fileCount == 0) {
            sprite.setTextColor(RED, BLACK);
            sprite.drawString("No files found!", 8, 50);
        } else {
            int startIdx = max(0, currentFileIndex - 5);
            for (int i = 0; i < 10 && (startIdx + i) < fileCount; i++) {
                int idx = startIdx + i;
                if (idx == currentFileIndex) {
                    sprite.setTextColor(WHITE, BLACK);
                } else {
                    sprite.setTextColor(GREEN, BLACK);
                }
                sprite.drawString(getFileName(idx).substring(0, 20), 8, 10 + (i * 12));
            }
        }

        sprite.setTextColor(grays[1], gray);
        sprite.drawString("WINAMP", 150, 4);
        sprite.setTextColor(grays[2], gray);
        sprite.drawString("LIST", 58, 0);
        sprite.setTextColor(grays[4], gray);
        sprite.drawString("VOL", 150, 80);
        sprite.drawString("LIG", 150, 122);

        if (isPlaying) {
            sprite.setTextColor(grays[8], BLACK);
            sprite.drawString("P", 152, 18);
            sprite.drawString("L", 152, 27);
            sprite.drawString("A", 152, 36);
            sprite.drawString("Y", 152, 45);
        } else {
            sprite.setTextColor(grays[8], BLACK);
            sprite.drawString("S", 152, 18);
            sprite.drawString("T", 152, 27);
            sprite.drawString("O", 152, 36);
            sprite.drawString("P", 152, 45);
        }

        sprite.setTextColor(GREEN, BLACK);
        sprite.setFont(&DSEG7_Classic_Mini_Regular_16);
        if (!isStoped)
            sprite.drawString(getPlaybackTimeString(), 172, 18);
        sprite.setTextFont(0);

        int percent = getBatteryPercent();
        sprite.setTextDatum(3);
        sprite.drawString(String(percent) + "%", 220, 121);

        sprite.setTextColor(BLACK, grays[4]);
        sprite.drawString("B", 220, 96);
        sprite.drawString("N", 198, 96);
        sprite.drawString("P", 176, 96);
        sprite.drawString("A", 154, 96);
        sprite.setTextColor(BLACK, grays[5]);
        sprite.drawString(">>", 202, 103);
        sprite.drawString("<<", 180, 103);

        spr.fillSprite(BLACK);
        spr.setTextColor(GREEN, BLACK);
        if (!isStoped && fileCount > 0) {
            spr.drawString(getFileName(currentFileIndex), textPos, 4);
        }
        textPos -= 2;
        if (textPos < -300) textPos = 90;
        
        spr.pushSprite(&sprite, 148, 59);
        sprite.pushSprite(0, 0);
    }
    
    graphSpeed++;
    if (graphSpeed == 4) graphSpeed = 0;
}

void drawScanOverlay() {
    sprite.fillRoundRect(40, 40, 160, 55, 8, BLACK);
    sprite.drawRoundRect(40, 40, 160, 55, 8, YELLOW);

    sprite.setTextColor(WHITE, BLACK);
    sprite.setTextFont(1);
    sprite.setTextDatum(4);

    sprite.drawString("Scanning folders...", 120, 55);

    int barWidth = map(scanProgress, 0, max<unsigned short int>(1, scanTotal), 0, 120);
    sprite.drawRoundRect(60, 70, 120, 10, 3, NAVY);
    sprite.fillRoundRect(60, 70, barWidth, 10, 3, GREEN);
}

void draw() {
        if (currentUIState == UI_FOLDER_SELECT) {
            drawFolderSelect();
            return;
        } else drawPlayer();

        if (isScanning) drawScanOverlay();
    }

void handleKeyPress(char key) {
    if (currentUIState == UI_FOLDER_SELECT) {
        int maxIndex = folderCount;
        if (key == ';') {
            selectedFolderIndex--;
            if (selectedFolderIndex < 0) selectedFolderIndex = maxIndex;
        } else if (key == '.') {
            selectedFolderIndex++;
            if (selectedFolderIndex > maxIndex) selectedFolderIndex = 0;
        } else if (key == '\n') {
            if (selectedFolderIndex == folderCount) {
                listAudioFiles(currentFolder);
                currentFileIndex = 0;
                currentUIState = UI_PLAYER;
                isPlaying = true;
                isStoped = false;
                textPos = 90;
                trackStartMillis = millis();
                playbackTime = 0;
            }
            else if (selectedFolderIndex == 0 && currentFolder != "/") {
                int lastSlash = currentFolder.lastIndexOf('/');
                currentFolder = (lastSlash > 0) ? currentFolder.substring(0, lastSlash) : "/";
                scanFolders(currentFolder);
                selectedFolderIndex = 0;
            }
            else {
                currentFolder = availableFolders[selectedFolderIndex];
                scanFolders(currentFolder);
                selectedFolderIndex = 0;
            }
        } else if (key == '`' || key == '\b') {
            if (currentFolder != "/") {
                int lastSlash = currentFolder.lastIndexOf('/');
                currentFolder = (lastSlash > 0) ? currentFolder.substring(0, lastSlash) : "/";
                scanFolders(currentFolder);
                selectedFolderIndex = 0;
            }
        }
    }
    else {
    if (key == '`' || key == '\b') {
        audio.stopSong();
        trackStartMillis = millis();
        playbackTime = 0;
        isPlaying = false;
        isStoped = true;
        currentUIState = UI_FOLDER_SELECT;
        selectedFolderIndex = 0;
        scanFolders("/");
    } else if (key == 'a' || key == ' ') {
        if (isPlaying && !isStoped) {
            playbackTime = millis() - trackStartMillis;
            isPlaying = false;
            isStoped = true;
        } else {
            trackStartMillis = millis() - playbackTime;
            isPlaying = true;
            isStoped = false;
        }
    } else if (key == 'c') {
        changeVolume(-volumeStep);
    } else if (key == 'v') {
        changeVolume(volumeStep);
    } else if (key == 'k') {
        M5Cardputer.Display.setBrightness(M5Cardputer.Display.getBrightness() - brightnessStep);
    }  else if (key == 'l') {
        M5Cardputer.Display.setBrightness(M5Cardputer.Display.getBrightness() + brightnessStep);
    } else if (key == 'n' || key == 'p' || key == 'r' || key == '\n') {
        if (key == 'n') {
            currentFileIndex++;
            if (currentFileIndex >= fileCount) currentFileIndex = 0;
        } else if (key == 'p') {
            currentFileIndex--;
            if (currentFileIndex < 0) currentFileIndex = fileCount - 1;
        } else if (key == 'r') {
            currentFileIndex = random(0, fileCount);
        } else if (key == '\n') {
            currentFileIndex = selectedFileIndex;
        }
        trackStartMillis = millis();
        playbackTime = 0;
        isPlaying = true;
        isStoped = false;
        textPos = 90;
        nextTrackRequest = true;
    } else if (key == ';') {
        selectedFileIndex--;
        if (selectedFileIndex < 0) selectedFileIndex = fileCount - 1;
    } else if (key == '.') {
        selectedFileIndex++;
        if (selectedFileIndex >= fileCount) selectedFileIndex = 0;  
    }
}
}