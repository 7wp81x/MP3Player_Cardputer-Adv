#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

extern String defaultBootFolder;

void initSettings();
void saveSettings();
void loadSettings();

#endif