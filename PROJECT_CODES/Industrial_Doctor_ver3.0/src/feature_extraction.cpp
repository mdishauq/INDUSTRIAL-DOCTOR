// =============================================================================
//  INDUSTRIAL DOCTOR v3.0  —  DYNAMIC FEATURE EXTRACTION
// =============================================================================

#include "feature_extraction.h"
#include <math.h>
#include <string.h>

FeatureExtractor::FeatureExtractor()
    : _initialized(false), _rmsHistIdx(0), _rmsHistCount(0), _lastFilteredEnvelope(0.0f) {
    memset(_rmsHistory, 0, sizeof(_rmsHistory));
}

FeatureExtractor::~FeatureExtractor() {
    if (_initialized) {
        i2s_driver_uninstall(kI2SPort);
    }
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
        .use_apll             = false,
        .tx_desc_auto_clear   = false,
        .fixed_mclk           = 0
    };

    const i2s_pin_config_t pinCfg = {
        .bck_io_num   = kI2SSck,
        .ws_io_num    = kI2SWs,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = kI2SSd
    };

    i2s_driver_install(kI2SPort, &i2sCfg, 0, NULL);
    i2s_set_pin(kI2SPort, &pinCfg);
}

bool FeatureExtractor::begin() {
    setupI2S();
    _initialized = true;
    return true;
}

void FeatureExtractor::applyHannWindow() {
    for (size_t i = 0; i < kSamples; i++) {
        float multiplier = 0.5f * (1.0f - cosf((2.0f * M_PI * i) / (kSamples - 1)));
        _windowedBuffer[i] = _normalizedBuffer[i] * multiplier;
    }
}

int FeatureExtractor::signWithHysteresis(float sample, float threshold) {
    if (sample > threshold) return 1;
    if (sample < -threshold) return -1;
    return 0;
}

float FeatureExtractor::updateRmsStab(float currentRms) {
    _rmsHistory[_rmsHistIdx] = currentRms;
    _rmsHistIdx = (_rmsHistIdx + 1) % kRmsStabWindow;
    if (_rmsHistCount < kRmsStabWindow) _rmsHistCount++;

    float sum = 0.0f;
    for (int i = 0; i < _rmsHistCount; i++) sum += _rmsHistory[i];
    float mean = sum / _rmsHistCount;

    float sumSq = 0.0f;
    for (int i = 0; i < _rmsHistCount; i++) {
        float diff = _rmsHistory[i] - mean;
        sumSq += diff * diff;
    }
    return sqrtf(sumSq / _rmsHistCount);
}

void FeatureExtractor::computeSpectralMetrics(float* peakFreq, float* spectralCentroid) {
    float maxMagnitude = -1.0f;
    float targetPeak   = 0.0f;
    
    float centroidNumerator   = 0.0f;
    float centroidDenominator = 0.0f;

    // Scan frequencies linearly via highly optimized single-frequency Goertzel coefficients
    for (float f = kMinFrequency; f <= kMaxFrequency; f += kFrequencyStep) {
        float omega = (2.0f * M_PI * f) / kSampleRate;
        float coeff = 2.0f * cosf(omega);

        float q1 = 0.0f;
        float q2 = 0.0f;

        for (size_t i = 0; i < kSamples; i++) {
            float q0 = _windowedBuffer[i] + coeff * q1 - q2;
            q2 = q1;
            q1 = q0;
        }

        float magnitude = sqrtf(q1 * q1 + q2 * q2 - coeff * q1 * q2);

        // Track absolute dominant peak
        if (magnitude > maxMagnitude) {
            maxMagnitude = magnitude;
            targetPeak = f;
        }

        // Accrue items for spectral center of mass calculation
        centroidNumerator   += f * magnitude;
        centroidDenominator += magnitude;
    }

    *peakFreq         = targetPeak;
    *spectralCentroid = (centroidDenominator > kEpsilon) ? (centroidNumerator / centroidDenominator) : 0.0f;
}

MotorFeatures FeatureExtractor::getNextFeatures() {
    MotorFeatures f;
    f.dataValid = false;

    if (!_initialized) return f;

    size_t bytesRead = 0;
    // Read clean raw blocks out of I2S DMA Ring Buffers
    esp_err_t err = i2s_read(kI2SPort, &_rawBuffer, sizeof(_rawBuffer), &bytesRead, portMAX_DELAY);

    if (err != ESP_OK || bytesRead != sizeof(_rawBuffer)) {
        return f; // Fail early if DMA stream is interrupted
    }

    // ── Phase 1: Time-Domain Initial Pass ──
    float sum            = 0.0f;
    float sumSq          = 0.0f;
    float maxAbs         = 0.0f;
    int   zeroCrossings  = 0;
    int   prevSign       = 0;
    float envelopeSumSq  = 0.0f;

    // Normalize 32-bit I2S data down to workable single precision float
    for (size_t i = 0; i < kSamples; i++) {
        // Shift right down to base level amplitude ranges
        _normalizedBuffer[i] = (float)(_rawBuffer[i] >> 14);

        float val = _normalizedBuffer[i];
        sum   += val;
        sumSq += val * val;

        float absVal = fabsf(val);
        if (absVal > maxAbs) maxAbs = absVal;

        // Zero-Crossing calculation with active hardware hysteresis noise guardrails
        int cs = signWithHysteresis(val, kZcrNoiseFloor);
        if (cs != 0) {
            if (prevSign != 0 && cs != prevSign) {
                zeroCrossings++;
            }
            prevSign = cs;
        }

        // High-Frequency Structural Demodulated Envelope Analysis Filter
        // Strips down mechanical rumble to expose high-frequency rolling friction patterns
        float highPassFiltered = (i == 0) ? 0.0f : (_normalizedBuffer[i] - _normalizedBuffer[i - 1]);
        float rectified        = fabsf(highPassFiltered);
        
        // Single-pole low-pass smoother over rectified high frequency stream
        _lastFilteredEnvelope = 0.95f * _lastFilteredEnvelope + 0.05f * rectified;
        envelopeSumSq += _lastFilteredEnvelope * _lastFilteredEnvelope;
    }

    float n = (float)kSamples;
    float mean = sum / n;

    // Calculate core time-domain descriptors
    f.rms         = sqrtf(sumSq / n);
    f.zcr         = (float)zeroCrossings / (n - 1.0f);
    f.crestFactor = (f.rms > kEpsilon) ? (maxAbs / f.rms) : 0.0f;
    f.rmsStab     = updateRmsStab(f.rms);
    f.envelopeRms = sqrtf(envelopeSumSq / n);

    // ── Phase 2: Higher Order Moments (Kurtosis) ──
    float m2 = 0.0f;
    float m4 = 0.0f;
    for (size_t i = 0; i < kSamples; i++) {
        float d  = _normalizedBuffer[i] - mean;
        float d2 = d * d;
        m2 += d2;
        m4 += d2 * d2;
    }
    float variance = m2 / n;
    f.kurtosis = (variance > kEpsilon) ? ((m4 / n) / (variance * variance)) : 0.0f;

    // ── Phase 3: Frequency-Domain Metrics via Windowed Goertzel Scans ──
    applyHannWindow();
    computeSpectralMetrics(&f.peakFreq, &f.spectralCentroid);

    f.dataValid = true;
    return f;
}