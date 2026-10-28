#include "feature_data.h"
#include <driver/i2s.h>
#include <math.h>

namespace {
constexpr i2s_port_t kI2SPort = I2S_NUM_0;
constexpr int kI2SWs = 13;
constexpr int kI2SSd = 11;
constexpr int kI2SSck = 12;
constexpr size_t kSamples = 1024;
constexpr double kSampleRate = 16000.0;
constexpr double kMinFrequency = 20.0;
constexpr double kMaxFrequency = 2000.0;
constexpr double kFrequencyStep = 10.0;
constexpr double kZcrNoiseFloor = 80.0;
constexpr double kEpsilon = 1e-12;

static int32_t rawBuffer[kSamples];
static double normalizedBuffer[kSamples];
static double windowedBuffer[kSamples];
}

FeatureExtractor::FeatureExtractor() : _initialized(false), _historyIndex(0), _bufferFull(false) {
    for(int i=0; i<HISTORY_SIZE; i++) {
        _rmsHistory[i] = 0.0;
        _kurtosisHistory[i] = 0.0;
    }
}

void FeatureExtractor::setupI2S() {
    const i2s_config_t i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = (uint32_t)kSampleRate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = kSamples,
        .use_apll = false};

    const i2s_pin_config_t pinConfig = {
        .bck_io_num = kI2SSck,
        .ws_io_num = kI2SWs,
        .data_out_num = -1,
        .data_in_num = kI2SSd};

    i2s_driver_install(kI2SPort, &i2sConfig, 0, nullptr);
    i2s_set_pin(kI2SPort, &pinConfig);
}

bool FeatureExtractor::begin() {
    setupI2S();
    _initialized = true;
    return true;
}

int FeatureExtractor::signWithHysteresis(double sample, double threshold) {
    if (sample > threshold) return 1;
    if (sample < -threshold) return -1;
    return 0;
}

void FeatureExtractor::computeSpectralMetrics(const double *samples, size_t count, double sampleRate,
                                              double *peakFreq, double *spectralCentroid) {
    double bestFrequency = 0.0, bestPower = 0.0, weightedPowerSum = 0.0, powerSum = 0.0;

    for (double frequency = kMinFrequency; frequency <= kMaxFrequency; frequency += kFrequencyStep) {
        const double omega = 2.0 * PI * frequency / sampleRate;
        const double coeff = 2.0 * cos(omega);
        double sPrev = 0.0, sPrev2 = 0.0;

        for (size_t i = 0; i < count; ++i) {
            const double s = samples[i] + coeff * sPrev - sPrev2;
            sPrev2 = sPrev;
            sPrev = s;
        }

        const double power = sPrev2 * sPrev2 + sPrev * sPrev - coeff * sPrev * sPrev2;
        if (power > kEpsilon) {
            weightedPowerSum += frequency * power;
            powerSum += power;
        }
        if (power > bestPower) {
            bestPower = power;
            bestFrequency = frequency;
        }
    }
    *peakFreq = bestFrequency;
    *spectralCentroid = (powerSum > kEpsilon) ? (weightedPowerSum / powerSum) : 0.0;
}

MotorFeatures FeatureExtractor::getNextFeatures() {
    MotorFeatures features = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, false};
    if (!_initialized) return features;

    size_t bytesRead = 0;
    i2s_read(kI2SPort, rawBuffer, sizeof(rawBuffer), &bytesRead, portMAX_DELAY);
    
    double sum = 0.0, sumSquares = 0.0, maxAbs = 0.0;
    int zeroCrossings = 0, previousSign = 0;

    for (size_t i = 0; i < kSamples; ++i) {
        normalizedBuffer[i] = static_cast<double>(rawBuffer[i] >> 16);
        sum += normalizedBuffer[i];
        const double window = 0.54 - 0.46 * cos(2.0 * PI * i / (kSamples - 1));
        windowedBuffer[i] = normalizedBuffer[i] * window;
        if (fabs(normalizedBuffer[i]) > maxAbs) maxAbs = fabs(normalizedBuffer[i]);
        
        int currentSign = signWithHysteresis(normalizedBuffer[i], kZcrNoiseFloor);
        if (currentSign != 0) {
            if (previousSign != 0 && currentSign != previousSign) zeroCrossings++;
            previousSign = currentSign;
        }
        sumSquares += normalizedBuffer[i] * normalizedBuffer[i];
    }

    features.rms = sqrt(sumSquares / kSamples);
    features.zcr = static_cast<double>(zeroCrossings) / (kSamples - 1);
    features.crestFactor = (features.rms > kEpsilon) ? (maxAbs / features.rms) : 0.0;

    double m2 = 0.0, m4 = 0.0, mean = sum / kSamples;
    for (size_t i = 0; i < kSamples; ++i) {
        double d = normalizedBuffer[i] - mean;
        double d2 = d * d;
        m2 += d2; m4 += d2 * d2;
    }

    double var = m2 / kSamples;
    features.kurtosis = (var > kEpsilon) ? ((m4 / kSamples) / (var * var)) : 0.0;
    computeSpectralMetrics(windowedBuffer, kSamples, kSampleRate, &features.peakFreq, &features.spectralCentroid);

    // Stability calculations
    _rmsHistory[_historyIndex] = features.rms;
    _kurtosisHistory[_historyIndex] = features.kurtosis;
    _historyIndex = (_historyIndex + 1) % HISTORY_SIZE;
    if (_historyIndex == 0) _bufferFull = true;

    if (_bufferFull) {
        double rMean = 0, kMean = 0, rVar = 0, kVar = 0;
        for(int i=0; i<HISTORY_SIZE; i++) { rMean += _rmsHistory[i]; kMean += _kurtosisHistory[i]; }
        rMean /= HISTORY_SIZE; kMean /= HISTORY_SIZE;
        for(int i=0; i<HISTORY_SIZE; i++) {
            rVar += pow(_rmsHistory[i] - rMean, 2);
            kVar += pow(_kurtosisHistory[i] - kMean, 2);
        }
        features.rmsStability = sqrt(rVar / HISTORY_SIZE);
        features.kurtosisStability = sqrt(kVar / HISTORY_SIZE);
    }

    features.dataValid = true;
    return features;
}