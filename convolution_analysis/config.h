#ifndef config_h
#define config_h

// Set pins
#define PIN_I2S_WS GPIO_NUM_22  // Word Select
#define PIN_I2S_SD GPIO_NUM_21  // Serial Data
#define PIN_I2S_SCK GPIO_NUM_26 // Serial Clock

#define PIN_BUTTON GPIO_NUM_4   // Pull-down button

// Config I2S
#define I2S_PORT I2S_NUM_0
#define I2S_READ_LEN (8 * 1024)

// Config DMA
#define DMA_BUF_COUNT 8 
#define DMA_BUF_LEN 32

// Config audio
#define BITS_PER_SAMPLE 8
#define SAMPLE_RATE 8000
#define NUM_CHANNELS 1

//Number of measurements we use for calculation
// The higher the number of samples, the more accurate frequencies we can detect. However, more samples = more computation

#define SAMPLES 2048    // always power of 2

#endif