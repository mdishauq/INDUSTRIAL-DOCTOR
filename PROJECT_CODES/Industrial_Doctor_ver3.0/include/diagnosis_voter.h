#pragma once
// ============================================================================
//  INDUSTRIAL DOCTOR v2.1 — CALIBRATION-FREE PHYSICS ARCHITECTURE
//  diagnosis_voter.h
//
//  Responsibility:
//    Evaluates raw waveform shape proportions directly to determine health state.
//    Operates strictly on dimensionless physical limits. Zero baselines required.
// ============================================================================
#include <Arduino.h>
#include "feature_extraction.h"

// ── Primary output states matching display design ───────────────────────────
enum MotorStatus { 
    STATUS_AMBIENT, 
    STATUS_HEALTHY, 
    STATUS_FAULTY, 
    STATUS_UNKNOWN 
};

// ── Physics-derived Fault Classifications ────────────────────────────────────
enum FaultType {
    FAULT_NONE,
    FAULT_UNBALANCED,   // Low-frequency kinetic energy modulation (wobble)
    FAULT_FRICTION,     // High-frequency dragging / continuous sliding wear
    FAULT_BEARING       // High-transient cyclic impact spikes (chipped/cracked)
};

// ── Complete Diagnostic Output ───────────────────────────────────────────────
struct DiagnosisResult {
    MotorStatus status;
    FaultType   faultType;
    float       confidence;     // Geometric structural certainty (0.0 to 1.0)
    char        label[32];      // Subclass string for OLED rendering
};

class DiagnosisVoter {
public:
    DiagnosisVoter();

    // The core decision engine accepting raw extracted feature indices directly
    // Mapping matches the layout in MotorFeatures::toArray()
    DiagnosisResult vote(const float f[8]);

    // Hybrid ensemble vote using physics rules + statistical anomaly score
    // anomalyScore is expected to be normalized to 0.0f - 1.0f
    DiagnosisResult voteEnsemble(const float f[8], float anomalyScore);

    // Call this after collecting a baseline to lock in your room's signature
    void setAmbientThreshold(float dynamicMax);

private:
    // Feature array semantic index map matching feature_extraction
    enum FeatureIdx {
        IDX_RMS = 0,
        IDX_RMS_STAB = 1,
        IDX_PEAK_FREQ = 2,
        IDX_KURTOSIS = 3,
        IDX_CREST_FACTOR = 4,
        IDX_SPECTRAL_CENTROID = 5,
        IDX_ZCR = 6,
        IDX_ENVELOPE_RMS = 7
    };

    // Dimensionless Geometric Physics Rules
    float kAmbientRmsMax;                                 // Below this, it's environment floor noise
    static constexpr float kHealthyCrestMax     = 4.10f;   // Clean sinusoids/rotations stay under 4.1
    static constexpr float kHealthyKurtosisMax  = 3.40f;   // Normal distribution bell-curve peak limit
    static constexpr float kHealthyZcrMax       = 0.30f;   // Healthy wave crosses zero less frequently
    static constexpr float kHealthyCentroidMax  = 1200.0f; // Healthy energy stays in lower bands
    static constexpr float kHealthyPeakStdMax   = 35.0f;   // Peak frequency stability across windows
    static constexpr float kUnbalanceStabRatio  = 0.120f;  // RMS instability / Total RMS fluctuation floor
    static constexpr float kFrictionCentroidMin = 1400.0f; // Continuous scratching shifts energy up in Hz
    static constexpr float kFrictionZcrMin      = 0.38f;   // White chaos component ratio increase

    static constexpr int kPeakHistorySize = 8;
    float _peakHistory[kPeakHistorySize];
    int _peakHistIdx;
    int _peakHistCount;

    void updatePeakHistory(float peakFreq);
    float getPeakStdDev() const;
};