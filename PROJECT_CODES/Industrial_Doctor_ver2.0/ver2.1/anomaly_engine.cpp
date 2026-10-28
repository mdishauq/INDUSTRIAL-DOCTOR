// ============================================================
//  INDUSTRIAL DOCTOR v2.0
//  anomaly_engine.cpp
// ============================================================
#include "anomaly_engine.h"
#include <math.h>
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
//  MASTER DATA PROFILES
//  All values derived from real CSV analysis of 7 recorded datasets.
//  Used as:
//    (a) Initial active profile before calibration completes (fallback)
//    (b) Starting point that live calibration overwrites at boot
// ─────────────────────────────────────────────────────────────────────────────

MotorProfile AnomalyEngine::defaultProfile8V() {
    // CSV stats — healthy_8v.csv  (n=247)
    return MotorProfile {
        {421.62,  54.24},   // rms       μ±σ
        { 25.03,  13.40},   // rmsStab
        {593.16, 169.03},   // peakFreq  (wide — motor switches modes under load)
        {  0.178,  0.023},  // zcr
        {739.57,  56.53},   // centroid
        {  3.077,  0.254},  // crest
        {  2.644,  0.184},  // kurt
        70.0                // ambientRmsThreshold
    };
}

MotorProfile AnomalyEngine::defaultProfile6V() {
    // CSV stats — healthy_6v.csv  (n=240)
    return MotorProfile {
        {335.02,  43.29},
        { 17.37,  10.40},
        {778.21, 243.37},
        {  0.146,  0.022},
        {929.60,  99.77},
        {  3.244,  0.372},
        {  2.870,  0.250},
        70.0
    };
}

// ─────────────────────────────────────────────────────────────────────────────
AnomalyEngine::AnomalyEngine()
    : _state(CAL_WARMUP),
      _voltageMode(MODE_UNKNOWN),
      _windowCount(0),
      _settlingWindowCount(0),
      _warmupCentroidSum(0.0),
      _warmupCentroidCount(0),
      _warmupRmsSum(0.0),
      _warmupRmsCount(0),
      _sumRms(0),  _sumSqRms(0),
      _sumStab(0), _sumSqStab(0),
      _sumPeak(0), _sumSqPeak(0),
      _sumZcr(0),  _sumSqZcr(0),
      _sumCent(0), _sumSqCent(0),
      _sumCrest(0),_sumSqCrest(0),
      _sumKurt(0), _sumSqKurt(0),
      _calCount(0)
{
    // Start with 8V profile as the safe default
    _activeProfile = defaultProfile8V();
}

