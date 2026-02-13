#include "settings.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

String defaultBootFolder = "/Music";
const char* settingsFile = "/mp3player_settings.json";

void initSettings() {
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
        return;
    }
    loadSettings();
}

void loadSettings() {
    File file = LittleFS.open(settingsFile, "r");
    if (!file) {
        Serial.println("No settings file found, using defaults.");
        return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.println("Failed to read settings JSON");
        return;
    }

    if (doc.containsKey("boot_folder")) {
        defaultBootFolder = doc["boot_folder"].as<String>();
    }
}

void saveSettings() {
    File file = LittleFS.open(settingsFile, "w");
    if (!file) {
        Serial.println("Failed to open settings file for writing");
        return;
    }

    StaticJsonDocument<256> doc;
    doc["boot_folder"] = defaultBootFolder;

    if (serializeJson(doc, file) == 0) {
        Serial.println("Failed to write JSON to file");
    }
    file.close();
}