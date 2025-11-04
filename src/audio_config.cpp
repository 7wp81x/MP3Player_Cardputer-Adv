#include "M5Cardputer.h"
#include "audio_config.h"
#include "file_manager.h"
#include "driver/i2s.h"
#include <Wire.h>
#include <math.h>

Audio audio;
int8_t volume = 10;
bool isPlaying = true;
bool isStoped = false;
uint8_t hpDetectPin = CARDPUTER_HP_DET_PIN;
uint8_t ampEnablePin = CARDPUTER_AMP_EN_PIN;
bool lastHPState = false;
bool codec_initialized = false;

static bool es8311_write(uint8_t reg, uint8_t val) {
    uint8_t data = val;
    if (!M5.In_I2C.writeRegister(ES8311_ADDR, reg, &data, 1, ES8311_I2C_FREQ)) {
        Serial.printf("ES8311 I2C write failed reg 0x%02X\n", reg);
        return false;
    }
    delay(2);
    return true;
}

void changeVolume(int8_t v) {
    volume += v;
    if (volume > 20) volume = 20;
    else if (volume < 0) volume = 0;
    audio.setVolume(volume);
}

void playTestTone(uint32_t freq_hz, uint32_t duration_ms, uint32_t sample_rate, uint16_t amplitude) {
    Serial.printf("Playing test tone: %lu Hz for %lu ms\n", freq_hz, duration_ms);
    
    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate = sample_rate;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags = 0;
    cfg.dma_buf_count = 6;
    cfg.dma_buf_len = 256;
    cfg.use_apll = false;
    cfg.tx_desc_auto_clear = true;
    cfg.fixed_mclk = 0;
    
    if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK) {
        Serial.println("i2s_driver_install failed - skipping test tone");
        return;
    }
    
    i2s_pin_config_t pins = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = CARDPUTER_I2S_BCLK,
        .ws_io_num = CARDPUTER_I2S_LRCK,
        .data_out_num = CARDPUTER_I2S_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    
    if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
        Serial.println("i2s_set_pin failed - skipping test tone");
        i2s_driver_uninstall(I2S_NUM_0);
        return;
    }
    
    i2s_set_clk(I2S_NUM_0, sample_rate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
    
    const size_t frames_per_buf = 256;
    int16_t buf[frames_per_buf * 2];
    double phase = 0.0;
    double phase_inc = 2.0 * M_PI * (double)freq_hz / (double)sample_rate;
    uint32_t elapsed = 0;
    uint32_t chunk_ms = (uint32_t)((1000.0 * frames_per_buf) / (double)sample_rate);
    if (chunk_ms == 0) chunk_ms = 6;
    
    while (elapsed < duration_ms) {
        for (size_t i = 0; i < frames_per_buf; ++i) {
            int16_t s = (int16_t)(sin(phase) * amplitude);
            phase += phase_inc;
            if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;
            buf[2 * i + 0] = s;
            buf[2 * i + 1] = s;
        }
        size_t bytes_written = 0;
        i2s_write(I2S_NUM_0, (const char *)buf, sizeof(buf), &bytes_written, portMAX_DELAY);
        elapsed += chunk_ms;
    }
    
    i2s_driver_uninstall(I2S_NUM_0);
    Serial.println("Test tone done");
}

bool initES8311Codec() {
    Serial.println("Initializing ES8311 codec for Cardputer Advanced");
    
    Wire.end();
    delay(50);
    Wire.begin(CARDPUTER_I2C_SDA, CARDPUTER_I2C_SCL, 100000);
    Wire.setTimeOut(50);
    delay(10);
    
    if (hpDetectPin >= 0) {
        pinMode(hpDetectPin, INPUT_PULLUP);
    }
    if (ampEnablePin >= 0) {
        pinMode(ampEnablePin, OUTPUT);
        digitalWrite(ampEnablePin, LOW);
        Serial.printf("AMP_EN: held LOW on pin %d until codec init\n", ampEnablePin);
    }

    struct RegisterValue {
        uint8_t reg;
        uint8_t value;
    };
    
    static constexpr RegisterValue kInitSequence[] = {
        {0x00, 0x80}, {0x01, 0xB5}, {0x02, 0x18}, {0x0D, 0x01},
        {0x12, 0x00}, {0x13, 0x10}, {0x32, 0xBF}, {0x37, 0x08},
    };
    
    bool ok = true;
    for (const auto &entry : kInitSequence) {
        if (!es8311_write(entry.reg, entry.value)) {
            ok = false;
        }
    }
    
    codec_initialized = ok;
    if (!codec_initialized) {
        Serial.println("ES8311 init sequence failed");
        return false;
    }
    
    Serial.println("ES8311 init sequence done");
    Serial.printf("Audio pins: I2C SDA=%d SCL=%d, BCLK=%d LRCK=%d DOUT=%d\n",
                  CARDPUTER_I2C_SDA, CARDPUTER_I2C_SCL, 
                  CARDPUTER_I2S_BCLK, CARDPUTER_I2S_LRCK, CARDPUTER_I2S_DOUT);

    if (hpDetectPin >= 0) {
        bool hpInserted = (digitalRead(hpDetectPin) == LOW);
        lastHPState = hpInserted;
        if (ampEnablePin >= 0) {
            digitalWrite(ampEnablePin, hpInserted ? LOW : HIGH);
            Serial.printf("%s detected -> speaker AMP %s\n", 
                         hpInserted ? "Headphones" : "No headphones",
                         hpInserted ? "OFF" : "ON");
        }
    } else if (ampEnablePin >= 0) {
        digitalWrite(ampEnablePin, HIGH);
    }
    
    playTestTone(440, 1500, 44100, 12000);

    audio.setPinout(CARDPUTER_I2S_BCLK, CARDPUTER_I2S_LRCK, CARDPUTER_I2S_DOUT);
    changeVolume(volume);
    audio.setBalance(0);
    
    return true;
}

void updateHeadphoneDetection() {
    if (hpDetectPin < 0) return;
    
    bool hpInserted = (digitalRead(hpDetectPin) == LOW);
    if (hpInserted != lastHPState) {
        lastHPState = hpInserted;
        if (ampEnablePin >= 0) {
            digitalWrite(ampEnablePin, hpInserted ? LOW : HIGH);
            Serial.printf("HP %s -> speaker AMP %s\n",
                         hpInserted ? "inserted" : "removed",
                         hpInserted ? "OFF" : "ON");
        }
    }
}