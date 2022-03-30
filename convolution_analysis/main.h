#include "config.h"
#include <esphome.h>
#include <driver/i2s.h>
#include <convolution.h>
#include <Preferences.h>

#include <cstdlib>

bool isButtonPressed;

Preferences preferences;

double test_arr[512] = { 0 };
double test2_arr[512] = { 0 };
static Convolution conv(test_arr, test_arr, 512);
static Convolution conv2(test_arr, test2_arr, 512);

void generateRandomArray(double* arr) {
    for (int i = 0; i < 512; i++) {
        arr[i] = rand() % 1024;
    }
}

uint32_t buffer32[SAMPLES];

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
    ESP_LOGE("Doorbell Sensor", "Failed installing driver: %d\n", err);
    while (true)
      ;
  }

  // Init GPIO
  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    ESP_LOGE("Doorbell Sensor", "Failed setting up pin: %d\n", err);
    while (true)
    ;
  }
  ESP_LOGI("Doorbell Sensor", "I2S driver installed correctly!");
}

static void prepare_for_fft(uint32_t *signal, double *vReal, double *vImag, uint16_t number_of_samples)
{
    for (uint16_t i = 0; i < number_of_samples; i++)
    {
        vReal[i] = (signal[i] >> 16) / 1.0;
        vImag[i] = 0.0;
    }
}

void read_data()
{
  uint32_t bytes_read = 0;
  i2s_read(I2S_PORT, (void *) buffer32, sizeof(buffer32), &bytes_read, portMAX_DELAY);
}

class ConvolutionSensor : public Component, public BinarySensor {
 public:

// binary sensor with states 0 -> not recognized; 1 -> recognized
BinarySensor *convolution_recognition_sensor = new BinarySensor();

// For components that should be initialized after a data connection (API/MQTT) is connected
float get_setup_priority() const override { return esphome::setup_priority::AFTER_CONNECTION; }

  void setup() override {
    // start up the I2S peripheral

    ESP_LOGI("Doorbell Sensor", "Configuring I2S...");
    init_i2s();

    srand(time(NULL));

    pinMode(PIN_BUTTON, INPUT);
    isButtonPressed = false;


    delay(500);
  }

  void loop() override {

    // check if button is pressed
    isButtonPressed = digitalRead(PIN_BUTTON);
    
    if (isButtonPressed) {
      generateRandomArray(test_arr);
      double result = conv.calculateCrossCorrelation(test_arr, test_arr, 512);
      ESP_LOGI("Doorbell Sensor", "Self %f", result);

      generateRandomArray(test2_arr);
      double result1 = conv2.calculateCrossCorrelation(test_arr, test2_arr, 512);
      ESP_LOGI("Doorbell Sensor", "Diff %f", result1);
    }
    delay(1000);
    
  }
};
