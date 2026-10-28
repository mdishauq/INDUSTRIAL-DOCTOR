#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include "AI_Doctor.h"
#include "feature_data.h"

namespace {
constexpr int kScreenWidth = 128;
constexpr int kScreenHeight = 64;
constexpr uint8_t kOledAddress = 0x3C;

Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &Wire, -1);
FeatureExtractor featureExtractor;
AIDoctor doctor;
bool oledReady = false;
uint8_t pageIndex = 0;
unsigned long lastPageSwitchMs = 0;
constexpr unsigned long kPageIntervalMs = 1800;

void updatePageIndex() {
	const unsigned long now = millis();
	if (now - lastPageSwitchMs >= kPageIntervalMs) {
		pageIndex = (pageIndex + 1) % 4;
		lastPageSwitchMs = now;
	}
}

const char *statusText(MotorStatus status) {
	switch (status) {
		case SYSTEM_IDLE:
			return "SYSTEM_IDLE";
		case HEALTHY_ECO:
			return "HEALTHY_ECO";
		case HEALTHY_MID:
			return "HEALTHY_MID";
		case HEALTHY_MAX:
			return "HEALTHY_MAX";
		case IMBALANCE_DETECTED:
			return "IMBALANCE";
		case FRICTION_WARNING:
			return "FRICTION";
		case CRITICAL_LOOSENESS:
			return "CRIT_LOOSE";
		default:
			return "UNKNOWN";
	}
}

void drawFeatureFrame(const MotorFeatures &f, const DiagnosisResult &diagnosis) {
	if (!oledReady) {
		return;
	}

	updatePageIndex();

	display.clearDisplay();
	display.setTextColor(SSD1306_WHITE);
	display.setTextSize(1);

	display.setCursor(0, 0);
	display.print("Feature Monitor");
	display.setCursor(106, 0);
	display.print(pageIndex + 1);
	display.print("/4");

	if (!f.dataValid) {
		display.setCursor(0, 24);
		display.print("No valid data");
		display.display();
		return;
	}

	if (pageIndex == 0) {
		display.setTextSize(1);
		display.setCursor(0, 12);
		display.print("RMS");
		display.setTextSize(2);
		display.setCursor(0, 20);
		display.printf("%.1f", f.rms);

		display.setTextSize(1);
		display.setCursor(0, 36);
		display.print("PEAK Hz");
		display.setTextSize(2);
		display.setCursor(0, 44);
		display.printf("%.0f", f.peakFreq);
	} else if (pageIndex == 1) {
		display.setTextSize(1);
		display.setCursor(0, 12);
		display.print("KURT");
		display.setTextSize(2);
		display.setCursor(0, 20);
		display.printf("%.2f", f.kurtosis);

		display.setTextSize(1);
		display.setCursor(0, 36);
		display.print("CREST");
		display.setTextSize(2);
		display.setCursor(0, 44);
		display.printf("%.2f", f.crestFactor);
	} else if (pageIndex == 2) {
		display.setTextSize(1);
		display.setCursor(0, 12);
		display.print("CENT Hz");
		display.setTextSize(2);
		display.setCursor(0, 20);
		display.printf("%.0f", f.spectralCentroid);

		display.setTextSize(1);
		display.setCursor(0, 36);
		display.print("ZCR");
		display.setTextSize(2);
		display.setCursor(0, 44);
		display.printf("%.3f", f.zcr);
	} else {
		display.setTextSize(1);
		display.setCursor(0, 12);
		display.print("STATE");
		display.setTextSize(2);
		display.setCursor(0, 20);
		display.print(statusText(diagnosis.status));

		display.setTextSize(1);
		display.setCursor(0, 40);
		display.print("CONFIDENCE");
		display.setTextSize(2);
		display.setCursor(0, 48);
		display.printf("%.0f%%", diagnosis.confidence);
	}

	display.display();
}
} // namespace

void setup() {
	Serial.begin(2000000);
	delay(150);

	Wire.begin(1, 2);
	oledReady = display.begin(SSD1306_SWITCHCAPVCC, kOledAddress);

	if (!featureExtractor.begin()) {
		Serial.println("Feature extractor init failed");
	} else {
		Serial.println("Feature extractor ready");
		Serial.println("rms,peak_hz,kurtosis,crest,centroid_hz,zcr,state,confidence,score");
	}
}

void loop() {
	const MotorFeatures features = featureExtractor.getNextFeatures();
	const DiagnosisResult diagnosis = doctor.diagnose(features);

	if (features.dataValid) {
		Serial.printf("%.3f,%.3f,%.5f,%.5f,%.3f,%.6f,%s,%.3f,%.3f\n", features.rms, features.peakFreq,
									features.kurtosis, features.crestFactor, features.spectralCentroid, features.zcr,
									statusText(diagnosis.status), diagnosis.confidence, diagnosis.anomalyScore);
	} else {
		Serial.println("No valid data");
	}

	drawFeatureFrame(features, diagnosis);
	delay(150);
}

