#include "driver/i2s.h"
#include "SPIFFS.h"

#define I2S_BCLK 27
#define I2S_LRC  26
#define I2S_DOUT 25
#define I2S_PORT I2S_NUM_0

void setupI2S() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 4,
    .dma_buf_len = 512,
    .use_apll = false
  };

  i2s_pin_config_t pin = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin);
}

void setup() {
  SPIFFS.begin(true);
  setupI2S();

  File f = SPIFFS.open("/test.wav", "r");
  f.seek(44);  // WAVヘッダスキップ

  uint8_t buffer[1024];
  size_t bytesRead;
  size_t bytesWritten;

  while ((bytesRead = f.read(buffer, sizeof(buffer))) > 0) {
    i2s_write(I2S_PORT, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
  }

  f.close();

  // ここが重要：無音でDMAを埋める
  i2s_zero_dma_buffer(I2S_PORT);

  // I2S を停止
  i2s_stop(I2S_PORT);

  // 完全に止めたいならこれも可
  // i2s_driver_uninstall(I2S_PORT);
}

void loop() {
}
