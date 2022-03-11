#include "config.h"
#include <esphome.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>

uint32_t buffer32[SAMPLES];
ushort counter;
short result[4];

static double real[SAMPLES];
static double imag[SAMPLES];
static arduinoFFT fft(real, imag, (uint16_t) SAMPLES, (double) SAMPLE_RATE);

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

// calculates energy from Re and Im parts and places it back in the Re part (Im part is zeroed)
static void calculate_energy(double *vReal, double *vImag, uint16_t samples)
{
    for (uint16_t i = 0; i < samples; i++)
    {
        vReal[i] = sq(vReal[i]) + sq(vImag[i]);
        vImag[i] = 0.0;
    }
}

bool detect_frequency(double peak, unsigned int bin)
{
    if (peak >= bin - 3 && peak <= bin + 3)
    {
        return true;
    }
    return false;
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

    counter = 0;

    delay(500);
  }

  void loop() override {

    for (ushort i = 0; i < 10; i++) {
      read_data();
      prepare_for_fft(buffer32, real, imag, SAMPLES);
      fft.Windowing(FFT_WIN_TYP_FLT_TOP, FFT_FORWARD);
      fft.Compute(FFT_FORWARD);
      calculate_energy(real, imag, SAMPLES);
      unsigned int peak = (int)floor(fft.MajorPeak());
      if (counter % 2 == 0) {
        if (detect_frequency(peak, 44) || detect_frequency(peak, 132)) {
          // ESP_LOGI("Sound Sensor", "Detected first freq");
          result[counter] = 1;
          counter++;
          continue;
        }
        else if (detect_frequency(peak, 61) || detect_frequency(peak, 185)) {
          continue;
        }
        else {
          counter = 0;
          break;
        }
      }
      else {
        if (detect_frequency(peak, 61) || detect_frequency(peak, 185)) {
          //  ESP_LOGI("Sound Sensor", "Detected second freq");
          result[counter] = 2;
          counter++;
          continue;
        }
        else if (detect_frequency(peak, 44) || detect_frequency(peak, 132)) {
          continue;
        }
        else {
          counter = 0;
          break;
        }
      }
    }
    if (result[0] == 1 && result[1] == 2 && result[2] == 1 && result[3] == 2) {
      ESP_LOGI("Sound Sensor", "Doorbell recognized");
      result[0] = 0;
    }
    // // read data from i2s
    // read_data();

    // prepare_for_fft(buffer32, real, imag, SAMPLES);

    // // FFT_WIN_TYP_FLT_TOP - flat top windowing method; FFT_FORWARD = 0x01
    // fft.Windowing(FFT_WIN_TYP_FLT_TOP, FFT_FORWARD);
    // fft.Compute(FFT_FORWARD);

    // // calculate energy in each bin
    // calculate_energy(real, imag, SAMPLES);

    // unsigned int peak = (int)floor(fft.MajorPeak());
    // // ESP_LOGI("Sound Sensor", "%d", peak);
    
    // // detecting 867 Hz
    // if (detect_frequency(peak, 182))
    // {
    //     ESP_LOGI("Sound Sensor", "Frequency detected");
    // }
  }
};
