#include <Arduino.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>

// --- I2S Pins ---
#define I2S_WS 13
#define I2S_SD 11
#define I2S_SCK 12
#define I2S_PORT I2S_NUM_0
#define SAMPLES 1024             
#define SAMPLING_FREQ 16000      

struct MachineData {
  String name;
  int machineType;
  float rpm;
  int components;
  float bearingFactor;
};

// Current Test: Aqua-Ton Pump
MachineData currentMachine = { "Aqua-Ton Pump", 1, 2880.0, 6, 3.8 };

double *vReal;
double *vImag;
ArduinoFFT<double> FFT = ArduinoFFT<double>();

void setup() {
  Serial.begin(115200);
  delay(500);
  // Standard I2S config for INMP441
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLING_FREQ, // 16000
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64
  };
  
  i2s_pin_config_t pin_config = { 
    .bck_io_num = I2S_SCK, 
    .ws_io_num = I2S_WS, 
    .data_out_num = -1, 
    .data_in_num = I2S_SD 
  };
  
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
}

void loop() {
  const int num_samples = 512;
  int32_t raw_samples[num_samples];
  size_t bytes_read;

  // Read a whole block of samples (much faster than reading one by one)
  i2s_read(I2S_PORT, &raw_samples, sizeof(raw_samples), &bytes_read, portMAX_DELAY);

  int samples_read = bytes_read / sizeof(int32_t);

  for (int i = 0; i < samples_read; i++) {
    // Shift and print
    int16_t sample = (int16_t)(raw_samples[i] >> 14);
    Serial.println(sample);
  }
}