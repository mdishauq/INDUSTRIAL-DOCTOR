// ============================================================
//  INDUSTRIAL DOCTOR v2.0
//  diagnosis_voter.cpp
//
//  Council of Voters — the final decision layer.
//
//  Order of operations every window:
//    1. ambientVeto  → motor off / dead silent / normal noise
//    2. crowdVeto    → loud environmental noise
//    3. frictionVoter, unbalancedVoter, criticalVoter
//    4. Decision logic
//    5. If HEALTHY → trigger engine.updateBaseline()
// ============================================================
#include "diagnosis_voter.h"
#include <math.h>
#include <string.h>

DiagnosisVoter::DiagnosisVoter() {}

// ─────────────────────────────────────────────────────────────────────────────
//  VETOES — run first, return true to classify AMBIENT immediately
// ─────────────────────────────────────────────────────────────────────────────

// ambientVeto covers three sub-cases:
//   (a) Motor is off — RMS below power-on gate
//   (b) Dead silence — ZCR = 0 (dead_silent ZCR = 0.000, Z = -7.59σ)
//   (c) Speech/normal noise — low ZCR + low RMS (normal_silent ZCR avg 0.052)
bool DiagnosisVoter::ambientVeto(const MotorFeatures& f) const {
    if (f.rms < kGateRms)                             return true;
    if (f.zcr < kSilentZcr)                           return true;
    if (f.zcr < kSpeechZcr && f.rms < kSpeechRms)    return true;
    return false;
}

// crowdVeto — Centroid (crowd avg 1046, +5.42σ) AND RmsStab (crowd avg 64, +2.94σ)
// ZCR alone cannot catch crowd noise (crowd ZCR = -0.42σ from healthy — invisible).
// Both features must fire together to prevent false vetoes.
bool DiagnosisVoter::crowdVeto(const MotorFeatures& f) const {
    return (f.spectralCentroid > kCrowdCentroid && f.rmsStab > kCrowdRmsStab);
}

// ─────────────────────────────────────────────────────────────────────────────
//  VOTERS
// ─────────────────────────────────────────────────────────────────────────────

// frictionVoter — ZCR rises sharply when bearing surfaces scrape.
// From data: fault_friction ZCR = +4.05σ above healthy 8V baseline.
// Threshold at 3.5σ gives a clear gap below the fault mean (+4.05σ).
bool DiagnosisVoter::frictionVoter(const ZScoreFrame& z) const {
    return (z.zZcr > kFrictionZcrZ);
}

// unbalancedVoter — structural looseness causes the motor frequency to drop
// to ~290 Hz (from 593 Hz) and the sound to go "darker" (centroid drops).
// From data: ZCR = -3.22σ AND Centroid = -3.05σ — both must fire together.
// ZCR dropping alone can also mean motor-off; Centroid confirmation is mandatory.
bool DiagnosisVoter::unbalancedVoter(const ZScoreFrame& z) const {
    return (z.zZcr < kUnbalZcrZ && z.zCentroid < kUnbalCentZ);
}

// criticalVoter — RmsStab exploding (kCriticalStabZ = 4σ) catches catastrophic
// events that neither friction nor unbalanced patterns can explain.
// Example: motor physically detaching, cable snapping, debris impact.
bool DiagnosisVoter::criticalVoter(const ZScoreFrame& z) const {
    return (z.zRmsStab > kCriticalStabZ);
}

// ─────────────────────────────────────────────────────────────────────────────
//  CONFIDENCE SCALERS
// ─────────────────────────────────────────────────────────────────────────────

// Friction confidence: scales from 0.70 at threshold (3.5σ) → 0.95 at 6.0σ
// Real fault_friction ZCR = +4.05σ → baseline confidence ≈ 0.77
double DiagnosisVoter::frictionConfidence(const ZScoreFrame& z) const {
    const double excess = z.zZcr - kFrictionZcrZ;
    const double conf   = 0.70 + (excess / (6.0 - kFrictionZcrZ)) * 0.25;
    return constrain(conf, 0.70, 0.95);
}

