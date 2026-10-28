#pragma once
// ============================================================
//  INDUSTRIAL DOCTOR v2.1
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

// ── Calibration preset selected from the menu ─────────────────────────────────
enum CalibrationPreset { CAL_PRESET_HEALTHY, CAL_PRESET_AMBIENT };

// ── Internal calibration state machine ────────────────────────────────────────
enum CalibrationState {
    CAL_AMBIENT_SETTLING,   // Motor OFF: Collecting features for ambient noise profile
    CAL_AMBIENT_READY,      // Motor OFF: Ambient profile is complete. Waiting for user.
    CAL_HEALTHY_SETTLING,   // Motor ON: Collecting features for healthy motor profile
    CAL_HEALTHY_READY,      // Motor ON: Healthy profile is complete. Normal operation.
    CAL_WARMUP,             // Legacy state, will be phased out
    CAL_COLLECTING,         // Legacy state, will be phased out
    CAL_SETTLING,           // Legacy state, will be phased out
    CAL_READY               // Legacy state, will be phased out
};

// A Profile is a statistical representation of a machine state (e.g.,
// "healthy 8V" or "ambient office noise"). It contains the mean and
// standard deviation for each of the 7 core features.
struct Profile {
    static constexpr int N_FEATURES = 8;
    float means[N_FEATURES];
    float stds[N_FEATURES];
};

// ── Output of the scoring step ────────────────────────────────────────────────
struct ZScore {
    float features[Profile::N_FEATURES];
    float combined;
    bool  structuralTrendWarning;
};

// ─────────────────────────────────────────────────────────────────────────────
class AnomalyEngine {
public:
    AnomalyEngine(Profile defaultProfile);

    // Call every loop iteration — drives the internal state machine
    void feedWindow(const float f[Profile::N_FEATURES]);

    // Calculates the anomaly score for a given window of feature data.
    ZScore getAnomalyScore(const float f[Profile::N_FEATURES], const Profile& profile);

    // Updates the rolling baseline with a new window of "healthy" data.
    void updateBaseline(const float f[Profile::N_FEATURES], float combinedScore);

    // Returns the current state of the calibration state machine.
    CalibrationState getState() const;

    // Manually advances the state machine to the next state.
    void advanceState();

    // Menu control hooks to restart calibration buffers cleanly
    void startAmbientCalibration();
    void startHealthyCalibration();

    // Returns the active healthy profile.
    Profile getHealthyProfile() const;

    // Returns the ambient noise profile.
    Profile getAmbientProfile() const;

    void setAmbientProfile(const Profile& p) { _ambientProfile = p; }
    void setHealthyProfile(const Profile& p) { _healthyProfile = p; }
    void forceState(CalibrationState s) { _state = s; }

    // Get calibration progress (0.0 to 1.0)
    float calibProgress() const;

private:
    // ── Timing constants ───────────────────────────────────────────────────────
    // Number of windows to collect for each calibration phase.
    static constexpr int kSettleWindows = 150;
    static constexpr int kTrendWindow = 50;

    // ── State ─────────────────────────────────────────────────────────────────
    CalibrationState _state;
    Profile          _healthyProfile;
    Profile          _ambientProfile;
    char             _predictionStr[64]; // Buffer for the prediction string

    // Buffer to hold feature data during the settling/collection phase.
    float _settleBuffer[kSettleWindows][Profile::N_FEATURES];
    int   _settleIndex;
    
    // Trend tracking
    float _trendBuffer[kTrendWindow][Profile::N_FEATURES];
    int   _trendIndex;
    bool  _trendWarningActive;
};