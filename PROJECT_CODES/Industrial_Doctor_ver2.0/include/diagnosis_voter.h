#pragma once
// ============================================================
//  INDUSTRIAL DOCTOR v2.0
//  diagnosis_voter.h
//
//  Responsibility: Take Z-scores from AnomalyEngine and return
//  a human-readable verdict with confidence.
//
//  Three primary output states:  AMBIENT / HEALTHY / FAULTY
//  Subclass labels:
//    AMBIENT  → "Dead Silent" / "Normal Noise" / "Crowd / Env Noise"
//    HEALTHY  → "8V Mode"    / "6V Mode"
//    FAULTY   → "Friction / Wear" / "Unbalanced" / "Critical Fault"
// ============================================================
#include <Arduino.h>
#include "feature_extraction.h"
#include "anomaly_engine.h"

// ── Primary output states (3 only, matching display design) ───────────────────
enum MotorStatus { AMBIENT, HEALTHY, FAULTY, UNKNOWN };

// ── Fault sub-classification ──────────────────────────────────────────────────
enum FaultType {
    FAULT_NONE,
    FAULT_FRICTION,
    FAULT_UNBALANCED,
    FAULT_CRITICAL
};

// ── Full diagnosis result ──────────────────────────────────────────────────────
struct DiagnosisResult {
    MotorStatus status;
    FaultType   faultType;
    double      confidence;     // 0.0 – 1.0
    double      anomalyScore;   // raw combined Z-score for serial logging
    char        label[24];      // subclass string for OLED display
};

// ─────────────────────────────────────────────────────────────────────────────
class DiagnosisVoter {
public:
    DiagnosisVoter();

    // Main entry point.  engine is non-const because HEALTHY verdict triggers
    // engine.updateBaseline() here — this is the only place it's called.
    DiagnosisResult vote(const ZScoreFrame&   z,
                         const MotorFeatures& f,
                         VoltageMode          vmode,
                         AnomalyEngine&       engine);

private:
    // ── Vetoes (run before voters, return AMBIENT immediately if true) ─────────
    bool ambientVeto(const MotorFeatures& f) const;
    bool crowdVeto  (const MotorFeatures& f) const;

    // ── Fault voters (return true if that fault pattern is detected) ───────────
    bool frictionVoter  (const ZScoreFrame& z) const;
    bool unbalancedVoter(const ZScoreFrame& z) const;
    bool criticalVoter  (const ZScoreFrame& z) const;

    // ── Confidence scalers ─────────────────────────────────────────────────────
    double frictionConfidence  (const ZScoreFrame& z) const;
    double unbalancedConfidence(const ZScoreFrame& z) const;
    double healthyConfidence   (const ZScoreFrame& z) const;

    // ── Confirmed thresholds (100% data-driven from CSV analysis) ─────────────

    // Ambient gates — from dead_silent.csv and normal_silent.csv
    static constexpr double kGateRms       =  70.0;   // RMS < 70  → motor off
    static constexpr double kSilentZcr     =   0.010;  // ZCR < 0.01 → dead silent
    static constexpr double kSpeechZcr     =   0.080;  // ZCR < 0.08 + low RMS → speech
    static constexpr double kSpeechRms     = 150.0;

    // Crowd veto — Centroid +5.42σ and RmsStab +2.94σ from crowd_noise.csv
    static constexpr double kCrowdCentroid =  900.0;
    static constexpr double kCrowdRmsStab  =   50.0;

    // Friction voter — ZCR = +4.05σ in fault_friction.csv; threshold at 3.5σ
    static constexpr double kFrictionZcrZ  =   3.5;

    // Unbalanced voter — ZCR = -3.22σ AND Centroid = -3.05σ in fault_unbalanced.csv
    static constexpr double kUnbalZcrZ     =  -2.5;
    static constexpr double kUnbalCentZ    =  -2.5;

    // Critical voter — RmsStab catastrophically high (4σ above healthy mean)
    static constexpr double kCriticalStabZ =   4.0;
};