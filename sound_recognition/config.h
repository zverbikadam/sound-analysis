#ifndef config_h
#define config_h

// Set pins
#define PIN_I2S_WS GPIO_NUM_22  // Word Select
#define PIN_I2S_SD GPIO_NUM_21  // Serial Data
#define PIN_I2S_SCK GPIO_NUM_26 // Serial Clock

// Config I2S
#define I2S_PORT I2S_NUM_0
#define I2S_READ_LEN (16 * 1024)

// Config DMA
#define DMA_BUF_COUNT 16 
#define DMA_BUF_LEN 64

// Config audio
#define BITS_PER_SAMPLE 16
#define SAMPLE_RATE 16000
#define NUM_CHANNELS 1

#define SAMPLES 1024
#define OCTAVES 9

#endif
