// ============================================================================
//  INDUSTRIAL DOCTOR v3.0 — HYBRID ANOMALY ENGINE (SOLUTION B)
// ============================================================================
#include "anomaly_engine.h"
#include <math.h>
#include <string.h>

AnomalyEngine::AnomalyEngine() {
    beginCalibration();
}

void AnomalyEngine::beginCalibration() {
    memset(_mean, 0, sizeof(_mean));
    memset(_m2, 0, sizeof(_m2));
    memset(_std, 0, sizeof(_std));
    _count = 0;
    _calibrated = false;
}

void AnomalyEngine::addSample(const float f[kFeatureCount]) {
    _count++;
    for (int i = 0; i < kFeatureCount; i++) {
        float delta = f[i] - _mean[i];
        _mean[i] += delta / _count;
        float delta2 = f[i] - _mean[i];
        _m2[i] += delta * delta2;
    }
}

bool AnomalyEngine::finalizeCalibration() {
    if (_count < 2) {
        _calibrated = false;
        return false;
    }

    for (int i = 0; i < kFeatureCount; i++) {
        float variance = _m2[i] / (_count - 1);
        float std = sqrtf(variance);
        _std[i] = (std < kStdEpsilon) ? kStdEpsilon : std;
    }

    _calibrated = true;
    return true;
}

bool AnomalyEngine::isCalibrated() const {
    return _calibrated;
}

int AnomalyEngine::sampleCount() const {
    return _count;
}

float AnomalyEngine::zToProbability(float z) const {
    if (z <= 2.0f) return 0.0f;
    if (z >= 5.0f) return 1.0f;
    return (z - 2.0f) / 3.0f;
}

float AnomalyEngine::score(const float f[kFeatureCount]) const {
    if (!_calibrated) return 0.0f;

    float zSum = 0.0f;
    int used = 0;

    for (int i = 0; i < kFeatureCount; i++) {
        float z = fabsf(f[i] - _mean[i]) / _std[i];
        if (z > kZClamp) z = kZClamp;
        zSum += z;
        used++;
    }

    if (used == 0) return 0.0f;

    float avgZ = zSum / used;
    float prob = zToProbability(avgZ);
    return constrain(prob, 0.0f, 1.0f);
}
