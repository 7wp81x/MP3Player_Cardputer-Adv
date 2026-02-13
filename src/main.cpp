#include <Arduino.h>
#include "M5Cardputer.h"
#include "audio_config.h"
#include "file_manager.h"
#include "ui_manager.h"
#include "settings.h"
#include <utility/Keyboard/KeyboardReader/TCA8418.h>

TaskHandle_t handleUITask = NULL;
TaskHandle_t handleAudioTask = NULL;
volatile bool usbConnected = false;

void showLoadingDots(const char* message = "Scanning folder...") {
    sprite1.fillSprite(TFT_BLACK);
    sprite1.setTextColor(TFT_WHITE, TFT_BLACK);
    sprite1.setTextFont(2);
    sprite1.setTextDatum(MC_DATUM);

    const char* dots[] = {"", ".", "..", "..."};
    uint8_t idx = 0;

    unsigned long t0 = millis();
    while (millis() - t0 < 5000) {
        sprite1.fillSprite(TFT_BLACK);
        sprite1.drawString(message, 120, 55);
        sprite1.drawString(dots[idx], 120, 80);
        sprite1.pushSprite(0, 0);

        idx = (idx + 1) % 4;
        delay(200);
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
    initSettings();

    xTaskCreatePinnedToCore([] (void*) { 
        initUI();
        if (!initSDCard()) {
            Serial.println("SD Init Failed!");
            while (true) vTaskDelay(1000);
        }

        showLoadingDots("Loading Music...");

        currentFolder = defaultBootFolder;
        scanDirectory(currentFolder);
        
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
                            if (isMassStorageMode) continue; 

                            if (currentUIState != UI_POPUP_MENU && currentUIState != UI_SETTINGS) {
                                isPlaying = false; 
                                currentUIState = UI_POPUP_MENU;
                                popupMenuIndex = 0; 
                            } else if (currentUIState == UI_SETTINGS) {
                                currentUIState = UI_POPUP_MENU;
                            } else {
                                currentUIState = UI_PLAYER;
                            }
                            resetActivityTimer();
                            continue; 
                        }

                        if (ch == 21 || (ch == 'u' && ks.ctrl)) { 
                            if (!isMassStorageMode) {
                                audio.stopSong();
                                isPlaying = false;
                                isStoped = true;
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
                    
                    bool exitPressed = false;
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

void audio_eof_mp3(const char *) {
    if (!isMassStorageMode && currentUIState == UI_PLAYER && fileCount > 0) {
        currentFileIndex = (currentFileIndex + 1) % fileCount;
        nextTrackRequest = true;
    }
}