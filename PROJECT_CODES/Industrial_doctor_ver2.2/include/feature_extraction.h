#pragma once
// ============================================================
//  INDUSTRIAL DOCTOR v2.0
//  feature_extraction.h
//
//  Responsibility: Raw I2S audio → 7 feature numbers.
//  Nothing here knows about Z-scores, calibration, or faults.
// ============================================================
#include <Arduino.h>
#include <driver/i2s.h>

struct MotorFeatures {
    float rms;              // Root Mean Square — signal energy
    float rmsStab;          // Rolling std of last 10 RMS values — amplitude stability
    float peakFreq;         // Dominant frequency via Goertzel — motor "heartbeat"
    float kurtosis;         // Statistical peakiness — impulse events
    float crestFactor;      // Peak / RMS — non-repetitive shock detector
    float spectralCentroid; // Spectral center of mass — "brightness" of vibration
    float zcr;              // Zero Crossing Rate — rhythmicity vs chaos
    float envelopeRms;      // High-Frequency envelope analysis
    bool  dataValid;
    void toArray(float* array) const;
};

class FeatureExtractor {
public:
    FeatureExtractor();
    bool          begin();
    MotorFeatures getNextFeatures();

private:
    // ── I2S pin / config constants ─────────────────────────────────────────────
    static constexpr i2s_port_t kI2SPort       = I2S_NUM_0;
    static constexpr int        kI2SWs          = 13;
    static constexpr int        kI2SSd          = 11;
    static constexpr int        kI2SSck         = 12;
    static constexpr size_t     kSamples        = 1024;
    static constexpr float      kSampleRate     = 16000.0f;

    // ── Spectral scan range ────────────────────────────────────────────────────
    static constexpr float      kMinFrequency   = 20.0f;
    static constexpr float      kMaxFrequency   = 7500.0f;
    static constexpr float      kFrequencyStep  = 50.0f;

    // ── ZCR hysteresis floor (eliminates DC noise crossings) ──────────────────
    static constexpr float      kZcrNoiseFloor  = 80.0f;

    // ── RMS stability window (how many past RMS values to track) ──────────────
    static constexpr int        kRmsStabWindow  = 10;

    static constexpr float      kEpsilon        = 1e-12f;

    void   setupI2S();
    void   computeSpectralMetrics(const float* samples, size_t count,
                                   float sampleRate,
                                   float* peakFreq,
                                   float* spectralCentroid);
    int    signWithHysteresis(float sample, float threshold);
    float  updateRmsStab(float newRms);

    bool   _initialized;

    // Circular buffer for RMS stability tracking
    float  _rmsHistory[kRmsStabWindow];
    int    _rmsHistIdx;
    int    _rmsHistCount;
};