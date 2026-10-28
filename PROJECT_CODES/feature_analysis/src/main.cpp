#include <Arduino.h>
#include "feature_data.h"

// Instantiate the extractor
FeatureExtractor featureExtractor;

void setup() {
  // Use a very high baud rate so Serial doesn't become a bottleneck
  Serial.begin(2000000); 
  delay(1000);

  Serial.println("\n--- INDUSTRIAL DOCTOR: DATA EXTRACTION MODE ---");
  
  if (!featureExtractor.begin()) {
    Serial.println("FATAL_ERROR: I2S Initialization Failed");
    while(1); // Stop here if hardware isn't responding
  }

  // Print the CSV Header
  // This helps us identify what each number represents
  Serial.println("RMS,RMS_Stab,Kurt,Kurt_Stab,PeakHz,Crest,Centroid,ZCR");
}

void loop() {
  // The extractor blocks until 1024 samples are ready (approx 64ms)
  const MotorFeatures f = featureExtractor.getNextFeatures();

  if (f.dataValid) {
    // Print values in a clean CSV format for easy copy-pasting
    Serial.printf("%.3f,%.3f,%.4f,%.4f,%.1f,%.3f,%.1f,%.5f\n", 
                  f.rms, 
                  f.rmsStability, 
                  f.kurtosis, 
                  f.kurtosisStability, 
                  f.peakFreq, 
                  f.crestFactor, 
                  f.spectralCentroid, 
                  f.zcr);
  } else {
    // If something goes wrong with the DMA/I2S stream
    Serial.println("0,0,0,0,0,0,0,0");
  }

  // No delay needed here! getNextFeatures() handles the timing.
}