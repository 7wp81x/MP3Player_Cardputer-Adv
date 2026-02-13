#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

extern String defaultBootFolder;
extern String lastFolder;
extern int lastFileIndex;

extern uint8_t savedVolume;
extern uint8_t savedBrightness;
extern uint8_t maxVolumeCap;

extern uint32_t screenTimeoutSeconds;

void saveSettings();
void loadSettings();
String validatePath(String path);

#endif