#ifndef FEATURE_DATA_H
#define FEATURE_DATA_H

#include <Arduino.h>

#define HISTORY_SIZE 10 

struct MotorFeatures {
    double rms;
    double rmsStability;
    double kurtosis;
    double kurtosisStability;
    double peakFreq;
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
    
    double _rmsHistory[HISTORY_SIZE];
    double _kurtosisHistory[HISTORY_SIZE];
    int _historyIndex;
    bool _bufferFull;
    bool _initialized;
};

#endif