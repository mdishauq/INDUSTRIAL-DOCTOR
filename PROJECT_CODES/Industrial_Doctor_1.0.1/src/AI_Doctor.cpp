#include "AI_Doctor.h"

namespace {
constexpr double kEpsilon = 1e-9;
constexpr double kIdleRmsThreshold = 70.0;
constexpr double kIdleFreqThreshold = 100.0;

constexpr double kMaxModeFreq = 630.0;
constexpr double kMidModeFreqLow = 550.0;
constexpr double kEcoModeFreqLow = 450.0;

constexpr double kCriticalKurtZ = 15.0;
constexpr double kImbalanceKurtZ = 4.5;
constexpr double kCriticalCrest = 5.5;
constexpr double kFrictionCentroid = 1100.0;

struct ModeModel {
	double rmsMean;
	double rmsStd;
	double kurtMean;
	double kurtStd;
	MotorStatus healthyLabel;
};

ModeModel selectMode(double peakFreq) {
	if (peakFreq > kMaxModeFreq) {
		return {680.0, 60.0, 2.25, 0.3, HEALTHY_MAX};
	}
	if (peakFreq >= kMidModeFreqLow) {
		return {470.0, 50.0, 2.22, 0.3, HEALTHY_MID};
	}
	if (peakFreq >= kEcoModeFreqLow) {
		return {490.0, 40.0, 2.15, 0.3, HEALTHY_ECO};
	}
	return {490.0, 40.0, 2.15, 0.3, HEALTHY_ECO};
}

MotorProfile defaultHealthyProfile() {
	return MotorProfile{
			{490.0, 40.0},
			{580.0, 60.0},
			{2.15, 0.30},
			{2.5, 0.8},
			{900.0, 180.0},
			{0.08, 0.03},
			kIdleRmsThreshold,
	};
}

MotorProfile defaultAmbientProfile() {
	return MotorProfile{
			{35.0, 20.0},
			{120.0, 60.0},
			{2.0, 0.5},
			{1.8, 0.4},
			{220.0, 120.0},
			{0.02, 0.02},
			kIdleRmsThreshold,
	};
}

} // namespace

AIDoctor::AIDoctor() : _healthyProfile(defaultHealthyProfile()), _ambientProfile(defaultAmbientProfile()), _lastConfidence(0.0f) {}

void AIDoctor::setHealthyProfile(const MotorProfile &profile) {
	_healthyProfile = profile;
}

void AIDoctor::setAmbientProfile(const MotorProfile &profile) {
	_ambientProfile = profile;
}

MotorProfile AIDoctor::healthyProfile() const {
	return _healthyProfile;
}

MotorProfile AIDoctor::ambientProfile() const {
	return _ambientProfile;
}

double AIDoctor::safeStd(double stddev) {
	return (fabs(stddev) < kEpsilon) ? 1.0 : fabs(stddev);
}

double AIDoctor::zScore(double value, double mean, double stddev) {
	return (value - mean) / safeStd(stddev);
}

double AIDoctor::absDouble(double value) {
	return value < 0.0 ? -value : value;
}

double AIDoctor::clampDouble(double value, double low, double high) {
	if (value < low) {
		return low;
	}
	if (value > high) {
		return high;
	}
	return value;
}

DiagnosisResult AIDoctor::runDiagnosis(const MotorFeatures &features) const {
	DiagnosisResult result{UNKNOWN, 0.0, 0.0};

	if (!features.dataValid) {
		return result;
	}

	if (features.rms < kIdleRmsThreshold || features.peakFreq < kIdleFreqThreshold) {
		result.status = SYSTEM_IDLE;
		result.confidence = 100.0;
		result.anomalyScore = 0.0;
		return result;
	}

	const ModeModel mode = selectMode(features.peakFreq);
	const double zRms = absDouble(zScore(features.rms, mode.rmsMean, mode.rmsStd));
	const double zKurt = absDouble(zScore(features.kurtosis, mode.kurtMean, mode.kurtStd));
	const double averageZ = 0.5 * (zRms + zKurt);
	result.anomalyScore = averageZ;
	result.confidence = clampDouble(100.0 - (averageZ * 10.0), 0.0, 100.0);

	if (zKurt > kCriticalKurtZ || features.crestFactor > kCriticalCrest) {
		result.status = CRITICAL_LOOSENESS;
		return result;
	}

	if (zKurt > kImbalanceKurtZ) {
		result.status = IMBALANCE_DETECTED;
		return result;
	}

	if (features.spectralCentroid > kFrictionCentroid) {
		result.status = FRICTION_WARNING;
		return result;
	}

	result.status = mode.healthyLabel;
	return result;
}

DiagnosisResult AIDoctor::diagnose(const MotorFeatures &features) const {
	const DiagnosisResult result = runDiagnosis(features);
	_lastConfidence = static_cast<float>(result.confidence);
	return result;
}

MotorStatus AIDoctor::analyze(const MotorFeatures &features) const {
	const DiagnosisResult result = runDiagnosis(features);
	_lastConfidence = static_cast<float>(result.confidence);
	return result.status;
}

float AIDoctor::getConfidence() const {
	return _lastConfidence;
}

