#include "ui_manager.h"
#include "font.h"
#include "file_manager.h"
#include "audio_config.h"

UIState currentUIState = UI_FOLDER_SELECT;
M5Canvas sprite1(&M5Cardputer.Display);
M5Canvas sprite2(&M5Cardputer.Display);
M5Canvas overlaySprite(&M5Cardputer.Display);

bool nextTrackRequest = false;
const uint8_t VISIBLE_FILE_COUNT = 10;
constexpr unsigned long HOLD_DELAY = 1500;
constexpr int SCROLL_SPEED = 1;

// === DODANE: Zmienne do wygaszania ekranu ===
constexpr unsigned long SCREEN_DIM_TIMEOUT = 30000; // 30 sekund (możesz zmienić)
constexpr uint8_t DIMMED_BRIGHTNESS = 16; // Jasność po wygaszeniu
unsigned long lastActivityTime = 0;
uint8_t savedBrightness = 128;
bool isScreenDimmed = false;
// ============================================

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
static uint8_t selectedFolderIndex = 0;
static uint8_t selectedFileIndex = 0;
static uint16_t viewStartIndex = 0;

void initUI() {
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(brightnessStep*2);
    savedBrightness = brightnessStep*2; // Zapisz początkową jasność
    sprite1.createSprite(240, 135);
    sprite2.createSprite(86, 16);

    uint8_t co = 214;
    for (uint8_t i = 0; i < 18; i++) {
        grays[i] = M5Cardputer.Display.color565(co, co, co + 40);
        co = co - 13;
    }
    
    lastActivityTime = millis(); // Inicjalizuj czas aktywności
}

// === DODANE: Funkcja resetująca timer aktywności ===
void resetActivityTimer() {
    lastActivityTime = millis();
    
    // Jeśli ekran był wygaszony, przywróć jasność
    if (isScreenDimmed) {
        M5Cardputer.Display.setBrightness(savedBrightness);
        isScreenDimmed = false;
    }
}
// ==================================================