// Unbalanced confidence: average excess of both ZCR and Centroid below threshold
// Real data: ZCR -3.22σ + Centroid -3.05σ → excess avg ≈ 0.64 → conf ≈ 0.75
double DiagnosisVoter::unbalancedConfidence(const ZScoreFrame& z) const {
    const double exZcr  = fabs(z.zZcr)      - fabs(kUnbalZcrZ);
    const double exCent = fabs(z.zCentroid)  - fabs(kUnbalCentZ);
    const double excess = (exZcr + exCent) * 0.5;
    const double conf   = 0.70 + (excess / 3.0) * 0.25;
    return constrain(conf, 0.70, 0.95);
}

// Healthy confidence: inversely proportional to combined anomaly score.
// combined = 0.0 → conf = 1.0 (perfectly normal),  combined = 2.0 → conf ~0.50
double DiagnosisVoter::healthyConfidence(const ZScoreFrame& z) const {
    const double conf = 1.0 - (z.combined / 4.0);
    return constrain(conf, 0.55, 0.99);
}

// ─────────────────────────────────────────────────────────────────────────────
//  VOTE — main decision function
// ─────────────────────────────────────────────────────────────────────────────
DiagnosisResult DiagnosisVoter::vote(const ZScoreFrame&   z,
                                      const MotorFeatures& f,
                                      VoltageMode          vmode,
                                      AnomalyEngine&       engine) {
    DiagnosisResult r;
    r.status      = UNKNOWN;
    r.faultType   = FAULT_NONE;
    r.confidence  = 0.0;
    r.anomalyScore = z.combined;
    strncpy(r.label, "Unknown", sizeof(r.label));

    if (!z.valid) return r;

    // ── VETO 1: Motor off / Dead silent / Normal noise ─────────────────────────
    if (ambientVeto(f)) {
        r.status = AMBIENT;

        if (f.rms < kGateRms) {
            strncpy(r.label, "Off / Idle", sizeof(r.label));
            r.confidence = 0.97;
        } else if (f.zcr < kSilentZcr) {
            strncpy(r.label, "Dead Silent", sizeof(r.label));
            r.confidence = 0.95;
        } else {
            strncpy(r.label, "Normal Noise", sizeof(r.label));
            r.confidence = 0.80;
        }
        return r;
    }

    // ── VETO 2: Crowd / environmental noise ────────────────────────────────────
    if (crowdVeto(f)) {
        r.status     = AMBIENT;
        r.confidence = 0.87;
        strncpy(r.label, "Crowd / Env Noise", sizeof(r.label));
        return r;
    }

    // ── FAULT VOTERS ───────────────────────────────────────────────────────────
    const bool isFriction   = frictionVoter(z);
    const bool isUnbalanced = unbalancedVoter(z);
    const bool isCritical   = criticalVoter(z);

    if (isFriction && isUnbalanced) {
        // Contradictory votes — both rising AND falling ZCR simultaneously.
        // This means the signal is chaotic in a way no single fault explains.
        r.status     = FAULTY;
        r.faultType  = FAULT_CRITICAL;
        r.confidence = 0.82;
        strncpy(r.label, "Critical Fault", sizeof(r.label));
    }
    else if (isCritical) {
        r.status     = FAULTY;
        r.faultType  = FAULT_CRITICAL;
        r.confidence = (double)constrain(
                         0.80 + (z.zRmsStab - kCriticalStabZ) * 0.03,
                         0.80, 0.97);
        strncpy(r.label, "Critical Fault", sizeof(r.label));
    }
    else if (isFriction) {
        r.status     = FAULTY;
        r.faultType  = FAULT_FRICTION;
        r.confidence = frictionConfidence(z);
        strncpy(r.label, "Friction / Wear", sizeof(r.label));
    }
    else if (isUnbalanced) {
        r.status     = FAULTY;
        r.faultType  = FAULT_UNBALANCED;
        r.confidence = unbalancedConfidence(z);
        strncpy(r.label, "Unbalanced", sizeof(r.label));
    }
    else {
        // ── HEALTHY ────────────────────────────────────────────────────────────
        r.status     = HEALTHY;
        r.faultType  = FAULT_NONE;
        r.confidence = healthyConfidence(z);

        if (vmode == MODE_6V) {
            strncpy(r.label, "6V Mode", sizeof(r.label));
        } else {
            strncpy(r.label, "8V Mode", sizeof(r.label));
        }

        // HEALTHY is the only verdict that updates the rolling baseline.
        // updateBaseline() internally enforces the guardrail (|score| > 1.8).
        engine.updateBaseline(f, z.combined);
    }

    return r;
}