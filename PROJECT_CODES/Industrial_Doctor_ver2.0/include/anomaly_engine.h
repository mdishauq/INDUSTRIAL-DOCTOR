#pragma once
// ============================================================
//  INDUSTRIAL DOCTOR v2.0
//  anomaly_engine.h
//
//  Responsibility:
//    - Auto-detect voltage mode (6V / 8V) from live audio
//    - Self-calibrate μ and σ from boot audio (first 170 windows)
//    - Score every live window as a ZScoreFrame
//    - Maintain a rolling EMA baseline with a fault guardrail
//
//  This file does NOT make HEALTHY/FAULTY decisions.
//  It only produces numbers. Decisions belong to diagnosis_voter.
// ============================================================
#include <Arduino.h>
#include "feature_extraction.h"

// ── Voltage operating mode ─────────────────────────────────────────────────────
enum VoltageMode { MODE_UNKNOWN, MODE_6V, MODE_8V };

// ── Internal calibration state machine ────────────────────────────────────────
enum CalibrationState {
    CAL_WARMUP,      // Windows 0–19:   Discard + detect voltage mode
    CAL_COLLECTING,  // Windows 20–169: Accumulate features for μ/σ
    CAL_SETTLING,    // Windows 170+:   EMA begins, settling display shown
    CAL_READY        // Normal operation (after kSettleWindows healthy frames)
};

// ── Per-feature statistical model ─────────────────────────────────────────────
struct FeatureStat {
    double mean;
    double stddev;
};

// ── Full motor operating profile ──────────────────────────────────────────────
struct MotorProfile {
    FeatureStat rms;
    FeatureStat rmsStab;
    FeatureStat peakFreq;
    FeatureStat zcr;
    FeatureStat centroid;
    FeatureStat crest;
    FeatureStat kurt;
    double      ambientRmsThreshold;  // RMS below this → motor is off
};

// ── Output of the scoring step ────────────────────────────────────────────────
struct ZScoreFrame {
    double zRms;
    double zRmsStab;
    double zPeakHz;
    double zZcr;
    double zCentroid;
    double zCrest;
    double zKurt;
    double combined;   // weighted anomaly score (see weights in .cpp)
    bool   valid;
};

// ─────────────────────────────────────────────────────────────────────────────
class AnomalyEngine {
public:
    AnomalyEngine();

    // Call every loop iteration — drives the internal state machine
    void        feedWindow(const MotorFeatures& f);

    // Score a window against the current profile. Only valid when SETTLING or READY.
    ZScoreFrame scoreWindow(const MotorFeatures& f) const;

    // Update EMA baseline. Called by DiagnosisVoter after a HEALTHY verdict.
    void        updateBaseline(const MotorFeatures& f, double combinedScore);

    // ── State queries ──────────────────────────────────────────────────────────
    CalibrationState  state()              const { return _state; }
    VoltageMode       voltageMode()        const { return _voltageMode; }
    float             calibProgress()      const;   // 0.0 → 1.0
    bool              isReady()            const { return _state == CAL_READY; }
    bool              isSettling()         const { return _state == CAL_SETTLING; }
    int               settlingCount()      const { return _settlingWindowCount; }
    const MotorProfile& activeProfile()   const { return _activeProfile; }

private:
    // ── Hardcoded master-data profiles (real CSV values, used as fallback) ─────
    static MotorProfile defaultProfile8V();
    static MotorProfile defaultProfile6V();

    // ── Internal helpers ───────────────────────────────────────────────────────
    double zScore(double val, double mean, double stddev) const;
    void   detectVoltageMode();
    void   finalizeCalibration();

    // ── Timing constants ───────────────────────────────────────────────────────
    static constexpr int    kWarmupWindows  = 20;    // discard + voltage detect
    static constexpr int    kCalibWindows   = 150;   // windows to build μ/σ
    static constexpr int    kSettleWindows  = 50;    // EMA warm-up display period

    // ── Statistical constants ─────────────────────────────────────────────────
    // σ floor prevents divide-by-zero and keeps the model from being
    // hypersensitive to features with naturally tiny variance.
    static constexpr double kSigmaFloor    = 5.0;

    // EMA update rate — α=0.01 means ~100 healthy windows to shift μ by 63%
    static constexpr double kEmaAlpha      = 0.01;

    // Guardrail: if |combined score| exceeds this, the baseline will NOT update.
    // Prevents the system from "learning" a fault as the new normal.
    // Value 1.8 chosen because fault_friction scores ~2.1 and fault_unbalanced ~2.2
    static constexpr double kGuardrail     = 1.8;

    // Voltage mode detection threshold: midpoint of 8V centroid (739) and 6V (929)
    static constexpr double kVoltageCentroidMidpoint = 834.6;

    // ── State ─────────────────────────────────────────────────────────────────
    CalibrationState _state;
    VoltageMode      _voltageMode;
    MotorProfile     _activeProfile;

    int _windowCount;
    int _settlingWindowCount;

    // Warmup: collect centroid samples for voltage mode detection
    double _warmupCentroidSum;
    int    _warmupCentroidCount;

    // Calibration accumulators (sum and sum-of-squares per feature)
    double _sumRms,   _sumSqRms;
    double _sumStab,  _sumSqStab;
    double _sumPeak,  _sumSqPeak;
    double _sumZcr,   _sumSqZcr;
    double _sumCent,  _sumSqCent;
    double _sumCrest, _sumSqCrest;
    double _sumKurt,  _sumSqKurt;
    int    _calCount;
};