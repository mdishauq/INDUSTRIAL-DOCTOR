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
    double rms;              // Root Mean Square — signal energy
    double rmsStab;          // Rolling std of last 10 RMS values — amplitude stability
    double peakFreq;         // Dominant frequency via Goertzel — motor "heartbeat"
    double kurtosis;         // Statistical peakiness — impulse events
    double crestFactor;      // Peak / RMS — non-repetitive shock detector
    double spectralCentroid; // Spectral center of mass — "brightness" of vibration
    double zcr;              // Zero Crossing Rate — rhythmicity vs chaos
    bool   dataValid;
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
    static constexpr double     kSampleRate     = 16000.0;

    // ── Spectral scan range ────────────────────────────────────────────────────
    static constexpr double     kMinFrequency   = 20.0;
    static constexpr double     kMaxFrequency   = 2000.0;
    static constexpr double     kFrequencyStep  = 10.0;

    // ── ZCR hysteresis floor (eliminates DC noise crossings) ──────────────────
    static constexpr double     kZcrNoiseFloor  = 80.0;

    // ── RMS stability window (how many past RMS values to track) ──────────────
    static constexpr int        kRmsStabWindow  = 10;

    static constexpr double     kEpsilon        = 1e-12;

    void   setupI2S();
    void   computeSpectralMetrics(const double* samples, size_t count,
                                   double sampleRate,
                                   double* peakFreq,
                                   double* spectralCentroid);
    int    signWithHysteresis(double sample, double threshold);
    double updateRmsStab(double newRms);

    bool   _initialized;

    // Circular buffer for RMS stability tracking
    double _rmsHistory[kRmsStabWindow];
    int    _rmsHistIdx;
    int    _rmsHistCount;
};