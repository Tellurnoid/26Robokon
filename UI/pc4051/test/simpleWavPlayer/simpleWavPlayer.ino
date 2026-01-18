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

/* ===== 再生制御 ===== */
bool isPlaying = false;
bool isPaused  = false;
float volume = 0.5;

File wavFile;

/* ===== プレイリスト（最大32曲まで）===== */
#define MAX_TRACKS 32
String playlist[MAX_TRACKS];
int playlistCount = 0;
int playIndex = 0;

/* ===== I2S 初期化 ===== */
void setupI2S(uint32_t sampleRate) {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = sampleRate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
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
}

/* ===== WAVヘッダ読み取り ===== */
bool readWavHeader(File &file, uint32_t &sampleRate, uint16_t &bits, uint16_t &channels) {
  uint8_t header[44];
  if (file.read(header, 44) != 44) return false;

  if (memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) return false;

  channels   = header[22] | (header[23] << 8);
  sampleRate = header[24] | (header[25] << 8) | (header[26] << 16) | (header[27] << 24);
  bits       = header[34] | (header[35] << 8);

  if (bits != 16) return false;
  if (channels != 1 && channels != 2) return false;

  Serial.printf("WAV: %u Hz, %ubit, %uch\n", sampleRate, bits, channels);
  return true;
}

/* ===== SDカードからWAVを自動収集 ===== */
void buildPlaylist(fs::FS &fs, const char * dirname) {
  File root = fs.open(dirname);
  if (!root) return;

  File file = root.openNextFile();
  while (file && playlistCount < MAX_TRACKS) {
    if (!file.isDirectory()) {
      String name = file.name();
      name.toLowerCase();
      if (name.endsWith(".wav")) {
        playlist[playlistCount++] = String(file.name());
        Serial.printf("Added: %s\n", file.name());
      }
    }
    file = root.openNextFile();
  }

  Serial.printf("Playlist loaded: %d tracks\n", playlistCount);
}

/* ===== 再生開始 ===== */
void startPlay(const String &path) {
  if (wavFile) wavFile.close();

  wavFile = SD.open(path);
  if (!wavFile) {
    Serial.println("Failed to open WAV");
    return;
  }

  uint32_t sr;
  uint16_t bits, ch;
  if (!readWavHeader(wavFile, sr, bits, ch)) {
    wavFile.close();
    return;
  }

  setupI2S(sr);
  isPlaying = true;
  isPaused  = false;

  Serial.printf("Playing: %s\n", path.c_str());
}

/* ===== 操作系 ===== */
void pausePlay()  { isPaused = true;  Serial.println("Paused"); }
void resumePlay() { isPaused = false; Serial.println("Resumed"); }

void nextTrack() {
  playIndex++;
  if (playIndex >= playlistCount) playIndex = 0;
  startPlay(playlist[playIndex]);
}

/* ===== setup ===== */
void setup() {
  Serial.begin(115200);

  if (!SD.begin()) {
    Serial.println("SD mount failed");
    return;
  }

  Serial.println("Scanning SD card...");
  buildPlaylist(SD, "/");

  if (playlistCount == 0) {
    Serial.println("No WAV files found.");
    return;
  }

  startPlay(playlist[0]);

  Serial.println("Commands:");
  Serial.println(" p : pause");
  Serial.println(" r : resume");
  Serial.println(" n : next");
}

/* ===== loop ===== */
void loop() {
  static uint8_t buffer[1024];
  size_t bytesRead, bytesWritten;

  if (isPlaying && !isPaused && wavFile) {
    bytesRead = wavFile.read(buffer, sizeof(buffer));
    if (bytesRead > 0) {

      int16_t *samples = (int16_t *)buffer;
      int count = bytesRead / 2;
      for (int i = 0; i < count; i++) {
        samples[i] = (int16_t)(samples[i] * volume);
      }

      i2s_write(I2S_PORT, buffer, bytesRead, &bytesWritten, portMAX_DELAY);

    } else {
      Serial.println("Track finished");
      nextTrack();
    }
  }

  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'p') pausePlay();
    if (c == 'r') resumePlay();
    if (c == 'n') nextTrack();
    if (c == 'u' && volume < 1.0) volume = volume + 0.1; Serial.println(volume);
    if (c == 'd' && volume > 0.0) volume = volume - 0.1; Serial.println(volume);
  }
}
