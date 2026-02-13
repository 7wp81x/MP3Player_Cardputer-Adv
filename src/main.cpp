#include <Arduino.h>
#include "M5Cardputer.h"
#include "audio_config.h"
#include "file_manager.h"
#include "ui_manager.h"
#include "settings.h"
#include "utility/Keyboard/KeyboardReader/TCA8418.h"
#include "TJpg_Decoder.h"

TaskHandle_t handleUITask = NULL;
TaskHandle_t handleAudioTask = NULL;
volatile bool usbConnected = false;
bool exitPressed = false;

void showLoadingDots(const char* message = "Initializing...") {
    uint16_t accent = TFT_SKYBLUE;
    uint16_t dimGray = 0x3186;
    float angle = 0;

    unsigned long t0 = millis();
    while (millis() - t0 < 3000) { 
        sprite1.fillSprite(TFT_BLACK);
        
        int cx = 120;
        int cy = 55;
        
        sprite1.drawArc(cx, cy, 22, 21, 0, 360, dimGray);
        sprite1.drawArc(cx, cy, 22, 21, (int)angle, (int)angle + 90, accent);
        
        sprite1.setTextColor(TFT_WHITE);
        sprite1.setFont(&fonts::FreeSansBold9pt7b);
        sprite1.setTextDatum(MC_DATUM);
        sprite1.drawString(message, cx, 100);

        sprite1.setFont(&fonts::Font0);
        sprite1.setTextColor(0x7BEF);
        sprite1.setTextSize(1);
        
        String loadingStatus = "Loading music";
        for(int i=0; i < (millis()/400 % 4); i++) loadingStatus += ".";
        sprite1.drawString(loadingStatus, cx, 118);

        sprite1.pushSprite(0, 0);

        angle += 9.0f; 
        if (angle >= 360) angle -= 360;
        
        delay(16);
    }
}

void handlePopupSelection() {
    switch (popupMenuIndex) {
        case 0:
            currentUIState = UI_PLAYER;
            break;
            
        case 1:
            currentUIState = UI_FOLDER_SELECT;
            currentFolder = "/";
            scanDirectory(currentFolder);
            selectedFolderIndex = 0;
            break;
            
        case 2:
            if (!isMassStorageMode) {
                audio.stopSong();
                isPlaying = false;
                nextTrackRequest = false;
                if (startMassStorageMode()) {
                    isMassStorageMode = true;
                }
            }
            break;
            
        case 3:
            currentUIState = UI_SETTINGS;
            break;
    }
}

void audio_id3data(const char *info) {
    String sInfo = String(info);
    if (sInfo.startsWith("TPE1: ")) {
        currentArtist = sInfo.substring(6);
    }
    if (sInfo.startsWith("TALB: ")) {          // ← ADD THIS
        currentAlbum = sInfo.substring(6);
    }
    Serial.print("ID3: "); Serial.println(info);
}

void audio_id3image(File& file, const size_t pos, const size_t size) {
    if (!albumArtEnabled) return;

    Serial.printf("Album art: %d bytes at pos %d\n", size, pos);

    // ── SAFETY: too big for no-PSRAM device ─────────────────────
    if (size > 120000) {                     // 120 KB max (you can try 150000 if you want)
        Serial.println("→ Too large, using disk icon instead");
        hasAlbumArt = false;
        return;
    }

    artSprite.fillSprite(TFT_BLACK);
    TJpgDec.setJpgScale(8);                  // highest scale = smallest RAM usage

    TJpgDec.setCallback([](int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) -> bool {
        artSprite.pushImage(x, y, w, h, bitmap);
        return true;
    });

    uint8_t *buffer = (uint8_t*)malloc(size);
    if (buffer) {
        file.seek(pos);
        file.read(buffer, size);

        // Most MP3s have extra ID3 header before the real JPEG → find FF D8
        size_t jpegStart = 0;
        for (size_t i = 0; i < size - 1; i++) {
            if (buffer[i] == 0xFF && buffer[i + 1] == 0xD8) {
                jpegStart = i;
                break;
            }
        }

        uint16_t jpgW = 0, jpgH = 0;
        TJpgDec.getJpgSize(&jpgW, &jpgH, buffer + jpegStart, size - jpegStart);

        uint16_t scaledW = jpgW / 8;
        uint16_t scaledH = jpgH / 8;
        int16_t ox = (artSprite.width()  - scaledW) / 2;
        int16_t oy = (artSprite.height() - scaledH) / 2;
        if (ox < 0) ox = 0;
        if (oy < 0) oy = 0;

        TJpgDec.drawJpg(ox, oy, buffer + jpegStart, size - jpegStart);

        free(buffer);
        hasAlbumArt = true;
        Serial.printf("→ Decoded successfully (%dx%d → ~%dx%d)\n", jpgW, jpgH, scaledW, scaledH);
    } else {
        Serial.println("→ Out of memory (even after size check)");
        hasAlbumArt = false;
    }
}

