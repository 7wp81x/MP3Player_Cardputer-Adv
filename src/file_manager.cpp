#include "file_manager.h"

String audioFiles[MAX_FILES];
int fileCount = 0;
int currentFileIndex = 0;
String currentFolder = "/";

String availableFolders[20];
int folderCount = 0;

bool isScanning = true;
unsigned short int scanProgress = 0;
unsigned short int scanTotal = 0;

bool initSDCard() {
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
    if (!SD.begin(SD_CS)) {
        Serial.println("ERROR: SD Mount Failed!");
        return false;
    }
    Serial.println("SD Card initialized successfully");
    return true;
}

void scanFolders(const String& folder) {
    folderCount = 0;
    scanProgress = 0;
    scanTotal = 0;
    isScanning = true;

    Serial.printf("Scanning for folders in: %s\n", folder.c_str());
    availableFolders[folderCount++] = folder;

    File root = SD.open(folder);
    if (!root || !root.isDirectory()) {
        Serial.println("Failed to open folder");
        isScanning = false;
        return;
    }

    for (File f = root.openNextFile(); f; f = root.openNextFile()) {
        if (f.isDirectory()) scanTotal++;
    }
    root.rewindDirectory();

    for (File f = root.openNextFile(); f; f = root.openNextFile()) {
        if (f.isDirectory()) {
            String dirname = String(f.name());
            if (!dirname.startsWith("/")) {
                dirname = (folder == "/") ? "/" + dirname : folder + "/" + dirname;
            }
            availableFolders[folderCount++] = dirname;
            scanProgress++;
        }
    }
    root.close();

    isScanning = false;
    Serial.printf("Total folders found: %d\n", folderCount);
}

void listAudioFiles(const String& folder) {
    fileCount = 0;
    currentFolder = folder;
    
    Serial.printf("Listing audio files in: %s\n", folder.c_str());
    
    File root = SD.open(folder);
    if (!root) {
        Serial.println("Failed to open folder");
        return;
    }
    
    if (!root.isDirectory()) {
        Serial.println("Not a directory");
        root.close();
        return;
    }
    
    File file = root.openNextFile();
    while (file && fileCount < MAX_FILES) {
        if (!file.isDirectory()) {
            String fname = String(file.name());

            if (!fname.startsWith("/")) {
                if (folder == "/") {
                    fname = "/" + fname;
                } else {
                    fname = folder + "/" + fname;
                }
            }

            String lower = fname;
            lower.toLowerCase();
            int dot = lower.lastIndexOf('.');
            
            if (dot >= 0) {
                String ext = lower.substring(dot + 1);
                if (ext == "mp3" || ext == "wav") {
                    audioFiles[fileCount] = fname;
                    Serial.printf("  [%d] %s\n", fileCount, fname.c_str());
                    fileCount++;
                }
            }
        }
        file = root.openNextFile();
    }
    
    root.close();
    
    if (fileCount >= MAX_FILES) {
        Serial.printf("WARNING: Reached MAX_FILES limit (%d). Some files were not loaded.\n", MAX_FILES);
    }
    
    Serial.printf("Total audio files found: %d\n", fileCount);
}

String getFileName(int index) {
    if (index < 0 || index >= fileCount) return "";
    
    String fname = audioFiles[index];

    int lastSlash = fname.lastIndexOf('/');
    if (lastSlash >= 0) {
        fname = fname.substring(lastSlash + 1);
    }
    
    int lastDot = fname.lastIndexOf('.');
    if (lastDot > 0) {
        fname = fname.substring(0, lastDot);
    }
    
    return fname;
}