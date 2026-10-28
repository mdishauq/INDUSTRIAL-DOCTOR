// ============================================================
//  INDUSTRIAL DOCTOR v2.0
//  feature_extraction.cpp
// ============================================================
#include "feature_extraction.h"
#include <math.h>
#include <string.h>

namespace {
    static int32_t rawBuffer[1024];
    static float   normalizedBuffer[1024];
    static float   windowedBuffer[1024];
}

FeatureExtractor::FeatureExtractor()
    : _initialized(false), _rmsHistIdx(0), _rmsHistCount(0)
{
    memset(_rmsHistory, 0, sizeof(_rmsHistory));
}

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

bool FeatureExtractor::begin() {
    setupI2S();
    _initialized = true;
    return true;
}

int FeatureExtractor::signWithHysteresis(float sample, float threshold) {
    if (sample >  threshold) return  1;
    if (sample < -threshold) return -1;
    return 0;
}

float FeatureExtractor::updateRmsStab(float newRms) {
    _rmsHistory[_rmsHistIdx] = newRms;
    _rmsHistIdx = (_rmsHistIdx + 1) % kRmsStabWindow;
    if (_rmsHistCount < kRmsStabWindow) _rmsHistCount++;

    double sum = 0.0, sumSq = 0.0;
    for (int i = 0; i < _rmsHistCount; i++) {
        sum   += (double)_rmsHistory[i];
        sumSq += (double)_rmsHistory[i] * (double)_rmsHistory[i];
    }
    double mean = sum / (double)_rmsHistCount;
    double var  = sumSq / (double)_rmsHistCount - mean * mean;
    return (float)((var > 0.0) ? sqrt(var) : 0.0);
}

void FeatureExtractor::computeSpectralMetrics(const float* samples, size_t count,
                                              float sampleRate,
                                              float* peakFreq,
                                              float* spectralCentroid) 
{
    float maxMag = -1.0f;
    float targetPeak = 0.0f;
    float numCentroid = 0.0f;
    float denCentroid = 0.0f;

    // --- FIX START: Logarithmic Frequency Stepping ---
    float f = kMinFrequency;     // Start at 20.0f
    float logMultiplier = 1.15f;  // Geometric scaling multiplier

    while (f <= kMaxFrequency) {
        // Standard Goertzel Algorithm Coefficients
        float omega = 2.0f * M_PI * f / sampleRate;
        float coeff = 2.0f * cosf(omega);

        float q0 = 0.0f;
        float q1 = 0.0f;
        float q2 = 0.0f;

        // Vectorized math processing loop
        for (size_t i = 0; i < count; i++) {
            q0 = samples[i] + coeff * q1 - q2;
            q2 = q1;
            q1 = q0;
        }

        float magnitude = sqrtf(q1 * q1 + q2 * q2 - coeff * q1 * q2);

        // Tracking the absolute dominant peak
        if (magnitude > maxMag) {
            maxMag = magnitude;
            targetPeak = f;
        }

        // Mass calculation for centroid tracking
        numCentroid += f * magnitude;
        denCentroid += magnitude;

        // Advance geometrically rather than linearly
        if (f < 500.0f) {
            f += 25.0f; // Fine-grained tracking below 500Hz for motor rumbles
        } else {
            f *= logMultiplier; // Wide steps at high frequencies to eliminate CPU starvation
        }
    }
    // --- FIX END ---

    *peakFreq = targetPeak;
    *spectralCentroid = (denCentroid > kEpsilon) ? (numCentroid / denCentroid) : 0.0f;
}

MotorFeatures FeatureExtractor::getNextFeatures() {
    MotorFeatures f = {0, 0, 0, 0, 0, 0, 0, 0, false};
    if (!_initialized) return f;

    size_t bytesRead = 0;

    const esp_err_t err = i2s_read(kI2SPort, rawBuffer,
                                    sizeof(rawBuffer), &bytesRead,
                                    pdMS_TO_TICKS(100));

    if (err != ESP_OK) return f; 
    if (bytesRead != sizeof(rawBuffer)) return f; 

    double sum    = 0.0;
    double sumSq  = 0.0;
    float maxAbs = 0.0f;
    int   zeroCrossings = 0;
    int   prevSign      = 0;

    const float twoPi    = 2.0f * PI;
    const float winDenom = (float)(kSamples - 1);

    double envelopeSumSq = 0.0;
    float lastFiltered = 0.0f;

    for (size_t i = 0; i < kSamples; i++) {
        // --- BUG FIX: Shift by 8 to keep 24-bit precision ---
        normalizedBuffer[i] = (float)(rawBuffer[i] >> 8);

        sum   += (double)normalizedBuffer[i];
        sumSq += (double)normalizedBuffer[i] * (double)normalizedBuffer[i];

        const float w = 0.54f - 0.46f * cosf(twoPi * (float)i / winDenom);
        windowedBuffer[i] = normalizedBuffer[i] * w;

        const float a = fabsf(normalizedBuffer[i]);
        if (a > maxAbs) maxAbs = a;

        const int cs = signWithHysteresis(normalizedBuffer[i], kZcrNoiseFloor);
        if (cs != 0) {
            if (prevSign != 0 && cs != prevSign) zeroCrossings++;
            prevSign = cs;
        }

        // High-Pass Filter + Rectification + Low-Pass Smooth for Envelope Analysis
        float filtered = 0.85f * (lastFiltered + normalizedBuffer[i] - (i > 0 ? normalizedBuffer[i-1] : 0.0f));
        lastFiltered = filtered;
        float rectified = fabsf(filtered);
        envelopeSumSq += (double)rectified * (double)rectified;
    }

    const double n    = (double)kSamples;
    const double mean = sum / n;

    f.rms         = (float)sqrt(sumSq / n);
    f.zcr         = (float)zeroCrossings / (float)(kSamples - 1);
    f.crestFactor = (f.rms > kEpsilon) ? (maxAbs / f.rms) : 0.0f;
    f.rmsStab     = updateRmsStab(f.rms);
    f.envelopeRms = (float)sqrt(envelopeSumSq / n);

    double m2 = 0.0, m4 = 0.0;
    for (size_t i = 0; i < kSamples; i++) {
        const double d  = (double)normalizedBuffer[i] - mean;
        const double d2 = d * d;
        m2 += d2;
        m4 += d2 * d2;
    }
    const double variance = m2 / n;
    if (variance > kEpsilon) {
        f.kurtosis = (float)((m4 / n) / (variance * variance));
    }

    computeSpectralMetrics(windowedBuffer, kSamples, kSampleRate,
                           &f.peakFreq, &f.spectralCentroid);

    f.dataValid = true;
    return f;
}

// Convert MotorFeatures struct to a float array for AnomalyEngine
void MotorFeatures::toArray(float* array) const {
    if (!dataValid) {
        for (int i = 0; i < 8; ++i) {
            array[i] = 0.0f;
        }
        return;
    }
    array[0] = rms;
    array[1] = rmsStab;
    array[2] = peakFreq;
    array[3] = kurtosis;
    array[4] = crestFactor;
    array[5] = spectralCentroid;
    array[6] = zcr;
    array[7] = envelopeRms;
}