void audio_eof_mp3(const char *) {
    if (!isMassStorageMode && currentUIState == UI_PLAYER && fileCount > 0) {
        currentFileIndex = (currentFileIndex + 1) % fileCount;
        nextTrackRequest = true;
    }
}

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    cfg.internal_mic = false;
    cfg.internal_spk = false;
    M5Cardputer.begin(cfg, true);

    M5Cardputer.Display.setFont(&fonts::lgfxJapanGothic_8);
    Serial.println("Configuring TCA8418 keyboard");
    std::unique_ptr<KeyboardReader> reader(new TCA8418KeyboardReader());
    M5Cardputer.Keyboard.begin(std::move(reader));
    

 

    xTaskCreatePinnedToCore([] (void*) { 
        initUI();
        if (!initSDCard()) {
            Serial.println("SD Init Failed!");
            while (true) vTaskDelay(1000);
        }

        showLoadingDots();

        scanDirectory(lastFolder);
        currentFileIndex = lastFileIndex;
        selectedFileIndex = lastFileIndex;
        loadSettings();

        if (fileCount > 0) {
            currentUIState = UI_PLAYER;
            selectedFileIndex = 0;
            currentFileIndex = 0;
            textPos = 90;
            nextTrackRequest = true;
            isPlaying = true;
            isStoped = false;
            trackStartMillis = millis();
        } else {
            currentUIState = UI_FOLDER_SELECT;
            currentFolder = "/";
            scanDirectory("/");
        }

        while (true) {
            M5Cardputer.update();
            unsigned long now = millis();

            if (M5Cardputer.Keyboard.isChange()) {
                auto ks = M5Cardputer.Keyboard.keysState();
                
                for (auto ch : ks.word) {
                    if (ch != 0) {
                        if (ch == '`') { 
                            resetActivityTimer();

                            if (isMassStorageMode) {
                                sprite1.setTextDatum(TL_DATUM);
                                M5Cardputer.Display.fillScreen(TFT_BLACK);
                                M5Cardputer.Display.setTextColor(TFT_WHITE);
                                M5Cardputer.Display.drawString("Exiting & Rebooting...", 120, 67);
                                
                                msc.end(); 
                                delay(500); 

                                ESP.restart();
                            }

                            if (currentUIState == UI_PLAYER) {
                                currentUIState = UI_POPUP_MENU;
                                popupMenuIndex = 0;
                            }
                            else if (currentUIState == UI_POPUP_MENU) {
                                currentUIState = UI_PLAYER;
                                popupMenuIndex = 0;
                            }
                            else if (currentUIState == UI_SETTINGS) {
                                currentUIState = UI_POPUP_MENU;
                                popupMenuIndex = 3; 
                            }
                            else {
                                currentUIState = UI_PLAYER;
                            }

                            continue;
                        }
                        

                        if (ch == 'u' && ks.ctrl) { 
                            if (!isMassStorageMode) {
                                if (isPlaying) {
                                    playbackTime = millis() - trackStartMillis; 
                                    isPlaying = false;
                                } else {
                                    trackStartMillis = millis() - playbackTime;
                                    isPlaying = true;
                                }
                                nextTrackRequest = false;
                                delay(100); 
                                if (startMassStorageMode()) Serial.println("MSC Started");
                            } else {
                                stopMassStorageMode();
                                M5Cardputer.Display.fillScreen(TFT_BLACK);
                                scanDirectory(currentFolder); 
                            }
                            continue;
                        }

                        if (!isMassStorageMode) {
                            handleKeyPress(ch);

                            if (ch == ';' || ch == '.') {
                                repeatState.key = ch;
                                repeatState.firstPressTime = now;
                                repeatState.lastActionTime = now;
                                repeatState.isRepeating = false;
                            }
                        }
                    }
                }

                if (!isMassStorageMode) {
                    if (ks.enter) handleKeyPress('\n');
                    if (ks.del)   handleKeyPress('\b');
                }
            }

            if (isMassStorageMode && !usbConnected) { 
                audio.stopSong();
                isPlaying = false;
                isStoped = true;
                delay(200);
                
                if (startMassStorageMode()) {
                    Serial.println("MSC Started from Menu");
                    usbConnected = true;
                } else {
                    isMassStorageMode = false; // Failed to take mutex
                    popupText = "SD Busy - Try Again";
                    popupStart = millis();
                }
            }

           if (isMassStorageMode && usbConnected) {
                if (M5Cardputer.Keyboard.isChange()) {
                    auto ks = M5Cardputer.Keyboard.keysState();
                    
                    for (auto ch : ks.word) {
                        if (ch == '`') exitPressed = true;
                    }

                    if (exitPressed) {
                        M5Cardputer.Display.fillScreen(TFT_BLACK);
                        M5Cardputer.Display.setTextColor(TFT_WHITE);
                        M5Cardputer.Display.drawString("Exiting & Rebooting...", 120, 67);
                        
                        msc.end(); 
                        delay(500); 

                        ESP.restart(); 
                    }
                }
                vTaskDelay(100 / portTICK_PERIOD_MS);
                continue; 
            }

            if (M5Cardputer.BtnA.wasPressed()) {
                if (currentUIState == UI_POPUP_MENU || currentUIState == UI_SETTINGS) {
                } else if (fileCount > 0) {
                    currentFileIndex = random(0, fileCount);
                    selectedFileIndex = currentFileIndex;
                    nextTrackRequest = true;
                    isPlaying = true;
                    isStoped = false;
                    trackStartMillis = millis();
                    playbackTime = 0;
                    textPos = 90;
                    currentUIState = UI_PLAYER;

                    int ideal = currentFileIndex - (VISIBLE_FILE_COUNT / 2);
                    viewStartIndex = max(0, min(fileCount - VISIBLE_FILE_COUNT, ideal));
                } else {
                    popupText = "No music";
                    popupStart = millis();
                }
                resetActivityTimer();
            }

            bool keyStillDown = M5Cardputer.Keyboard.isPressed();
            if (repeatState.key != 0 && keyStillDown) {
                unsigned long held = now - repeatState.firstPressTime;
                if (held >= HOLD_THRESHOLD_MS) {
                    unsigned long sinceLast = now - repeatState.lastActionTime;
                    unsigned long interval = repeatState.isRepeating ? REPEAT_INTERVAL_MS : HOLD_THRESHOLD_MS;

                    if (sinceLast >= interval) {
                        handleKeyPress(repeatState.key);
                        repeatState.lastActionTime = now;
                        repeatState.isRepeating = true;
                    }
                }
            } else {
                repeatState.key = 0;
                repeatState.isRepeating = false;
            }

            if (currentUIState == UI_POPUP_MENU) {
                drawPopupMenu();
            } else if (currentUIState == UI_SETTINGS) {
                drawSettingsMenu();
            } else {
                draw();
            }

            vTaskDelay(35 / portTICK_PERIOD_MS);
        }
    }, "UITask", 20480, NULL, 2, &handleUITask, 0);

    // --- AUDIO TASK ---
    xTaskCreatePinnedToCore([] (void*) {
        if (!initES8311Codec()) Serial.println("Codec init failed");
        while (true) {
            if (!isMassStorageMode) {
                if (nextTrackRequest && fileCount > 0) {
                    hasAlbumArt = false;
                    currentArtist = "Unknown Artist";
                    currentAlbum  = "Unknown Album";

                    audio.stopSong();
                    trackStartMillis = millis();
                    playbackTime = 0;
                    const String &path = audioFiles[currentFileIndex];
                    
                    if (SD.exists(path) && codec_initialized && audio.connecttoFS(SD, path.c_str())) {
                        isPlaying = true;
                        isStoped = false;
                    } else {
                        isPlaying = false;
                        isStoped = true;
                    }
                    nextTrackRequest = false;
                }

                if (currentUIState == UI_PLAYER && isPlaying && codec_initialized && !isStoped && fileCount > 0) {
                    audio.loop();
                    if (!audio.isRunning()) {
                        currentFileIndex = (currentFileIndex + 1) % fileCount;
                        nextTrackRequest = true;
                    }
                }
            }
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }
    }, "AudioTask", 12288, NULL, 3, &handleAudioTask, 1);
    
}

void loop() {
    updateHeadphoneDetection();
    delay(100);
}

