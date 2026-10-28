#pragma once
// ============================================================================
//  INDUSTRIAL DOCTOR v3.0 — HYBRID ANOMALY ENGINE (SOLUTION B)
//  Captures a healthy baseline and scores deviations using bounded Z-scores.
// ============================================================================
#include <Arduino.h>

class AnomalyEngine {
public:
    static constexpr int kFeatureCount = 8;

    AnomalyEngine();

    void beginCalibration();
    void addSample(const float f[kFeatureCount]);
    bool finalizeCalibration();
    bool isCalibrated() const;

    float score(const float f[kFeatureCount]) const;
    int sampleCount() const;

private:
    float _mean[kFeatureCount];
    float _m2[kFeatureCount];
    float _std[kFeatureCount];
    int _count;
    bool _calibrated;

    static constexpr float kStdEpsilon = 1e-3f;
    static constexpr float kZClamp = 6.0f;

    float zToProbability(float z) const;
};
