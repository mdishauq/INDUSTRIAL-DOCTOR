// ============================================================
//  INDUSTRIAL DOCTOR v2.0
//  diagnosis_voter.cpp
// ============================================================
#include "diagnosis_voter.h"
#include <math.h>
#include <string.h>

// Global Database Profiles (derived from CSVs)
const GlobalProfile DiagnosisVoter::globalDB[3] = {
    {"DB: Friction",   {{150.0f, 10.0f, 500.0f, 2.5f, 3.5f, 800.0f, 0.40f, 25.0f}, {10.0f, 2.0f, 20.0f, 0.5f, 0.5f, 50.0f, 0.05f, 5.0f}}},
    {"DB: Unbalanced", {{160.0f, 20.0f, 290.0f, 2.8f, 4.0f, 400.0f, 0.10f, 10.0f}, {15.0f, 5.0f, 10.0f, 0.6f, 0.8f, 30.0f, 0.02f, 2.0f}}},
    {"DB: Healthy",    {{100.0f, 5.0f,  593.0f, 2.0f, 3.0f, 600.0f, 0.20f, 5.0f},  {5.0f,  1.0f, 15.0f, 0.2f, 0.3f, 40.0f, 0.03f, 1.0f}}}
};

DiagnosisVoter::DiagnosisVoter() {}

bool DiagnosisVoter::ambientVeto(const float f[Profile::N_FEATURES], const AnomalyEngine& engine) const {
    const Profile& ambient = engine.getAmbientProfile();
    double rmsThreshold = ambient.means[0] + (3.0 * ambient.stds[0]); 
    if (f[0] < rmsThreshold) return true;
    if (f[6] < kSilentZcr)   return true;
    if (f[6] < kSpeechZcr && f[0] < kSpeechRms) return true;
    return false;
}

bool DiagnosisVoter::crowdVeto(const float f[Profile::N_FEATURES]) const {
    return (f[5] > kCrowdCentroid && f[1] > kCrowdRmsStab);
}

bool DiagnosisVoter::transientVeto(const float f[Profile::N_FEATURES]) const {
    return (f[4] > kTransientCrest && f[1] > kTransientRmsStab);
}

bool DiagnosisVoter::frictionVoter(const ZScore& z) const {
    return (z.features[6] > kFrictionZcrZ);
}

bool DiagnosisVoter::unbalancedVoter(const ZScore& z) const {
    return (z.features[6] < kUnbalZcrZ && z.features[5] < kUnbalCentZ);
}

bool DiagnosisVoter::criticalVoter(const ZScore& z) const {
    return (z.features[1] > kCriticalStabZ);
}

double DiagnosisVoter::frictionConfidence(const ZScore& z) const {
    const double excess = z.features[6] - kFrictionZcrZ;
    const double conf   = 0.70 + (excess / (6.0 - kFrictionZcrZ)) * 0.25;
    return constrain(conf, 0.70, 0.95);
}

double DiagnosisVoter::unbalancedConfidence(const ZScore& z) const {
    const double exZcr  = fabs(z.features[6]) - fabs(kUnbalZcrZ);
    const double exCent = fabs(z.features[5]) - fabs(kUnbalCentZ);
    const double excess = (exZcr + exCent) * 0.5;
    const double conf   = 0.70 + (excess / 3.0) * 0.25;
    return constrain(conf, 0.70, 0.95);
}

double DiagnosisVoter::healthyConfidence(const ZScore& z) const {
    const double conf = 1.0 - (z.combined / 4.0);
    return constrain(conf, 0.55, 0.99);
}

DiagnosisResult DiagnosisVoter::vote(const ZScore& z, const float f[Profile::N_FEATURES], AnomalyEngine& engine) {
    DiagnosisResult r;
    r.status      = UNKNOWN;
    r.faultType   = FAULT_NONE;
    r.confidence  = 0.0;
    r.anomalyScore = z.combined;
    strncpy(r.label, "Unknown", sizeof(r.label));

    if (ambientVeto(f, engine)) {
        r.status = AMBIENT;
        strncpy(r.label, "Ambient", sizeof(r.label));
        r.confidence = 0.95;
        return r;
    }

    if (crowdVeto(f)) {
        r.status     = AMBIENT;
        r.confidence = 0.87;
        strncpy(r.label, "Crowd / Env Noise", sizeof(r.label));
        return r;
    }

    if (transientVeto(f)) {
        r.status     = AMBIENT;
        r.confidence = 0.92;
        strncpy(r.label, "Transient Shock", sizeof(r.label));
        return r;
    }

    const bool isFriction   = frictionVoter(z);
    const bool isUnbalanced = unbalancedVoter(z);
    const bool isCritical   = criticalVoter(z);
    const bool isStage2Bearing = (z.features[7] > 3.0f && z.features[0] < 1.5f); // High envelope Z-score, low normal RMS Z-score

    if (z.structuralTrendWarning) {
        r.status     = FAULTY;
        r.faultType  = FAULT_CRITICAL;
        r.confidence = 0.85;
        strncpy(r.label, "Structural Trend Warning", sizeof(r.label));
    }
    else if (isStage2Bearing) {
        r.status     = FAULTY;
        r.faultType  = FAULT_FRICTION;
        r.confidence = 0.80; // High envelope Z-score confidence
        strncpy(r.label, "Stg 2 Bearing Defect", sizeof(r.label));
    }
    else if (isFriction && isUnbalanced) {
        r.status     = FAULTY;
        r.faultType  = FAULT_CRITICAL;
        r.confidence = 0.82;
        strncpy(r.label, "Critical Fault", sizeof(r.label));
    }
    else if (isCritical) {
        r.status     = FAULTY;
        r.faultType  = FAULT_CRITICAL;
        r.confidence = (double)constrain(0.80 + (z.features[1] - kCriticalStabZ) * 0.03, 0.80, 0.97);
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
        r.status     = HEALTHY;
        r.faultType  = FAULT_NONE;
        r.confidence = healthyConfidence(z);
        strncpy(r.label, "Healthy", sizeof(r.label));

        if (engine.getState() == CAL_HEALTHY_READY && z.combined < 0.5) {
            engine.updateBaseline(f, z.combined);
        }
    }
    return r;
}

DatabaseComparison DiagnosisVoter::compareWithGlobalDatabase(const float f[Profile::N_FEATURES]) const {
    DatabaseComparison bestMatch;
    bestMatch.matchZScore = 999999.0;
    strcpy(bestMatch.matchName, "Unknown DB");

    // Calculate sum of absolute Z-scores for each global profile to find the closest match
    for(int i = 0; i < 3; i++) {
        double zSum = 0;
        for(int j = 0; j < Profile::N_FEATURES; j++) {
            zSum += abs(f[j] - globalDB[i].p.means[j]) / (globalDB[i].p.stds[j] + 1e-8);
        }
        if(zSum < bestMatch.matchZScore) {
            bestMatch.matchZScore = zSum;
            strcpy(bestMatch.matchName, globalDB[i].name);
        }
    }
    return bestMatch;
}