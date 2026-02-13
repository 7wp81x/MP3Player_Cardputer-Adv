#include "file_manager.h"
#include "M5Cardputer.h"

SemaphoreHandle_t sdMutex = NULL;

String audioFiles[MAX_FILES];
uint8_t fileCount = 0;
uint8_t currentFileIndex = 0;
String currentFolder = "/";

String availableFolders[20];
uint8_t folderCount = 0;

USBMSC msc;
bool isMassStorageMode = false;
int32_t usbReadCallback(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    uint32_t secSize = SD.sectorSize();
    if (secSize == 0) return -1;
    for (uint32_t x = 0; x < bufsize / secSize; ++x) {
        if (!SD.readRAW((uint8_t*)buffer + x * secSize, lba + x)) return -1;
    }
    return bufsize;
}

int32_t usbWriteCallback(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    uint32_t secSize = SD.sectorSize();
    if (secSize == 0) return -1;
    for (uint32_t x = 0; x < bufsize / secSize; ++x) {
        if (!SD.writeRAW(buffer + x * secSize, lba + x)) return -1;
    }
    return bufsize;
}

bool usbStartStopCallback(uint8_t, bool start, bool load_eject) {
    return true;
}

bool startMassStorageMode() {
    if (sdMutex == NULL) return false;

    if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        Serial.println("Failed to take SD Mutex for MSC");
        return false;
    }

    uint32_t secSize = SD.sectorSize();
    uint32_t numSectors = SD.numSectors();

    msc.vendorID("M5CARD");
    msc.productID("SD-USB");
    msc.onRead(usbReadCallback);
    msc.onWrite(usbWriteCallback);
    msc.onStartStop(usbStartStopCallback);
    msc.mediaPresent(true);
    msc.begin(numSectors, secSize);
    USB.begin();

    isMassStorageMode = true;
    M5Cardputer.Display.setFont(&fonts::Font0);
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setTextDatum(MC_DATUM);
    
    M5Cardputer.Display.setTextColor(TFT_GREENYELLOW);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.drawString("USB MASS STORAGE", 120, 45);
    
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.setTextSize(1.5);
    M5Cardputer.Display.drawString("Connect to PC", 120, 75);
    
    M5Cardputer.Display.setTextColor(TFT_SILVER);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.drawString("Press ESC to Eject & Reboot", 120, 110);
    
    return true;
}

void stopMassStorageMode() {
    msc.end();
    // USB.end();
    
    if (sdMutex != NULL) {
        xSemaphoreGive(sdMutex);
    }
    
    initSDCard(); 
    Serial.println("MSC Stopped. SD Re-initialized.");
}

bool initSDCard() {
    // Create the mutex if it doesn't exist
    if (sdMutex == NULL) {
        sdMutex = xSemaphoreCreateMutex();
    }

    SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
    if (!SD.begin(SD_CS)) {
        Serial.println("SD Mount Failed");
        return false;
    }

    uint8_t type = SD.cardType();
    Serial.printf("SD initialized | Type: %d | Size: %llu MB\n", 
                  type, SD.cardSize() / (1024 * 1024));

    return true;
}

void scanDirectory(const String& path) {
    // Protect SD access with Mutex
    if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
        fileCount = folderCount = 0;
        currentFolder = path;

        File dir = SD.open(path);
        if (!dir || !dir.isDirectory()) {
            xSemaphoreGive(sdMutex);
            return;
        }

        File entry = dir.openNextFile();
        while (entry && fileCount < MAX_FILES) {
            String name = entry.name();
            bool isRoot = (path == "/");

            if (entry.isDirectory()) {
                if (folderCount < 20) {
                    String full = isRoot ? "/" + name : path + "/" + name;
                    availableFolders[folderCount++] = full;
                }
            } else {
                String lower = name;
                lower.toLowerCase();
                if (lower.endsWith(".mp3") || lower.endsWith(".wav")) {
                    String full = isRoot ? "/" + name : path + "/" + name;
                    audioFiles[fileCount++] = full;
                }
            }
            entry.close();
            entry = dir.openNextFile();
            vTaskDelay(1); 
        }
        dir.close();
        xSemaphoreGive(sdMutex);
    }
}

String getFileName(uint8_t index) {
    if (index >= fileCount) return "";
    String path = audioFiles[index];
    int slash = path.lastIndexOf('/');
    int dot   = path.lastIndexOf('.');
    if (dot <= slash) return path;
    if (slash == -1)  return path.substring(0, dot);
    return path.substring(slash + 1, dot);
}