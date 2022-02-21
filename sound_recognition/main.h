#include "config.h"
#include <esphome.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>

int32_t buffer32[SAMPLES];

const int BLOCK_SIZE = SAMPLES;
// our FFT data
static double real[SAMPLES];
static double imag[SAMPLES];
static arduinoFFT fft(real, imag, (uint16_t) SAMPLES, (double) SAMPLES);
static float energy[OCTAVES];
// A-weighting curve from 31.5 Hz ... 8000 Hz
static const float aweighting[] = {-39.4, -26.2, -16.1, -8.6, -3.2, 0.0, 1.2, 1.0, -1.1};

static unsigned int wine_glass = 0;

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

static void integerToFloat(int32_t *integer, double *vReal, double *vImag, uint16_t samples)
{
    for (uint16_t i = 0; i < samples; i++)
    {
        vReal[i] = (integer[i] >> 16) / 10.0;
        vImag[i] = 0.0;
    }
}

void read_data()
{
  uint32_t bytes_read = 0;
  uint32_t samples_written = 0;
  uint16_t samples_read;
  i2s_read(I2S_PORT, (void *) buffer32, sizeof(buffer32), &bytes_read, portMAX_DELAY);

  samples_read = bytes_read >> 2;

  // for (uint32_t i = 0; i < samples_read; i++)
  // {
  //   uint8_t lsb = buffer32[i * 4 + 2];
  //   uint8_t msb = buffer32[i * 4 + 3];
  //   uint16_t sample = ((((uint16_t)msb) << 8) | ((uint16_t)lsb)) << 4;
  //   if (samples_written < sizeof(samples_for_fft)) {
  //     // SAVE SAMPLES TO SOME ARRAY
  //     samples_written++;
  //   }
  //   else break;
  // }
  // prepare data for fft
  integerToFloat(buffer32, real, imag, SAMPLES);
}

// calculates energy from Re and Im parts and places it back in the Re part (Im part is zeroed)
static void calculateEnergy(double *vReal, double *vImag, uint16_t samples)
{
    for (uint16_t i = 0; i < samples; i++)
    {
        vReal[i] = sq(vReal[i]) + sq(vImag[i]);
        vImag[i] = 0.0;
    }
}

// sums up energy in bins per octave
static void sumEnergy(double *bins, float *energies, int bin_size, int num_octaves)
{
    // skip the first bin
    int bin = bin_size;
    for (int octave = 0; octave < num_octaves; octave++)
    {
        float sum = 0.0;
        for (int i = 0; i < bin_size; i++)
        {
            sum += real[bin++];
        }
        energies[octave] = sum;
        bin_size *= 2;
    }
}

static float decibel(float v)
{
    return 10.0 * log(v) / log(10);
}

// converts energy to logaritmic, returns A-weighted sum
static float calculateLoudness(float *energies, const float *weights, int num_octaves, float scale)
{
    float sum = 0.0;
    for (int i = 0; i < num_octaves; i++)
    {
        float energy = scale * energies[i];
        sum += energy * pow(10, weights[i] / 10.0);
        energies[i] = decibel(energy);
    }
    return decibel(sum);
}

unsigned int countSetBits(unsigned int n)
{
    unsigned int count = 0;
    while (n)
    {
        count += n & 1;
        n >>= 1;
    }
    return count;
}

bool detectFrequency(unsigned int *mem, unsigned int minMatch, double peak, unsigned int bin1, unsigned int bin2, bool wide)
{
    if (peak == bin1 || peak == bin2 || (wide && (peak == bin1 + 1 || peak == bin1 - 1 || peak == bin2 + 1 || peak == bin2 - 1)))
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

    delay(500);
  }

  void loop() override {
    read_data();

    // FFT - flat top windowing method; FFT_FORWARD = 0x01
    fft.Windowing(FFT_WIN_TYP_FLT_TOP, FFT_FORWARD);
    fft.Compute(FFT_FORWARD);

    // calculate energy in each bin
    calculateEnergy(real, imag, SAMPLES);

    // sum up energy in bin for each octave
    sumEnergy(real, energy, 1, OCTAVES);

    // calculate loudness per octave + A weighted loudness
    float loudness = calculateLoudness(energy, aweighting, OCTAVES, 1.0);
    unsigned int peak = (int)floor(fft.MajorPeak());

    // detecting 1kHz and 1.5kHz
    if (detectFrequency(&wine_glass, 15, peak, 39, 40, true))
    {
        ESP_LOGI("Sound Sensor", "Detected wine glass, cheers!");
    }
  }
};