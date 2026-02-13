#include "settings.h"
#include <SD.h>
#include <ArduinoJson.h>
#include "M5Cardputer.h"
#include "file_manager.h"
#include "ui_manager.h"

String defaultBootFolder = "/Music";
String lastFolder = "/Music";
int lastFileIndex = 0;
uint8_t savedVolume = 10;
uint8_t savedBrightness = 100;
uint8_t maxVolumeCap = 20;

uint32_t screenTimeoutSeconds = 30;

const char* settingsFile = "/mp3player_settings.json";


String validatePath(String path) {
    if (path.length() == 0) return "/Music";
    if (!path.startsWith("/")) path = "/" + path;
    if (path.endsWith("/") && path.length() > 1) path.remove(path.length() - 1);
    
    if (sdMutex != NULL && xSemaphoreTake(sdMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        if (SD.exists(path)) {
            File dir = SD.open(path);
            if (dir && dir.isDirectory()) {
                dir.close();
                xSemaphoreGive(sdMutex);
                return path;
            }
            if (dir) dir.close();
        }
        xSemaphoreGive(sdMutex);
    }
    return "/Music";
}

void loadSettings() {
    if (sdMutex == NULL) return;

    if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        File file = SD.open(settingsFile, "r");
        if (!file) {
            Serial.println("Settings file not found on SD, using defaults.");
            xSemaphoreGive(sdMutex);
            return;
        }

        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, file);
        file.close();
        xSemaphoreGive(sdMutex);

        if (!error) {
            defaultBootFolder = doc["boot_folder"] | "/Music";
            lastFolder = doc["last_folder"] | "/Music";
            lastFileIndex = doc["last_index"] | 0;
            savedVolume = doc["volume"] | 10;
            savedBrightness = doc["brightness"] | 100;
            screenTimeoutSeconds = doc["timeout_sec"] | 30;
            maxVolumeCap = doc["vol_cap"] | 20;
            screenTimeoutEnabled = doc["timeout_on"] | true;
            isShowMp3Image = doc["show_anim"] | true;

            M5Cardputer.Display.setBrightness(savedBrightness);
            Serial.println("Settings loaded from SD.");
        }
    }
}

void saveSettings() {
    if (sdMutex == NULL) return;

    lastFolder = currentFolder;
    lastFileIndex = currentFileIndex;

    if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
        File file = SD.open(settingsFile, FILE_WRITE); // Note: FILE_WRITE for SD
        if (!file) {
            Serial.println("Failed to write settings to SD");
            xSemaphoreGive(sdMutex);
            return;
        }

        StaticJsonDocument<1024> doc;
        doc["boot_folder"] = defaultBootFolder;
        doc["last_folder"] = lastFolder;
        doc["last_index"] = lastFileIndex;
        doc["volume"] = savedVolume;
        doc["brightness"] = M5Cardputer.Display.getBrightness();
        doc["timeout_on"] = screenTimeoutEnabled;
        doc["timeout_sec"] = screenTimeoutSeconds;
        doc["show_anim"] = isShowMp3Image;
        doc["vol_cap"] = maxVolumeCap;

        if (serializeJson(doc, file) == 0) {
            Serial.println("Failed to serialize JSON to SD");
        }
        
        file.close();
        xSemaphoreGive(sdMutex);
        Serial.println("Settings saved to SD.");
    }
}