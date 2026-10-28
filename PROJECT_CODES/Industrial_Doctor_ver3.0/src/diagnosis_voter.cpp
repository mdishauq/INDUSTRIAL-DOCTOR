// ============================================================================
//  INDUSTRIAL DOCTOR v2.1 — CALIBRATION-FREE PHYSICS ARCHITECTURE
//  diagnosis_voter.cpp
// ============================================================================
#include "diagnosis_voter.h"
#include <math.h>
#include <string.h>

DiagnosisVoter::DiagnosisVoter() {
    kAmbientRmsMax = 45.0f; // Default baseline fallback
    memset(_peakHistory, 0, sizeof(_peakHistory));
    _peakHistIdx = 0;
    _peakHistCount = 0;
}

void DiagnosisVoter::updatePeakHistory(float peakFreq) {
    _peakHistory[_peakHistIdx] = peakFreq;
    _peakHistIdx = (_peakHistIdx + 1) % kPeakHistorySize;
    if (_peakHistCount < kPeakHistorySize) {
        _peakHistCount++;
    }
}

float DiagnosisVoter::getPeakStdDev() const {
    if (_peakHistCount == 0) return 0.0f;
    float sum = 0.0f;
    for (int i = 0; i < _peakHistCount; i++) {
        sum += _peakHistory[i];
    }
    float mean = sum / _peakHistCount;

    float sumSq = 0.0f;
    for (int i = 0; i < _peakHistCount; i++) {
        float diff = _peakHistory[i] - mean;
        sumSq += diff * diff;
    }
    return sqrtf(sumSq / _peakHistCount);
}

void DiagnosisVoter::setAmbientThreshold(float dynamicMax) {
    // Add a structural buffer to ensure ambient spikes don't trigger false faults
    kAmbientRmsMax = constrain(dynamicMax * 1.30f, 20.0f, 120.0f);
    Serial.printf("--> Dynamic Ambient RMS Guardrail locked at: %.2f\n", kAmbientRmsMax);
}

DiagnosisResult DiagnosisVoter::vote(const float f[8]) {
    return voteEnsemble(f, 0.0f);
}

