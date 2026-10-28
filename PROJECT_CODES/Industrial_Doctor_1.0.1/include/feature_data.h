#ifndef FEATURE_DATA_H
#define FEATURE_DATA_H

#include <Arduino.h>

struct MotorFeatures {
	double rms;
	double peakFreq;
	double kurtosis;
	double crestFactor;
	double spectralCentroid;
	double zcr;
	bool dataValid;
};

class FeatureExtractor {
public:
	FeatureExtractor();
	bool begin();
	MotorFeatures getNextFeatures();

private:
	void setupI2S();
	void computeSpectralMetrics(const double *samples, size_t count, double sampleRate,
															double *peakFreq, double *spectralCentroid);
	int signWithHysteresis(double sample, double threshold);

	bool _initialized;
};

#endif

