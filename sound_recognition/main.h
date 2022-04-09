#include "config.h"
#include <esphome.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>
#include <math.h>

const byte NUMBER_OF_TOP_FREQUENCIES = 4;
// 1 FFT = 0.064s, so 8*0.064 = 0.512 (half of second)
const byte NUMBER_OF_SAMPLES_IN_TIME = 10;

const int DOORBELL_FEQUENCIES[5] = {690, 960, 2070, 2900, 3440};

double **major_frequencies_in_time;
const byte range = 30;

int counter;
bool wasDetected;

uint32_t buffer32[SAMPLES];

static double real[SAMPLES];
static double imag[SAMPLES] = { 0.0 };
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

static void process_signal(uint32_t *input, double *result, int number_of_samples)
{
    for (int i = 0; i < number_of_samples; i++)
    {
        result[i] = (input[i] >> 16) / 1.0;
    }
}

void read_data(uint32_t *signal, size_t signal_size)
{
  uint32_t bytes_read = 0;
  i2s_read(I2S_PORT, (void *) signal, signal_size, &bytes_read, portMAX_DELAY);
}

// calculates energy from Re and Im parts and places it back in the Re part (Im part is zeroed)
static void calculate_energy(double *vReal, double *vImag, uint16_t samples)
{
    for (uint16_t i = 0; i < samples; i++)
    {
        vReal[i] = sqrt(sq(vReal[i]) + sq(vImag[i]));
        vImag[i] = 0.0;
    }
}

bool is_within_range(int peak) {
  if ((peak >= DOORBELL_FEQUENCIES[0] - range) && (peak <= DOORBELL_FEQUENCIES[0] + range)) 
    return true;
  if ((peak >= DOORBELL_FEQUENCIES[1] - range) && (peak <= DOORBELL_FEQUENCIES[1] + range)) 
    return true;
  if ((peak >= DOORBELL_FEQUENCIES[2] - range) && (peak <= DOORBELL_FEQUENCIES[2] + range)) 
    return true;
  if ((peak >= DOORBELL_FEQUENCIES[3] - range) && (peak <= DOORBELL_FEQUENCIES[3] + range)) 
    return true;
  if ((peak >= DOORBELL_FEQUENCIES[4] - range) && (peak <= DOORBELL_FEQUENCIES[4] + range)) 
    return true;
  return false;
}

bool is_doorbell_detected(double **frequencies_in_time)
{

  int result = 0;
  for (int i = 0; i < NUMBER_OF_SAMPLES_IN_TIME; i++) {
    for (int j = 0; j < NUMBER_OF_TOP_FREQUENCIES; j++) {
      int peak = (int) floor(frequencies_in_time[i][j]);
      if(is_within_range(peak)) {
        result++;
      }
    }
  }
  if ((result >= (int) NUMBER_OF_SAMPLES_IN_TIME * 1.5)) {
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
    wasDetected = false;
    major_frequencies_in_time = new double*[NUMBER_OF_SAMPLES_IN_TIME];
    for(int i = 0; i < NUMBER_OF_SAMPLES_IN_TIME; i++) {
      major_frequencies_in_time[i] = new double[NUMBER_OF_TOP_FREQUENCIES];
    }
    if (major_frequencies_in_time == NULL) {
      ESP_LOGE("Sound Sensor", "Not enough memory.");
    }

    delay(500);
  }

  void loop() override {
    // read data from i2s
    read_data(buffer32, sizeof(buffer32));

    process_signal(buffer32, real, SAMPLES);

    // FFT_WIN_TYP_FLT_TOP - flat top windowing method; FFT_FORWARD = 0x01
    fft.Windowing(FFT_WIN_TYP_FLT_TOP, FFT_FORWARD);
    fft.Compute(FFT_FORWARD);

    // calculate energy in each bin
    calculate_energy(real, imag, SAMPLES);


    // get 4 frequencies with biggest amplitude
    fft.TopPeaks(major_frequencies_in_time[counter], NUMBER_OF_TOP_FREQUENCIES);

    if(is_doorbell_detected(major_frequencies_in_time)) {
      if (!wasDetected) {
        wasDetected = true;
        sound_recognition_sensor->publish_state(1);
      }
    }
    else {
      wasDetected = false;
      sound_recognition_sensor->publish_state(0);
    }

    if (counter > 6) {
      counter = 0;
    } else {
      counter++;
    }
  }
};
