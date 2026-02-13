#include "settings.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "M5Cardputer.h"
#include <SD.h>
#include "ui_manager.h"

String defaultBootFolder = "/Music";
String lastFolder = "/Music";
int lastFileIndex = 0;
uint8_t savedVolume = 10;
uint8_t savedBrightness = 100;

uint8_t maxVolumeCap = 20;
uint32_t screenTimeoutSeconds = 30;
const char* settingsFile = "/mp3player_settings.json";

void initSettings() {
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Fail");
        return;
    }
    loadSettings();
}

String validatePath(String path) {
    if (path.length() == 0) return "/Music";
    if (!path.startsWith("/")) path = "/" + path;
    if (path.endsWith("/") && path.length() > 1) path.remove(path.length() - 1);
    
    if (SD.exists(path)) {
        File dir = SD.open(path);
        if (dir.isDirectory()) {
            dir.close();
            return path;
        }
        dir.close();
    }
    return "/Music";
}

void loadSettings() {
    File file = LittleFS.open(settingsFile, "r");
    if (!file) return;

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (!error) {
        if (doc.containsKey("boot_folder")) defaultBootFolder = doc["boot_folder"].as<String>();
        if (doc.containsKey("last_folder")) lastFolder = doc["last_folder"].as<String>();
        
        lastFileIndex = doc["last_index"] | 0;
        savedVolume = doc["volume"] | 10;
        savedBrightness = doc["brightness"] | 100;
        screenTimeoutSeconds = doc["timeout_sec"] | 30;
        maxVolumeCap = doc["vol_cap"] | 20;

        screenTimeoutEnabled = doc["timeout_on"] | true;
        isShowMp3Image = doc["show_anim"] | true; // SAVES VINYL STATE

        M5Cardputer.Display.setBrightness(savedBrightness);
    }
}

void saveSettings() {
    File file = LittleFS.open(settingsFile, "w");
    if (!file) return;

    StaticJsonDocument<1024> doc;

    // Save everything to the JSON object
    doc["boot_folder"] = defaultBootFolder;
    doc["last_folder"] = lastFolder;
    doc["last_index"] = lastFileIndex;
    doc["volume"] = savedVolume;
    doc["brightness"] = M5Cardputer.Display.getBrightness();
    doc["timeout_on"] = screenTimeoutEnabled;
    doc["timeout_sec"] = screenTimeoutSeconds;
    doc["show_anim"] = isShowMp3Image; // SAVES VINYL STATE
    doc["vol_cap"] = maxVolumeCap;

    serializeJson(doc, file);
    file.close();
}