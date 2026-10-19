#pragma once
// =============================================================================
//  INDUSTRIAL DOCTOR v3.0  —  DYNAMIC FEATURE EXTRACTION
//  Responsibility: Raw I2S Stream → 8 Standardized Signal Features (Floats)
// =============================================================================

#include <Arduino.h>
#include <driver/i2s.h>

struct MotorFeatures {
    float rms;              // Root Mean Square (Signal Energy)
    float rmsStab;          // Rolling Standard Deviation of last 10 windows (Amplitude Stability)
    float peakFreq;         // Dominant Frequency via Goertzel Scan (Motor Heartbeat)
    float kurtosis;         // Statistical Fourth Moment (Impulse/Impact Event Detector)
    float crestFactor;      // Peak / RMS ratio (Sharp Non-Repetitive Shock Detector)
    float spectralCentroid; // Spectral Center of Mass ("Brightness" / Friction Hiss)
    float zcr;              // Zero Crossing Rate (Rhythmicity vs Chaotic White Noise)
    float envelopeRms;      // RMS of the high-frequency demodulated rectified envelope
    bool  dataValid;

    void toArray(float* array) const {
        array[0] = rms;
        array[1] = rmsStab;
        array[2] = peakFreq;
        array[3] = kurtosis;
        array[4] = crestFactor;
        array[5] = spectralCentroid;
        array[6] = zcr;
        array[7] = envelopeRms;
    }
};

class FeatureExtractor {
public:
    FeatureExtractor();
    ~FeatureExtractor();

    // Configuration & Allocation
    void setupI2S(); // PUBLIC: Can now be called from main.cpp safely!
    bool begin();
    MotorFeatures getNextFeatures();

    // Constant definition for structural dimensions downstream
    static constexpr int N_FEATURES = 8;

private:
    // Hardware Peripherals (I2S DMA & Pins)
    static constexpr i2s_port_t kI2SPort       = I2S_NUM_0;
    static constexpr int        kI2SWs          = 13;
    static constexpr int        kI2SSd          = 11;
    static constexpr int        kI2SSck         = 12;
    
    // Signal Sampling Context
    static constexpr size_t     kSamples        = 1024;
    static constexpr float      kSampleRate     = 16000.0f;

    // Mathematical Configuration
    static constexpr float      kMinFrequency   = 20.0f;
    static constexpr float      kMaxFrequency   = 7500.0f;
    static constexpr float      kFrequencyStep  = 50.0f;
    static constexpr float      kZcrNoiseFloor  = 80.0f; // Elimination of low-level DC offset fluttering
    static constexpr int        kRmsStabWindow  = 10;
    static constexpr float      kEpsilon        = 1e-12f;

    // Internal Memory Pipelines
    bool    _initialized;
    int32_t _rawBuffer[kSamples];
    float   _normalizedBuffer[kSamples];
    float   _windowedBuffer[kSamples];

    // Tracking history states
    float   _rmsHistory[kRmsStabWindow];
    int     _rmsHistIdx;
    int     _rmsHistCount;
    float   _lastFilteredEnvelope;

    // Internal DSP Stages
    void  applyHannWindow();
    float updateRmsStab(float currentRms);
    int   signWithHysteresis(float sample, float threshold);
    void  computeSpectralMetrics(float* peakFreq, float* spectralCentroid);
};