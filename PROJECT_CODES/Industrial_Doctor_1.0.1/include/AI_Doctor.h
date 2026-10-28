#ifndef AI_DOCTOR_H
#define AI_DOCTOR_H

#include <Arduino.h>

#include "feature_data.h"

enum MotorStatus {
	SYSTEM_IDLE,
	HEALTHY_ECO,
	HEALTHY_MID,
	HEALTHY_MAX,
	IMBALANCE_DETECTED,
	FRICTION_WARNING,
	CRITICAL_LOOSENESS,
	UNKNOWN
};

struct FeatureStat {
	double mean;
	double stddev;
};

struct MotorProfile {
	FeatureStat rms;
	FeatureStat peakFreq;
	FeatureStat kurtosis;
	FeatureStat crestFactor;
	FeatureStat spectralCentroid;
	FeatureStat zcr;
	double ambientRmsThreshold;
};

struct DiagnosisResult {
	MotorStatus status;
	double confidence;
	double anomalyScore;
};

class AIDoctor {
public:
	AIDoctor();

	void setHealthyProfile(const MotorProfile &profile);
	void setAmbientProfile(const MotorProfile &profile);
	MotorProfile healthyProfile() const;
	MotorProfile ambientProfile() const;

	DiagnosisResult diagnose(const MotorFeatures &features) const;
	MotorStatus analyze(const MotorFeatures &features) const;
	float getConfidence() const;

private:
	static double safeStd(double stddev);
	static double zScore(double value, double mean, double stddev);
	static double absDouble(double value);
	static double clampDouble(double value, double low, double high);
	DiagnosisResult runDiagnosis(const MotorFeatures &features) const;

	MotorProfile _healthyProfile;
	MotorProfile _ambientProfile;
	mutable float _lastConfidence;
};

#endif

