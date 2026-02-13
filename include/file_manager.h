#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
// USB / MSC Includes
#include <USB.h>
#include <USBMSC.h>

#define SD_SCK 40
#define SD_MISO 39
#define SD_MOSI 14
#define SD_CS 12

#define MAX_FILES 100

extern String audioFiles[MAX_FILES];
extern uint8_t fileCount;
extern uint8_t currentFileIndex;
extern String currentFolder;

extern String availableFolders[];
extern uint8_t folderCount;

extern bool isScanning;
extern bool isScanningFiles;
extern uint8_t scanProgress;
extern uint8_t scanTotal;

extern SemaphoreHandle_t sdMutex;

extern USBMSC msc;
extern bool isMassStorageMode;

bool initSDCard();
void scanDirectory(const String& folder);
String getFileName(uint8_t index);

bool startMassStorageMode();
void stopMassStorageMode();

#endif