// ─────────────────────────────────────────────────────────────────────────────
float AnomalyEngine::calibProgress() const {
    switch (_state) {
        case CAL_WARMUP:      return 0.0f;
        case CAL_COLLECTING:  return (float)_calCount / (float)kCalibWindows;
        case CAL_SETTLING:
        case CAL_READY:       return 1.0f;
    }
    return 0.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
//  zScore — safe Z-score with sigma floor.
//  The kSigmaFloor (5.0) is intentionally conservative: it prevents the system
//  from becoming hypersensitive to features whose healthy variance is near-zero.
// ─────────────────────────────────────────────────────────────────────────────
double AnomalyEngine::zScore(double val, double mean, double stddev) const {
    double sd = fabs(stddev) < kSigmaFloor ? kSigmaFloor : fabs(stddev);
    return (val - mean) / sd;
}

// ─────────────────────────────────────────────────────────────────────────────
//  detectVoltageMode — called after kWarmupWindows are collected.
//  Uses Centroid as the primary discriminator:
//    8V centroid μ=739,  6V centroid μ=929,  midpoint=834.6
//  The 190Hz gap between modes makes this very reliable.
// ─────────────────────────────────────────────────────────────────────────────
void AnomalyEngine::detectVoltageMode() {
    if (_warmupCentroidCount == 0) {
        // Motor was off during warmup — default to 8V
        _voltageMode   = MODE_8V;
        _activeProfile = defaultProfile8V();
        return;
    }

    const double avgCentroid = _warmupCentroidSum / (double)_warmupCentroidCount;

    if (avgCentroid > kVoltageCentroidMidpoint) {
        _voltageMode   = MODE_6V;
        _activeProfile = defaultProfile6V();
    } else {
        _voltageMode   = MODE_8V;
        _activeProfile = defaultProfile8V();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  finalizeCalibration — computes μ and σ from accumulated sums.
//  Only overwrites the profile if enough windows were collected (≥10).
//  Applies kSigmaFloor to every feature.
// ─────────────────────────────────────────────────────────────────────────────
void AnomalyEngine::finalizeCalibration() {
    if (_calCount < 10) {
        // Too few valid windows — keep master data profile as-is
        return;
    }

    const double n = (double)_calCount;

    auto makeStat = [&](double sum, double sumSq) -> FeatureStat {
        const double mu    = sum / n;
        const double var   = (sumSq / n) - (mu * mu);
        double       sigma = (var > 0.0) ? sqrt(var) : kSigmaFloor;
        if (sigma < kSigmaFloor) sigma = kSigmaFloor;
        return {mu, sigma};
    };

    _activeProfile.rms      = makeStat(_sumRms,   _sumSqRms);
    _activeProfile.rmsStab  = makeStat(_sumStab,  _sumSqStab);
    _activeProfile.peakFreq = makeStat(_sumPeak,  _sumSqPeak);
    _activeProfile.zcr      = makeStat(_sumZcr,   _sumSqZcr);
    _activeProfile.centroid = makeStat(_sumCent,  _sumSqCent);
    _activeProfile.crest    = makeStat(_sumCrest, _sumSqCrest);
    _activeProfile.kurt     = makeStat(_sumKurt,  _sumSqKurt);
    // ambientRmsThreshold stays at the master-data value (70.0)
}

// ─────────────────────────────────────────────────────────────────────────────
//  feedWindow — the main state machine driver.
//  Must be called once per loop iteration before scoreWindow.
// ─────────────────────────────────────────────────────────────────────────────
void AnomalyEngine::feedWindow(const MotorFeatures& f) {
    if (!f.dataValid) return;
    _windowCount++;

    switch (_state) {

        // ── Phase 1: Discard + detect voltage mode ─────────────────────────────
        case CAL_WARMUP:
            // Calculate dynamic ambient floor from ALL warmup frames
            if (f.rms > 0) {
                _warmupRmsSum += f.rms;
                _warmupRmsCount++;
            }

            // Only collect centroid when motor appears to be running
            if (f.rms > _activeProfile.ambientRmsThreshold) {
                _warmupCentroidSum += f.spectralCentroid;
                _warmupCentroidCount++;
            }
            
            if (_windowCount >= kWarmupWindows) {
                detectVoltageMode();
                
                // Set the dynamic ambient threshold (30% headroom above noise floor)
                if (_warmupRmsCount > 0) {
                    double avgAmbientRms = _warmupRmsSum / (double)_warmupRmsCount;
                    // Lower bound of 70 to prevent hypersensitivity in dead silence
                    // Upper bound of 350 to prevent false-gate if motor was on or loud bang occurred
                    double dynamicGate = avgAmbientRms * 1.3;
                    if (dynamicGate < 70.0) dynamicGate = 70.0;
                    if (dynamicGate > 350.0) dynamicGate = 350.0;
                    _activeProfile.ambientRmsThreshold = dynamicGate;
                }
                
                _state = CAL_COLLECTING;
            }
            break;

        // ── Phase 2: Accumulate features for μ/σ computation ──────────────────
        case CAL_COLLECTING:
            // Only include windows where the motor is clearly running
            if (f.rms > _activeProfile.ambientRmsThreshold) {
                _sumRms   += f.rms;             _sumSqRms   += f.rms * f.rms;
                _sumStab  += f.rmsStab;         _sumSqStab  += f.rmsStab * f.rmsStab;
                _sumPeak  += f.peakFreq;        _sumSqPeak  += f.peakFreq * f.peakFreq;
                _sumZcr   += f.zcr;             _sumSqZcr   += f.zcr * f.zcr;
                _sumCent  += f.spectralCentroid;_sumSqCent  += f.spectralCentroid * f.spectralCentroid;
                _sumCrest += f.crestFactor;     _sumSqCrest += f.crestFactor * f.crestFactor;
                _sumKurt  += f.kurtosis;        _sumSqKurt  += f.kurtosis * f.kurtosis;
                _calCount++;
            }

            if (_calCount >= kCalibWindows) {
                finalizeCalibration();
                _state = CAL_SETTLING;
            }
            break;

        // ── Phase 3: EMA begins, settling display period ───────────────────────
        //    DiagnosisVoter runs during this phase and triggers baseline updates.
        //    After kSettleWindows frames, transition to READY.
        case CAL_SETTLING:
            _settlingWindowCount++;
            if (_settlingWindowCount >= kSettleWindows) {
                _state = CAL_READY;
            }
            break;

        case CAL_READY:
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  scoreWindow — compute individual and combined Z-scores.
//  Available during CAL_SETTLING and CAL_READY.
//
//  Feature weights derived from Z-score separability analysis on all 7 datasets:
//    ZCR      × 0.35  — separates ALL fault types (friction +4.05σ, unbal -3.22σ)
//    Centroid × 0.30  — crowd +5.42σ, unbalanced -3.05σ, 6V vs 8V +3.36σ
//    RmsStab  × 0.15  — crowd +2.94σ, friction +1.92σ
//    PeakHz   × 0.10  — unbalanced -1.78σ (moderate with real σ=169)
//    Kurt     × 0.07  — speech +4.74σ (backup ambient detector)
//    Crest    × 0.03  — weak across all classes, included for completeness
//    RMS              — NOT in weighted score. Used only as power-on gate.
// ─────────────────────────────────────────────────────────────────────────────
ZScoreFrame AnomalyEngine::scoreWindow(const MotorFeatures& f) const {
    ZScoreFrame frame = {};

    if (!f.dataValid ||
        (_state != CAL_READY && _state != CAL_SETTLING)) {
        return frame;
    }

    const MotorProfile& p = _activeProfile;

    frame.zRms      = zScore(f.rms,               p.rms.mean,      p.rms.stddev);
    frame.zRmsStab  = zScore(f.rmsStab,            p.rmsStab.mean,  p.rmsStab.stddev);
    frame.zPeakHz   = zScore(f.peakFreq,           p.peakFreq.mean, p.peakFreq.stddev);
    frame.zZcr      = zScore(f.zcr,                p.zcr.mean,      p.zcr.stddev);
    frame.zCentroid = zScore(f.spectralCentroid,   p.centroid.mean, p.centroid.stddev);
    frame.zCrest    = zScore(f.crestFactor,        p.crest.mean,    p.crest.stddev);
    frame.zKurt     = zScore(f.kurtosis,           p.kurt.mean,     p.kurt.stddev);

    frame.combined =
        0.35 * fabs(frame.zZcr)      +
        0.30 * fabs(frame.zCentroid)  +
        0.15 * fabs(frame.zRmsStab)  +
        0.10 * fabs(frame.zPeakHz)   +
        0.07 * fabs(frame.zKurt)     +
        0.03 * fabs(frame.zCrest);

    frame.valid = true;
    return frame;
}

// ─────────────────────────────────────────────────────────────────────────────
//  updateBaseline — EMA update of profile means only.
//
//  σ values are intentionally NOT updated after calibration.
//  Reason: if σ grew over time (e.g. in a noisy environment), the Z-score
//  denominator would widen and the system would become progressively less
//  sensitive to faults. Keeping σ fixed means "if the machine drifts THIS far
//  from normal, it's still a fault" — the sensitivity doesn't decay.
//
//  GUARDRAIL: if |combined score| > kGuardrail (1.8), the update is blocked.
//  This prevents the system from absorbing a developing fault into its memory.
//  1.8 is chosen because both known fault types score above 2.0 in the data.
// ─────────────────────────────────────────────────────────────────────────────
void AnomalyEngine::updateBaseline(const MotorFeatures& f, double combinedScore) {
    if (_state != CAL_READY && _state != CAL_SETTLING) return;
    if (fabs(combinedScore) > kGuardrail) return;   // guardrail engaged

    constexpr double a = kEmaAlpha;       //  0.01
    constexpr double b = 1.0 - kEmaAlpha; //  0.99

    _activeProfile.rms.mean      = a * f.rms             + b * _activeProfile.rms.mean;
    _activeProfile.rmsStab.mean  = a * f.rmsStab          + b * _activeProfile.rmsStab.mean;
    _activeProfile.peakFreq.mean = a * f.peakFreq         + b * _activeProfile.peakFreq.mean;
    _activeProfile.zcr.mean      = a * f.zcr              + b * _activeProfile.zcr.mean;
    _activeProfile.centroid.mean = a * f.spectralCentroid + b * _activeProfile.centroid.mean;
    _activeProfile.crest.mean    = a * f.crestFactor      + b * _activeProfile.crest.mean;
    _activeProfile.kurt.mean     = a * f.kurtosis         + b * _activeProfile.kurt.mean;
}