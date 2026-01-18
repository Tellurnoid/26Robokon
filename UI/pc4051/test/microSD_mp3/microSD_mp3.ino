#include <Arduino.h>
#include "SPIFFS.h"
#include "SD.h"
#include "SPI.h"
#include "driver/i2s.h"

/* ===== PCM5102 接続ピン ===== */
#define I2S_BCLK 27
#define I2S_LRC  26
#define I2S_DOUT 25
#define I2S_PORT I2S_NUM_0

/* ===== 音量（0.0 ～ 1.0）===== */
float volume = 0.2;

/* ===== I2S 初期化 ===== */
void setupI2S(uint32_t sampleRate) {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = sampleRate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 6,
    .dma_buf_len = 512,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin = {
    .bck_io_num   = I2S_BCLK,
    .ws_io_num    = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num  = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin);
  i2s_zero_dma_buffer(I2S_PORT);

  Serial.println("I2S initialized");
}

/* ===== WAV ヘッダを簡易解析 ===== */
bool readWavHeader(File &file, uint32_t &sampleRate, uint16_t &bits, uint16_t &channels) {
  uint8_t header[44];
  if (file.read(header, 44) != 44) {
    Serial.println("Failed to read WAV header");
    return false;
  }

  // "RIFF"
  if (memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) {
    Serial.println("Not a WAV file");
    return false;
  }

  channels   = header[22] | (header[23] << 8);
  sampleRate = header[24] | (header[25] << 8) | (header[26] << 16) | (header[27] << 24);
  bits       = header[34] | (header[35] << 8);

  Serial.println("===== WAV INFO =====");
  Serial.printf("SampleRate : %u Hz\n", sampleRate);
  Serial.printf("Channels   : %u\n", channels);
  Serial.printf("Bits       : %u bit\n", bits);
  Serial.println("====================");

  if (bits != 16) {
    Serial.println("Only 16bit PCM supported");
    return false;
  }
  if (channels != 1 && channels != 2) {
    Serial.println("Only mono or stereo supported");
    return false;
  }

  return true;
}

/* ===== SDカード上の WAV 再生 ===== */
void playWavFromSD(const char *path) {
  File f = SD.open(path);
  if (!f) {
    Serial.println("Failed to open WAV file on SD");
    return;
  }

  Serial.printf("Opened WAV file: %s\n", path);

  uint32_t sampleRate;
  uint16_t bits, channels;

  if (!readWavHeader(f, sampleRate, bits, channels)) {
    f.close();
    return;
  }

  // WAVのサンプリング周波数に合わせてI2S初期化
  setupI2S(sampleRate);

  const int bufSize = 1024;
  uint8_t buffer[bufSize];
  size_t bytesRead, bytesWritten;

  Serial.println("Start playback...");

  while ((bytesRead = f.read(buffer, bufSize)) > 0) {
    // 16bit PCMとして音量調整
    int16_t *samples = (int16_t *)buffer;
    int sampleCount = bytesRead / 2;

    for (int i = 0; i < sampleCount; i++) {
      samples[i] = (int16_t)((float)samples[i] * volume);
    }

    i2s_write(I2S_PORT, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
  }

  f.close();
  Serial.println("Playback finished");

  // 再生後のノイズ防止
  uint8_t zero[512] = {0};
  for (int i = 0; i < 10; i++) {
    i2s_write(I2S_PORT, zero, sizeof(zero), &bytesWritten, portMAX_DELAY);
  }
  i2s_zero_dma_buffer(I2S_PORT);
}

/* ===== セットアップ ===== */
void setup() {
  Serial.begin(115200);

  Serial.println("Mounting SD card...");
  if (!SD.begin()) {
    Serial.println("SD Card Mount Failed");
    return;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) Serial.println("MMC");
  else if (cardType == CARD_SD) Serial.println("SDSC");
  else if (cardType == CARD_SDHC) Serial.println("SDHC");
  else Serial.println("UNKNOWN");

  Serial.printf("SD Size: %llu MB\n", SD.cardSize() / (1024 * 1024));

  // 例: SDカード直下に置いた test.wav を再生
  playWavFromSD("/JULY.wav");
}

void loop() {
}
