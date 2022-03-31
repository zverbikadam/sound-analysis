#include "config.h"
#include <esphome.h>
#include <driver/i2s.h>
#include <convolution.h>
#include <Preferences.h>

Preferences prefs;


bool isButtonPressed;

int32_t *learning_buffer;
double convolution_core[SAMPLES] = { 0 };

int32_t input_signal[SAMPLES];
double processed_input_signal[SAMPLES];

Convolution conv(processed_input_signal, convolution_core, SAMPLES);

void init_i2s() {
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

static void process_input_signal(int32_t *input, double *result)
{
    for (int i = 0; i < SAMPLES; i++)
    {
        result[i] = (input[i] >> 16) / 1.0;
    }
}

void read_data(int32_t *buffer, size_t size) {
  uint32_t bytes_read = 0;
  i2s_read(I2S_PORT, (void *) buffer, size, &bytes_read, portMAX_DELAY);
}

void read_data_learn(int32_t *buffer, size_t size) {
  ESP_LOGI("Doorbell Sensor", "Button pressed -> recording new sample signal...");
  uint32_t bytes_read = 0;
  i2s_read(I2S_PORT, (void *) buffer, size, &bytes_read, portMAX_DELAY);
}

void create_convolution_core(int32_t *input, double *result) {
  for (int i = 0; i < 512; i++) {
    result[i+511] = (input[i] >> 16) / 1.0;
  }
}

void save_to_memory(int32_t * buffer, size_t size) {
  prefs.clear();
  prefs.putBytes("signal", (void *) buffer, size);
  ESP_LOGI("Doorbell Signal", "Recording of new sample signal done...");
  delete[] buffer;
  prefs.end();
  ESP.restart();
}

void analyze() {
  // TODO
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
    // get signal from flash
    prefs.getBytes("signal", learning_buffer, signal_length);
    // convert to convolution core
    create_convolution_core(learning_buffer, convolution_core);

    pinMode(PIN_BUTTON, INPUT);
    isButtonPressed = false;

    delay(500);
  }

  void loop() override {

    // check if button is pressed
    isButtonPressed = digitalRead(PIN_BUTTON);
    if (isButtonPressed) {
      read_data_learn(learning_buffer, (sizeof(int32_t) * (SAMPLES/4)));
      save_to_memory(learning_buffer, (sizeof(int32_t)*(SAMPLES/4)));
    }

    // read data
    read_data(input_signal, sizeof(input_signal));
    process_input_signal(input_signal, processed_input_signal);

    // analyze data
    analyze();
  }
};