DiagnosisResult DiagnosisVoter::voteEnsemble(const float f[8], float anomalyScore) {
    DiagnosisResult r;
    r.status = STATUS_UNKNOWN;
    r.faultType = FAULT_NONE;
    r.confidence = 0.0f;
    strcpy(r.label, "Analyzing...");

    // 1. Unpack features for cleaner readability
    const float rms        = f[IDX_RMS];
    const float rmsStab    = f[IDX_RMS_STAB];
    const float peakFreq   = f[IDX_PEAK_FREQ];
    const float kurtosis   = f[IDX_KURTOSIS];
    const float crest      = f[IDX_CREST_FACTOR];
    const float centroid   = f[IDX_SPECTRAL_CENTROID];
    const float zcr        = f[IDX_ZCR];

    // Protect against division by zero in unpowered states
    if (rms < 1e-4f) {
        r.status = STATUS_AMBIENT;
        r.faultType = FAULT_NONE;
        r.confidence = 1.0f;
        strcpy(r.label, "Silent / Off");
        return r;
    }

    // 2. Evaluate Base Operational States (Ambient Veto)
    if (rms <= kAmbientRmsMax) {
        r.status = STATUS_AMBIENT;
        r.faultType = FAULT_NONE;
        r.confidence = constrain(1.0f - (rms / kAmbientRmsMax), 0.0f, 1.0f);
        strcpy(r.label, "Ambient Noise");
        return r;
    }

    // Calculate normalized dimensionless proportions
    float relativeInstability = rmsStab / rms;
    updatePeakHistory(peakFreq);
    float peakStdDev = getPeakStdDev();

    // 3. Run Physics Isolation Matrix
    
    // Check A: Bearing Fault Pattern (Cyclic structural impact spikes)
    // Physical indicator: Sharp transient peaks expanding far from the signal body.
    if (crest > kHealthyCrestMax || kurtosis > kHealthyKurtosisMax) {
        r.status = STATUS_FAULTY;
        r.faultType = FAULT_BEARING;
        
        // Calculate confidence based on how aggressively the wave geometric spikes overshoot
        float crestExcess = (crest - kHealthyCrestMax) / kHealthyCrestMax;
        float kurtExcess  = (kurtosis - kHealthyKurtosisMax) / kHealthyKurtosisMax;
        r.confidence = constrain(0.5f + (max(crestExcess, kurtExcess) * 0.5f), 0.5f, 1.0f);
        
        strcpy(r.label, "Bearing Damage");
        return r;
    }

    // Check B: Friction / Structural Dragging (Continuous high-frequency wear)
    // Physical indicator: Spectral distribution weight center skews upward; chaos factor (ZCR) rises.
    if (centroid > kFrictionCentroidMin && zcr > kFrictionZcrMin) {
        r.status = STATUS_FAULTY;
        r.faultType = FAULT_FRICTION;
        
        float centroidScore = (centroid - kFrictionCentroidMin) / 1000.0f;
        r.confidence = constrain(0.6f + (centroidScore * 0.4f), 0.6f, 0.98f);
        
        strcpy(r.label, "Friction / Drag");
        return r;
    }

    // Check C: Unbalanced Shaft / Eccentric Wobble (Low-frequency energy surge)
    // Physical indicator: Continuous mechanical breathing cycle creating high relative envelope shifts.
    if (relativeInstability > kUnbalanceStabRatio) {
        r.status = STATUS_FAULTY;
        r.faultType = FAULT_UNBALANCED;
        
        float instabilityExcess = (relativeInstability - kUnbalanceStabRatio) / kUnbalanceStabRatio;
        r.confidence = constrain(0.5f + (instabilityExcess * 0.5f), 0.5f, 0.95f);
        
        strcpy(r.label, "Shaft Unbalance");
        return r;
    }

    // 4. Solution A: weighted physics-based healthy score
    float physicsVotes = 0.0f;
    float physicsTotal = 0.0f;

    if (rms > kAmbientRmsMax) physicsVotes += 2.0f;
    physicsTotal += 2.0f;

    if (crest <= kHealthyCrestMax) physicsVotes += 1.0f;
    physicsTotal += 1.0f;

    if (kurtosis <= kHealthyKurtosisMax) physicsVotes += 1.0f;
    physicsTotal += 1.0f;

    if (zcr <= kHealthyZcrMax) physicsVotes += 1.0f;
    physicsTotal += 1.0f;

    if (centroid <= kHealthyCentroidMax) physicsVotes += 1.0f;
    physicsTotal += 1.0f;

    if (peakStdDev <= kHealthyPeakStdMax) physicsVotes += 1.0f;
    physicsTotal += 1.0f;

    float physicsHealthyScore = (physicsTotal > 0.0f) ? (physicsVotes / physicsTotal) : 0.0f;
    float physicsAnomalyScore = 1.0f - physicsHealthyScore;

    // 5. Solution B: statistical anomaly score (bounded 0.0 to 1.0)
    float boundedAnomalyScore = constrain(anomalyScore, 0.0f, 1.0f);

    // 6. Meta-voter: combine Solution A + Solution B
    constexpr float kPhysicsWeight = 0.40f;
    constexpr float kStatsWeight   = 0.60f;
    float finalAnomalyScore = (physicsAnomalyScore * kPhysicsWeight) + (boundedAnomalyScore * kStatsWeight);

    if (finalAnomalyScore <= 0.35f) {
        r.status = STATUS_HEALTHY;
        r.faultType = FAULT_NONE;
        r.confidence = constrain(1.0f - finalAnomalyScore, 0.5f, 1.0f);
        strcpy(r.label, "Motor Healthy");
        return r;
    }

    if (finalAnomalyScore >= 0.60f) {
        r.status = STATUS_FAULTY;
        r.faultType = FAULT_NONE;
        r.confidence = constrain(finalAnomalyScore, 0.5f, 1.0f);
        strcpy(r.label, "System Anomaly");
        return r;
    }

    // 7. Not ambient, not hard-fault, but mixed evidence
    r.status = STATUS_UNKNOWN;
    r.faultType = FAULT_NONE;
    r.confidence = 0.5f;
    strcpy(r.label, "Uncertain");
    return r;
}