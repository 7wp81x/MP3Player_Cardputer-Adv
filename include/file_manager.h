#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <SD.h>

#define SD_SCK 40
#define SD_MISO 39
#define SD_MOSI 14
#define SD_CS 12

#define MAX_FILES 250

extern String audioFiles[MAX_FILES];
extern int fileCount;
extern int currentFileIndex;
extern String currentFolder;

extern String availableFolders[];
extern int folderCount;

extern bool isScanning;
extern unsigned short int scanProgress;
extern unsigned short int scanTotal;

bool initSDCard();
void scanFolders(const String& folder);
void listAudioFiles(const String& folder);
String getFileName(int index);

#endif