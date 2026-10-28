// ============================================================
//  INDUSTRIAL DOCTOR v2.0
//  feature_extraction.cpp
//
//  I2S setup + all 7 feature computations.
//  This file is intentionally "dumb" — no AI logic here.
// ============================================================
#include "feature_extraction.h"
#include <math.h>
#include <string.h>

// ── Static audio buffers (kept off the stack) ─────────────────────────────────
namespace {
    static int32_t rawBuffer[1024];
    static double  normalizedBuffer[1024];
    static double  windowedBuffer[1024];
}

// ─────────────────────────────────────────────────────────────────────────────
FeatureExtractor::FeatureExtractor()
    : _initialized(false), _rmsHistIdx(0), _rmsHistCount(0)
{
    memset(_rmsHistory, 0, sizeof(_rmsHistory));
}

// ─────────────────────────────────────────────────────────────────────────────
void FeatureExtractor::setupI2S() {
    const i2s_config_t i2sCfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate          = (uint32_t)kSampleRate,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 8,
        .dma_buf_len          = kSamples,
        .use_apll             = false
    };
    const i2s_pin_config_t pins = {
        .bck_io_num    = kI2SSck,
        .ws_io_num     = kI2SWs,
        .data_out_num  = -1,
        .data_in_num   = kI2SSd
    };
    i2s_driver_install(kI2SPort, &i2sCfg, 0, nullptr);
    i2s_set_pin(kI2SPort, &pins);
}

// ─────────────────────────────────────────────────────────────────────────────
bool FeatureExtractor::begin() {
    setupI2S();
    _initialized = true;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  signWithHysteresis — returns ±1 only when signal clearly crosses threshold.
//  Prevents thermal noise from generating false zero-crossings in dead silence.
// ─────────────────────────────────────────────────────────────────────────────
int FeatureExtractor::signWithHysteresis(double sample, double threshold) {
    if (sample >  threshold) return  1;
    if (sample < -threshold) return -1;
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  updateRmsStab — maintains a circular buffer of the last kRmsStabWindow RMS
//  values and returns their standard deviation.  This "stability" metric is the
//  primary crowd-noise and friction detector.
// ─────────────────────────────────────────────────────────────────────────────
double FeatureExtractor::updateRmsStab(double newRms) {
    _rmsHistory[_rmsHistIdx] = newRms;
    _rmsHistIdx = (_rmsHistIdx + 1) % kRmsStabWindow;
    if (_rmsHistCount < kRmsStabWindow) _rmsHistCount++;

    double sum = 0.0, sumSq = 0.0;
    for (int i = 0; i < _rmsHistCount; i++) {
        sum   += _rmsHistory[i];
        sumSq += _rmsHistory[i] * _rmsHistory[i];
    }
    double mean = sum / (double)_rmsHistCount;
    double var  = sumSq / (double)_rmsHistCount - mean * mean;
    return (var > 0.0) ? sqrt(var) : 0.0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  computeSpectralMetrics — Goertzel algorithm swept across 20–2000 Hz.
//  Returns the dominant frequency (Peak Frequency) and the spectral centroid
//  (power-weighted average frequency).  Goertzel is used instead of full FFT
//  because it is more efficient when scanning a specific frequency range.
// ─────────────────────────────────────────────────────────────────────────────
void FeatureExtractor::computeSpectralMetrics(const double* samples, size_t count,
                                               double sampleRate,
                                               double* peakFreq,
                                               double* spectralCentroid) {
    double bestFreq  = 0.0, bestPower = 0.0;
    double wPowerSum = 0.0, powerSum  = 0.0;

    for (double freq = kMinFrequency; freq <= kMaxFrequency; freq += kFrequencyStep) {
        const double omega = 2.0 * PI * freq / sampleRate;
        const double coeff = 2.0 * cos(omega);
        double s1 = 0.0, s2 = 0.0;

        for (size_t i = 0; i < count; i++) {
            const double s = samples[i] + coeff * s1 - s2;
            s2 = s1;
            s1 = s;
        }

        const double power = s2*s2 + s1*s1 - coeff*s1*s2;

        if (power > kEpsilon) {
            wPowerSum += freq * power;
            powerSum  += power;
        }
        if (power > bestPower) {
            bestPower = power;
            bestFreq  = freq;
        }
    }

    *peakFreq         = bestFreq;
    *spectralCentroid = (powerSum > kEpsilon) ? (wPowerSum / powerSum) : 0.0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  getNextFeatures — blocks until one full 1024-sample window is available,
//  then computes all 7 features in a single pass where possible.
// ─────────────────────────────────────────────────────────────────────────────
MotorFeatures FeatureExtractor::getNextFeatures() {
    MotorFeatures f = {0, 0, 0, 0, 0, 0, 0, false};
    if (!_initialized) return f;

    size_t bytesRead = 0;
    const esp_err_t err = i2s_read(kI2SPort, rawBuffer,
                                    sizeof(rawBuffer), &bytesRead, portMAX_DELAY);
    if (err != ESP_OK || bytesRead < sizeof(rawBuffer)) return f;

    // ── Single pass: normalize, window, RMS, ZCR, peak amplitude ─────────────
    double sum    = 0.0;
    double sumSq  = 0.0;
    double maxAbs = 0.0;
    int    zeroCrossings = 0;
    int    prevSign      = 0;

    const double twoPi     = 2.0 * PI;
    const double winDenom  = (double)(kSamples - 1);

    for (size_t i = 0; i < kSamples; i++) {
        normalizedBuffer[i] = (double)(rawBuffer[i] >> 16);

        sum   += normalizedBuffer[i];
        sumSq += normalizedBuffer[i] * normalizedBuffer[i];

        // Hamming window for spectral leakage reduction
        const double w = 0.54 - 0.46 * cos(twoPi * (double)i / winDenom);
        windowedBuffer[i] = normalizedBuffer[i] * w;

        const double a = fabs(normalizedBuffer[i]);
        if (a > maxAbs) maxAbs = a;

        // ZCR with hysteresis
        const int cs = signWithHysteresis(normalizedBuffer[i], kZcrNoiseFloor);
        if (cs != 0) {
            if (prevSign != 0 && cs != prevSign) zeroCrossings++;
            prevSign = cs;
        }
    }

    const double n    = (double)kSamples;
    const double mean = sum / n;

    f.rms         = sqrt(sumSq / n);
    f.zcr         = (double)zeroCrossings / (double)(kSamples - 1);
    f.crestFactor = (f.rms > kEpsilon) ? (maxAbs / f.rms) : 0.0;
    f.rmsStab     = updateRmsStab(f.rms);

    // ── Kurtosis — requires second pass for accuracy ──────────────────────────
    double m2 = 0.0, m4 = 0.0;
    for (size_t i = 0; i < kSamples; i++) {
        const double d  = normalizedBuffer[i] - mean;
        const double d2 = d * d;
        m2 += d2;
        m4 += d2 * d2;
    }
    const double variance = m2 / n;
    if (variance > kEpsilon) {
        f.kurtosis = (m4 / n) / (variance * variance);
    }

    // ── Spectral features via Goertzel ────────────────────────────────────────
    computeSpectralMetrics(windowedBuffer, kSamples, kSampleRate,
                           &f.peakFreq, &f.spectralCentroid);

    f.dataValid = true;
    return f;
}