// === DODANE: Funkcja sprawdzająca timeout i wygaszająca ekran ===
void checkScreenTimeout() {
    if (isScreenDimmed) return; // Już wygaszony
    
    unsigned long now = millis();
    if (now - lastActivityTime > SCREEN_DIM_TIMEOUT) {
        savedBrightness = M5Cardputer.Display.getBrightness();
        M5Cardputer.Display.setBrightness(DIMMED_BRIGHTNESS);
        isScreenDimmed = true;
    }
}
// ===============================================================

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

    sprite1.fillRect(0, 0, 240, 135, gray);
    sprite1.drawRoundRect(0, 0, 240, 135, 4, light);

    sprite1.setTextFont(1);
    sprite1.setTextColor(ORANGE, gray);
    sprite1.setTextDatum(4);
    sprite1.drawString("Select Folder", 75, 10);

    sprite1.fillRoundRect(10, 22, 220, 20, 4, grays[12]);
    sprite1.setTextFont(1);
    sprite1.setTextColor(WHITE, grays[12]);
    sprite1.setTextDatum(0);
    sprite1.drawString(currentFolder, 16, 27);

    sprite1.setTextFont(0);
    sprite1.setTextColor(GREEN, gray);
    sprite1.setTextDatum(2);
    sprite1.drawString("[Audios: " + String(fileCount) + "]", 230, 12);

    sprite1.setTextFont(0);
    sprite1.setTextDatum(0);
    sprite1.setTextColor(grays[5], gray);
    sprite1.drawString("[;]/[.]-Nav   [ok]-Open   [Esc]-Back", 10, 122);

    sprite1.setTextFont(1);

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
                continue;
            }
        }

        uint16_t bg = isSelected ? (isConfirmButton ? RED : BLUE) : gray;
        uint16_t fg = isSelected ? WHITE : (isConfirmButton ? RED : GREEN);

        if (isSelected)
            sprite1.fillRoundRect(8, y - 1, 224, lineHeight + 2, 3, bg);
        sprite1.setTextColor(fg, bg);

        sprite1.drawString(displayName, 14, y + 1);
        y += lineHeight;
    }

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
            if (!isStoped)
                g[i] = random(1, 5);
            for (int j = 0; j < g[i]; j++)
                sprite1.fillRect(172 + (i * 4), 50 - j * 3, 3, 2, grays[4]);
        }

        sprite1.setTextFont(0);
        sprite1.setTextDatum(0);
        
        if (fileCount == 0) {
            sprite1.setTextColor(RED, BLACK);
            sprite1.drawString("No files found!", 8, 50);
        } else {
            if (fileCount <= VISIBLE_FILE_COUNT) {
                viewStartIndex = 0;
            } else if (viewStartIndex > fileCount - VISIBLE_FILE_COUNT) {
                viewStartIndex = fileCount - VISIBLE_FILE_COUNT;
            }
            int startIdx = viewStartIndex;
            for (int i = 0; i < 10 && (startIdx + i) < fileCount; i++) {
                int idx = startIdx + i;
                bool isNow = (idx == currentFileIndex);
                bool isCursor = (idx == selectedFileIndex);

                if (isNow) {
                    sprite1.setTextColor(WHITE, BLACK);
                } else if (isCursor) {
                    sprite1.setTextColor(YELLOW, BLACK);
                } else {
                    sprite1.setTextColor(GREEN, BLACK);
                }

                if (isCursor) {
                    sprite1.drawString(">", 2, 10 + (i * 12));
                    sprite1.drawString(getFileName(idx).substring(0, 20), 12, 10 + (i * 12));
                } else {
                    sprite1.drawString(getFileName(idx).substring(0, 20), 8, 10 + (i * 12));
                }
            }
        }

        sprite1.setTextColor(grays[1], gray);
        sprite1.drawString("MP3 Adv", 150, 4);
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
            sprite1.setTextColor(grays[8], BLACK);
            sprite1.drawString("S", 152, 18);
            sprite1.drawString("T", 152, 27);
            sprite1.drawString("O", 152, 36);
            sprite1.drawString("P", 152, 45);
        }

        sprite1.setTextColor(GREEN, BLACK);
        sprite1.setFont(&DSEG7_Classic_Mini_Regular_16);
        if (!isStoped)
            sprite1.drawString(getPlaybackTimeString(), 172, 18);
        sprite1.setTextFont(0);

        sprite1.setTextDatum(3);
        sprite1.drawString(String(M5Cardputer.Power.getBatteryLevel()) + "%", 220, 121);

        sprite1.setTextColor(BLACK, grays[4]);
        sprite1.drawString("R", 220, 96);
        sprite1.drawString("N", 198, 96);
        sprite1.drawString("P", 176, 96);
        sprite1.drawString("A", 154, 96);
        sprite1.setTextColor(BLACK, grays[5]);
        sprite1.drawString(">>", 202, 103);
        sprite1.drawString("<<", 180, 103);

        sprite2.fillSprite(BLACK);
        sprite2.setTextColor(GREEN, BLACK);
        if (!isStoped && fileCount > 0) {
            sprite2.drawString(getFileName(currentFileIndex), textPos, 4);
        }
        textPos -= 2;
        if (textPos < -300) textPos = 90;
        
        sprite2.pushSprite(&sprite1, 148, 59);
        sprite1.pushSprite(0, 0);
    }
    
    graphSpeed++;
    if (graphSpeed == 4) graphSpeed = 0;
}

void draw() {
    checkScreenTimeout(); // Sprawdź czy wygasić ekran
    
    if (currentUIState == UI_FOLDER_SELECT) {
        drawFolderSelect();
    } else {
        drawPlayer();
    }
}

