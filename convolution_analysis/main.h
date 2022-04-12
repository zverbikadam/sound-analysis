#include "config.h"
#include <esphome.h>
#include <driver/i2s.h>
#include <Preferences.h>

Preferences prefs;

static bool isButtonPressed;
static bool lastButtonState = false;
static bool wasDetected;

static int32_t learning_buffer[SAVED_SIGNAL_SAMPLES] = { 0 };
static int16_t maximum_amplitude_in_learning_buffer = 0;

static int32_t input_signal[INPUT_SIGNAL_SAMPLES];

static double max_correlation_value;

const float delta = 0.8;

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

int16_t process_signal_and_get_max_amplitude(int32_t *signal, int number_of_samples)
{
  int16_t max = 0;
    for (int i = 0; i < number_of_samples; i++)
    {
        signal[i] = signal[i] >> 16;
        if (signal[i] > max) {
          max = signal[i];
        }
    }

    return max;
}

void read_data(int32_t *signal, size_t signal_size) {
  uint32_t bytes_read = 0;
  i2s_read(I2S_PORT, (void *) signal, signal_size, &bytes_read, portMAX_DELAY);
}

void save_to_memory(int32_t *signal, size_t size, double correlation_value) {
  prefs.clear();
  ESP_LOGI("Doorbell Signal", "Saving data to memory...");
  prefs.putBytes("signal", (void *) signal, size);
  prefs.putDouble("corr-value", correlation_value);
}

double calculateCorrelationIndex(int index, float ratio_koefficient) {
    if (ratio_koefficient == NULL) {
        ratio_koefficient = 1.0;
    }
    double result = 0;

    for (int i = index; i < index + SAVED_SIGNAL_SAMPLES; i++) {
        result += (input_signal[i] * (learning_buffer[i-index] / ratio_koefficient));
    }

    return (result / SAVED_SIGNAL_SAMPLES);
}

double calculateCorrelationIndex(int32_t *first_signal, int32_t *second_signal, int index, float ratio_koefficient) {
    if (ratio_koefficient == NULL) {
        ratio_koefficient = 1.0;
    }
    double result = 0;

    for (int i = index; i < index + SAVED_SIGNAL_SAMPLES; i++) {
        result += (first_signal[i] * (second_signal[i-index] / ratio_koefficient));
    }

    return (result / SAVED_SIGNAL_SAMPLES);
}

double analyze(float ratio) {
  // TODO
  double result = 0;
  double max = 0;
  for (int i = 0; i <INPUT_SIGNAL_SAMPLES - SAVED_SIGNAL_SAMPLES; i++) {
    result = calculateCorrelationIndex(i, ratio);
    if (result > max) {
      max = result;
    }
  }
  return max;
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

    prefs.begin("audio", false); // false stands for read-write mode; true is for read-only
    
    size_t signal_length = prefs.getBytes("signal", NULL, NULL);
    if (signal_length > 0) {
      // get signal from flash memory
      if (signal_length <= sizeof(learning_buffer)) {
        prefs.getBytes("signal", learning_buffer, signal_length);
        maximum_amplitude_in_learning_buffer = process_signal_and_get_max_amplitude(learning_buffer, SAVED_SIGNAL_SAMPLES);
      } else {
        ESP_LOGE("Doorbell Sensor", "Saved signal size is bigger than learning_buffer. Clearing memory and restarting ESP...");
        prefs.clear();
        ESP.restart();
      }
    }
    
    max_correlation_value = prefs.getDouble("corr-value", 0.0); 

    wasDetected = false;
    
    pinMode(PIN_BUTTON, INPUT);

    delay(500);
  }

  void loop() override {

    // check if button is pressed
    isButtonPressed = digitalRead(PIN_BUTTON);
    if (isButtonPressed && !lastButtonState) {
      lastButtonState = true;
      ESP_LOGI("Doorbell Sensor", "Button pressed -> recording new sample signal...");
      delay(500);

      read_data(learning_buffer, sizeof(learning_buffer));
      maximum_amplitude_in_learning_buffer = process_signal_and_get_max_amplitude(learning_buffer, SAVED_SIGNAL_SAMPLES);
      
      max_correlation_value = calculateCorrelationIndex(learning_buffer, learning_buffer, 0, NULL);
      
      save_to_memory(learning_buffer, sizeof(learning_buffer), max_correlation_value);
    } else {
      lastButtonState = false;
      // read data
      read_data(input_signal, sizeof(input_signal));
      int16_t max_amplitude = process_signal_and_get_max_amplitude(input_signal, INPUT_SIGNAL_SAMPLES);

      float amplitude_ratio = (float) max_amplitude / maximum_amplitude_in_learning_buffer;

      // analyze data
      if (analyze(amplitude_ratio) > (max_correlation_value * delta)) {
        wasDetected = true;
        convolution_recognition_sensor->publish_state(1);
      } else {
        wasDetected = false;
        convolution_recognition_sensor->publish_state(0);
      }

      if (wasDetected) {
        delay(2000);
      }
    }
  }
};
