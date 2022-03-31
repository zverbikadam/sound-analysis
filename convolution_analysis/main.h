#include "config.h"
#include <esphome.h>
#include <driver/i2s.h>
#include <convolution.h>
#include <Preferences.h>

Preferences prefs;


bool isButtonPressed;

int32_t *learning_buffer;
float convolution_core[SAMPLES] = { 0 };
int32_t input_signal[SAMPLES];

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

// static void prepare_for_fft(uint32_t *signal, double *vReal, double *vImag, uint16_t number_of_samples)
// {
//     for (uint16_t i = 0; i < number_of_samples; i++)
//     {
//         vReal[i] = (signal[i] >> 16) / 1.0;
//         vImag[i] = 0.0;
//     }
// }

void read_data()
{
  uint32_t bytes_read = 0;
  i2s_read(I2S_PORT, (void *) input_signal, sizeof(input_signal), &bytes_read, portMAX_DELAY);
}

void read_data_learn()
{
  
  uint32_t bytes_read = 0;
  i2s_read(I2S_PORT, (void *) learning_buffer, (sizeof(int32_t) * (SAMPLES/4)), &bytes_read, portMAX_DELAY);
}

void create_convolution_core() {
  for (int i = 0; i < 512; i++) {
    convolution_core[i+511] = (learning_buffer[i] >> 16) / 1.0;
  }
}

void save_to_memory() {
  // prefs.begin("audio", false);
  prefs.clear();
  // // // save value
  prefs.putBytes("signal", (void *) learning_buffer, (sizeof(int32_t)*(SAMPLES/4)));

  delete[] learning_buffer;
  prefs.end();
  ESP.restart();

}

class ConvolutionSensor : public Component, public BinarySensor {
 public:

// binary sensor with states 0 -> not recognized; 1 -> recognized
BinarySensor *convolution_recognition_sensor = new BinarySensor();

// For components that should be initialized after a data connection (API/MQTT) is connected
float get_setup_priority() const override { return esphome::setup_priority::AFTER_CONNECTION; }

  void setup() override {

    prefs.begin("audio", false); // false stands for read-write mode; true is for read-only
    
    // start up the I2S peripheral
    ESP_LOGI("Doorbell Sensor", "Configuring I2S...");
    init_i2s();

    
    size_t signal_length = prefs.getBytes("signal", NULL, NULL);
    ESP_LOGI("Doorbell Sensor", "Signal length %d", signal_length);
    learning_buffer = new int32_t[signal_length];
    if (learning_buffer == NULL) {
      ESP_LOGE("Doorbell Sensor", "Not enough memory.");
      while(1);
    }
    
    prefs.getBytes("signal", learning_buffer, signal_length);

    pinMode(PIN_BUTTON, INPUT);
    isButtonPressed = false;


    delay(500);
  }

  void loop() override {

    // check if button is pressed
    isButtonPressed = digitalRead(PIN_BUTTON);
    if (isButtonPressed) {
      read_data_learn();
      save_to_memory();
    }
  }
};
