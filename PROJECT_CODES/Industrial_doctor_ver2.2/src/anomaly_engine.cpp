// ============================================================
//  INDUSTRIAL DOCTOR v2.1
//  anomaly_engine.cpp
// ============================================================
#include "anomaly_engine.h"
#include <math.h>
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
AnomalyEngine::AnomalyEngine(Profile defaultProfile)
    : _state(CAL_AMBIENT_SETTLING),
      _healthyProfile(defaultProfile),
      _settleIndex(0),
      _trendIndex(0),
      _trendWarningActive(false)
{
    strcpy(_predictionStr, "Pred: Calibrating...");
    memset(_trendBuffer, 0, sizeof(_trendBuffer));
}

// ─────────────────────────────────────────────────────────────────────────────
void AnomalyEngine::feedWindow(const float f[Profile::N_FEATURES]) {
    if (_state == CAL_AMBIENT_SETTLING || _state == CAL_HEALTHY_SETTLING) {
        // We are in a calibration phase. Copy the feature data into the
        // settle buffer.
        for (int i = 0; i < Profile::N_FEATURES; i++) {
            _settleBuffer[_settleIndex][i] = f[i];
        }
        _settleIndex++;

        // Check if the buffer is full.
        if (_settleIndex >= kSettleWindows) {
            // The buffer is full. Calculate the mean and standard deviation
            // for each feature.
            double sums[Profile::N_FEATURES] = {0};
            double sumSqs[Profile::N_FEATURES] = {0};

            for (int i = 0; i < kSettleWindows; i++) {
                for (int j = 0; j < Profile::N_FEATURES; j++) {
                    sums[j] += (double)_settleBuffer[i][j];
                    sumSqs[j] += (double)_settleBuffer[i][j] * (double)_settleBuffer[i][j];
                }
            }

            // Depending on which settling phase we were in, save the
            // calculated profile to the correct destination.
            if (_state == CAL_AMBIENT_SETTLING) {
                for (int i = 0; i < Profile::N_FEATURES; i++) {
                    double mean = sums[i] / kSettleWindows;
                    double variance = (sumSqs[i] / kSettleWindows) - (mean * mean);
                    if (variance < 0.0) variance = 0.0;
                    _ambientProfile.means[i] = (float)mean;
                    _ambientProfile.stds[i] = (float)sqrt(variance);
                }
                _state = CAL_AMBIENT_READY;
            } else {  // CAL_HEALTHY_SETTLING
                for (int i = 0; i < Profile::N_FEATURES; i++) {
                    double mean = sums[i] / kSettleWindows;
                    double variance = (sumSqs[i] / kSettleWindows) - (mean * mean);
                    if (variance < 0.0) variance = 0.0;
                    _healthyProfile.means[i] = (float)mean;
                    _healthyProfile.stds[i] = (float)sqrt(variance);
                }
                _state = CAL_HEALTHY_READY;
            }

            // Reset the buffer index for the next calibration phase.
            _settleIndex = 0;
        }
    }
}

ZScore AnomalyEngine::getAnomalyScore(const float f[Profile::N_FEATURES], const Profile& profile) {
    ZScore z;
    z.combined = 0;

    for (int i = 0; i < Profile::N_FEATURES; i++) {
        // Calculate the z-score for each feature.
        // A small epsilon is added to the standard deviation to avoid division by zero.
        z.features[i] =
            abs(f[i] - profile.means[i]) / (profile.stds[i] + 1e-8f);
        z.combined += z.features[i];
    }
    
    // Save to trend buffer for tracking
    for (int i = 0; i < Profile::N_FEATURES; i++) {
        _trendBuffer[_trendIndex][i] = z.features[i];
    }
    _trendIndex = (_trendIndex + 1) % kTrendWindow;
    
    // Using simple linear regression on Z-score drift:
    bool structuralTrend = false;
    for (int i = 0; i < Profile::N_FEATURES; i++) {
        float sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0, sumY2 = 0;
        int n = kTrendWindow;
        for (int j = 0; j < n; j++) {
            float x = (float)j;
            float y = _trendBuffer[(j + _trendIndex) % kTrendWindow][i];
            sumX += x;
            sumY += y;
            sumXY += x * y;
            sumX2 += x * x;
            sumY2 += y * y;
        }
        float denom = (n * sumX2 - sumX * sumX) * (n * sumY2 - sumY * sumY);
        float r = 0.0f;
        if (denom > 0.0f) {
            r = (n * sumXY - sumX * sumY) / sqrtf(denom);
        }
        float slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
        if (r > 0.8f && slope > 0.02f) { // Significant slope and strong correlation
            structuralTrend = true;
            break;
        }
    }
    
    z.structuralTrendWarning = structuralTrend;

    return z;
}

void AnomalyEngine::updateBaseline(const float f[Profile::N_FEATURES],
                                   float combinedScore) {
    // Rolling baseline using Exponential Moving Average (EMA)
    float alpha = 0.05 * (1.0 - tanh(combinedScore));

    for (int i = 0; i < Profile::N_FEATURES; i++) {
        _healthyProfile.means[i] =
            (1 - alpha) * _healthyProfile.means[i] + alpha * f[i];
    }
}

CalibrationState AnomalyEngine::getState() const {
    return _state;
}

void AnomalyEngine::advanceState() {
    if (_state == CAL_AMBIENT_READY) {
        _state = CAL_HEALTHY_SETTLING;
    }
}

void AnomalyEngine::startAmbientCalibration() {
    _state = CAL_AMBIENT_SETTLING;
    _settleIndex = 0; // Reset buffer
}

void AnomalyEngine::startHealthyCalibration() {
    _state = CAL_HEALTHY_SETTLING;
    _settleIndex = 0; // Reset buffer
}

Profile AnomalyEngine::getHealthyProfile() const {
    return _healthyProfile;
}

Profile AnomalyEngine::getAmbientProfile() const {
    return _ambientProfile;
}

float AnomalyEngine::calibProgress() const {
    if (_state == CAL_AMBIENT_SETTLING || _state == CAL_HEALTHY_SETTLING) {
        return (float)_settleIndex / (float)kSettleWindows;
    }
    if (_state == CAL_AMBIENT_READY || _state == CAL_HEALTHY_READY) {
        return 1.0f;  // Phase complete
    }
    return 0.0f;
}