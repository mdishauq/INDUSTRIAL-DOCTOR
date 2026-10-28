// ============================================================
//  INDUSTRIAL DOCTOR v2.0
//  feature_extraction.cpp
// ============================================================
#include "feature_extraction.h"
#include <math.h>
#include <string.h>

namespace {
    static int32_t rawBuffer[1024];
    static double  normalizedBuffer[1024];
    static double  windowedBuffer[1024];
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

int FeatureExtractor::signWithHysteresis(double sample, double threshold) {
    if (sample >  threshold) return  1;
    if (sample < -threshold) return -1;
    return 0;
}

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

MotorFeatures FeatureExtractor::getNextFeatures() {
    MotorFeatures f = {0, 0, 0, 0, 0, 0, 0, false};
    if (!_initialized) return f;

    size_t bytesRead = 0;

    const esp_err_t err = i2s_read(kI2SPort, rawBuffer,
                                    sizeof(rawBuffer), &bytesRead,
                                    pdMS_TO_TICKS(100));

    if (err != ESP_OK) return f; 
    if (bytesRead != sizeof(rawBuffer)) return f; 

    double sum    = 0.0;
    double sumSq  = 0.0;
    double maxAbs = 0.0;
    int    zeroCrossings = 0;
    int    prevSign      = 0;

    const double twoPi    = 2.0 * PI;
    const double winDenom = (double)(kSamples - 1);

    for (size_t i = 0; i < kSamples; i++) {
        // --- BUG FIX: Shift by 8 to keep 24-bit precision ---
        normalizedBuffer[i] = (double)(rawBuffer[i] >> 8);

        sum   += normalizedBuffer[i];
        sumSq += normalizedBuffer[i] * normalizedBuffer[i];

        const double w = 0.54 - 0.46 * cos(twoPi * (double)i / winDenom);
        windowedBuffer[i] = normalizedBuffer[i] * w;

        const double a = fabs(normalizedBuffer[i]);
        if (a > maxAbs) maxAbs = a;

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

    computeSpectralMetrics(windowedBuffer, kSamples, kSampleRate,
                           &f.peakFreq, &f.spectralCentroid);

    f.dataValid = true;
    return f;
}

// Convert MotorFeatures struct to a float array for AnomalyEngine
void MotorFeatures::toArray(float* array) const {
    if (!dataValid) {
        for (int i = 0; i < 7; ++i) {
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
}