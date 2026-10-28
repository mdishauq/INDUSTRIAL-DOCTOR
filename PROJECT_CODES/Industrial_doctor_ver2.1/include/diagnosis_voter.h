#pragma once
// ============================================================
//  INDUSTRIAL DOCTOR v2.0
//  diagnosis_voter.h
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

// ── Secondary Global Database Output ──────────────────────────────────────────
struct DatabaseComparison {
    char   matchName[24];
    double matchZScore;
};

// Struct for storing CSV training baseline
struct GlobalProfile {
    char name[24];
    Profile p;
};

// ─────────────────────────────────────────────────────────────────────────────
class DiagnosisVoter {
public:
    DiagnosisVoter();

    DiagnosisResult vote(const ZScore&      z,
                         const float          f[Profile::N_FEATURES],
                         AnomalyEngine&       engine);

    // Compares current frame against the global DB trained from CSVs
    DatabaseComparison compareWithGlobalDatabase(const float f[Profile::N_FEATURES]) const;

private:
    // ── Vetoes (run before voters, return AMBIENT immediately if true) ─────────
    bool ambientVeto(const float f[Profile::N_FEATURES], const AnomalyEngine& engine) const;
    bool crowdVeto  (const float f[Profile::N_FEATURES]) const;
    bool transientVeto(const float f[Profile::N_FEATURES]) const;

    // ── Fault voters (return true if that fault pattern is detected) ───────────
    bool frictionVoter  (const ZScore& z) const;
    bool unbalancedVoter(const ZScore& z) const;
    bool criticalVoter  (const ZScore& z) const;

    // ── Confidence scalers ─────────────────────────────────────────────────────
    double frictionConfidence  (const ZScore& z) const;
    double unbalancedConfidence(const ZScore& z) const;
    double healthyConfidence   (const ZScore& z) const;

    // ── Confirmed thresholds (100% data-driven from CSV analysis) ─────────────
    static constexpr double kSilentZcr     =   0.010;  
    static constexpr double kSpeechZcr     =   0.080;  
    static constexpr double kSpeechRms     = 150.0;
    static constexpr double kCrowdCentroid =  900.0;
    static constexpr double kCrowdRmsStab  =   50.0;
    static constexpr double kTransientCrest   =   4.8;
    static constexpr double kTransientRmsStab =  200.0;
    static constexpr double kFrictionZcrZ  =   3.5;
    static constexpr double kUnbalZcrZ     =  -2.5;
    static constexpr double kUnbalCentZ    =  -2.5;
    static constexpr double kCriticalStabZ =   4.0;

    // Array holding static profiles from CSVs
    static const GlobalProfile globalDB[3]; 
};