void handleKeyPress(char key) {
    resetActivityTimer(); // Reset timera przy każdym naciśnięciu klawisza
    
    if (key == 'c') {
        changeVolume(-volumeStep);
        Serial.printf("Volume: %d\n", volume);
    } else if (key == 'v') {
        changeVolume(volumeStep);
        Serial.printf("Volume: %d\n", volume);
    } else if (key == 'k') {
        savedBrightness = M5Cardputer.Display.getBrightness() - brightnessStep;
        M5Cardputer.Display.setBrightness(savedBrightness);
        Serial.printf("Brightness: %d\n", savedBrightness);
    }  else if (key == 'l') {
        savedBrightness = M5Cardputer.Display.getBrightness() + brightnessStep;
        M5Cardputer.Display.setBrightness(savedBrightness);
        Serial.printf("Brightness: %d\n", savedBrightness);
    }
    if (currentUIState == UI_FOLDER_SELECT) {
        const bool hasParent = (currentFolder != "/");
        const int baseParent = hasParent ? 1 : 0;
        const int totalItems = baseParent + folderCount + 1;
        const int confirmButtonIndex = totalItems - 1;

        if (key == ';') {
            selectedFolderIndex--;
            if (selectedFolderIndex < 0) selectedFolderIndex = totalItems - 1;
        } else if (key == '.') {
            selectedFolderIndex++;
            if (selectedFolderIndex >= totalItems) selectedFolderIndex = 0;
        } else if (key == '\n') {
            if (selectedFolderIndex == confirmButtonIndex) {
                scanDirectory(currentFolder);
                currentFileIndex = 0;
                currentUIState = UI_PLAYER;
                selectedFileIndex = currentFileIndex;
                if (selectedFileIndex >= fileCount) selectedFileIndex = 0;
                if (fileCount <= VISIBLE_FILE_COUNT) {
                    viewStartIndex = 0;
                } else {
                    viewStartIndex = max(0, (int)currentFileIndex - (VISIBLE_FILE_COUNT / 2));
                }
                isPlaying = true;
                isStoped = false;
                textPos = 90;
                trackStartMillis = millis();
                playbackTime = 0;
            }
            else if (hasParent && selectedFolderIndex == 0) {
                int lastSlash = currentFolder.lastIndexOf('/');
                currentFolder = (lastSlash > 0) ? currentFolder.substring(0, lastSlash) : "/";
                scanDirectory(currentFolder);
                selectedFolderIndex = 0;
            }
            else {
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
            trackStartMillis = millis();
            playbackTime = 0;
            isPlaying = false;
            isStoped = true;
            currentUIState = UI_FOLDER_SELECT;
            selectedFolderIndex = 0;
            scanDirectory("/");
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
        } else if (key == 'n' || key == '/' || key == 'p' || key == ',' || key == 'r' || key == '\n') {
            if (fileCount == 0) {
                return;
            } else if (key == 'n' || key == '/') {
                currentFileIndex = (currentFileIndex + 1) % fileCount;
            } else if (key == 'p' || key == ',') {
                currentFileIndex = (currentFileIndex == 0) ? (fileCount - 1) : (currentFileIndex - 1);
            } else if (key == 'r') {
                currentFileIndex = random(0, fileCount);
            } else if (key == '\n') {
                if (selectedFileIndex < fileCount) currentFileIndex = selectedFileIndex;
            }
            trackStartMillis = millis();
            playbackTime = 0;
            isPlaying = true;
            isStoped = false;
            textPos = 90;
            nextTrackRequest = true;
        } else if (key == ';' || key == '.') {
            if (fileCount > 0) {
                if (key == ';') {
                    selectedFileIndex = (selectedFileIndex == 0) ? (fileCount - 1) : (selectedFileIndex - 1);
                } else {
                    selectedFileIndex = (selectedFileIndex + 1) % fileCount;
                }

                if (fileCount <= VISIBLE_FILE_COUNT) {
                    viewStartIndex = 0;
                } else {
                    if (selectedFileIndex < viewStartIndex) {
                        viewStartIndex = selectedFileIndex;
                    } else if (selectedFileIndex >= viewStartIndex + VISIBLE_FILE_COUNT) {
                        viewStartIndex = selectedFileIndex - VISIBLE_FILE_COUNT + 1;
                    }
                }
            }
        }
    }
}
