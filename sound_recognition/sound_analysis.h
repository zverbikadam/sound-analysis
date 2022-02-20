#include "main.h"
#include <esphome.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>

uint8_t buffer32[4096];

const int BLOCK_SIZE = SAMPLES;
// our FFT data
static double* real = new double[SAMPLES];
static double* imag = new double[SAMPLES];
static arduinoFFT fft(real, imag, (uint16_t) SAMPLES, (double) SAMPLES);
static float energy[OCTAVES];

void init_i2s()
{
  esp_err_t err;

  // Config struct
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = 0,
      .dma_buf_count = DMA_BUF_COUNT,
      .dma_buf_len = DMA_BUF_LEN,
      .use_apll = false,
  };

  // Config GPIO
  const i2s_pin_config_t pin_config = {
      .bck_io_num = PIN_I2S_SCK,
      .ws_io_num = PIN_I2S_WS,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = PIN_I2S_SD};

  // Init driver
  err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
if (err != ESP_OK) {
  ESP_LOGE("Sound Sensor", "Failed installing driver: %d\n", err);
  while (true)
    ;
}

  // Init GPIO
  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    ESP_LOGE("Sound Sensor", "Failed setting up pin: %d\n", err);
    while (true)
    ;
  }
  ESP_LOGI("Sound Sensor", "I2S driver installed correctly!");
}

void read_data()
{
  uint32_t bytes_read = 0;
  uint16_t samples_read;
  Serial.println("Recording started!");
  i2s_read(I2S_PORT, buffer32, sizeof(buffer32), &bytes_read, portMAX_DELAY);

  samples_read = bytes_read >> 2;

  for (uint32_t i = 0; i < samples_read; i++)
  {
    uint8_t lsb = buffer32[i * 4 + 2];
    uint8_t msb = buffer32[i * 4 + 3];
    uint16_t sample = ((((uint16_t)msb) << 8) | ((uint16_t)lsb)) << 4;

      //Serial.printf("%ld\n", sample);
  }
}

class SoundSensor : public Component, public CustomMQTTDevice, public BinarySensor {
 public:

// binary sensor with states 0 -> not recognized; 1 -> recognized
BinarySensor *sound_recognition_sensor = new BinarySensor();

// For components that should be initialized after a data connection (API/MQTT) is connected
float get_setup_priority() const override { return esphome::setup_priority::AFTER_CONNECTION; }

  void setup() override {
    // start up the I2S peripheral

    ESP_LOGI("Sound Sensor", "Configuring I2S...");
    init_i2s();

    delay(500);
  }

  void loop() override {
    read_data();
  }
};