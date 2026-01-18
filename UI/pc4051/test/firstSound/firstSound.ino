#include "driver/i2s.h"
#include <math.h>

#define I2S_BCLK 27
#define I2S_LRC  26
#define I2S_DOUT 25

const int sampleRate = 44100;
const float freq = 440.0; // A4

void setupI2S() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = sampleRate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 4,
    .dma_buf_len = 512,
    .use_apll = false
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

void setup() {
  setupI2S();
}

void loop() {
  const int samples = 256;
  static int16_t buffer[samples * 2];
  for (int i = 0; i < samples; i++) {
    float v = sinf(2 * 3.14159 * freq * (i / (float)sampleRate));
    int16_t val = (int16_t)(v * 30000);
    buffer[i*2]   = val; // Left
    buffer[i*2+1] = val; // Right
  }
  size_t out;
  i2s_write(I2S_NUM_0, buffer, sizeof(buffer), &out, portMAX_DELAY);
}
