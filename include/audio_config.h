#ifndef AUDIO_CONFIG_H
#define AUDIO_CONFIG_H

#include <Arduino.h>
#include "Audio.h"

constexpr int CARDPUTER_I2C_SDA = 8;
constexpr int CARDPUTER_I2C_SCL = 9;
constexpr int CARDPUTER_I2S_BCLK = 41;
constexpr int CARDPUTER_I2S_LRCK = 43;
constexpr int CARDPUTER_I2S_DOUT = 42;
constexpr int CARDPUTER_HP_DET_PIN = 17;
constexpr int CARDPUTER_AMP_EN_PIN = 46;

constexpr uint8_t ES8311_ADDR = 0x18;
constexpr uint32_t ES8311_I2C_FREQ = 400000UL;

extern Audio audio;
extern int8_t volume;
extern bool isPlaying;
extern bool isStoped;
extern uint8_t hpDetectPin;
extern uint8_t ampEnablePin;
extern bool lastHPState;
extern bool codec_initialized;

void handlePlayback(bool playCommand, bool stopCommand, bool nextTrack = false, bool prevTrack = false);
void resetTimer();

bool initES8311Codec();
void changeVolume(int8_t v);
void playTestTone(uint32_t freq_hz, uint32_t duration_ms, uint32_t sample_rate = 44100, uint16_t amplitude = 12000);
void updateHeadphoneDetection();

#endif