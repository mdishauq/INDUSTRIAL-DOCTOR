// ============================================================
//  INDUSTRIAL DOCTOR v2.1  —  main.cpp
// ============================================================
#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include "boot_animation.h"
#include "calibration_menu.h"
#include "feature_extraction.h"
#include "anomaly_engine.h"
#include "diagnosis_voter.h"

// ── Hardware ───────────────────────────────────────────────────────────────────
namespace {
    constexpr int     kScreenWidth   = 128;
    constexpr int     kScreenHeight  = 64;
    constexpr uint8_t kOledAddress   = 0x3C;
}

static Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &Wire, -1);
static FeatureExtractor featureExtractor;

static Profile createDefaultProfile() {
    Profile p;
    for (int i = 0; i < Profile::N_FEATURES; i++) {
        p.means[i] = 100.0;
        p.stds[i] = 10.0;
    }
    return p;
}

static AnomalyEngine     engine(createDefaultProfile());
static DiagnosisVoter    voter;
static CalibrationMenu   calibMenu;
static bool              oledReady = false;

static uint8_t        pageIndex        = 0;
static unsigned long  lastPageSwitchMs = 0;
static constexpr unsigned long kPageIntervalMs = 1800;

static uint8_t       dotTick   = 0;
static unsigned long lastDotMs = 0;

static void updateDots() {
    if (millis() - lastDotMs >= 380) {
        dotTick = (dotTick + 1) % 6;
        lastDotMs = millis();
    }
}

static const char* animDots() {
    switch (dotTick) {
        case 0: return "   ";
        case 1: return ".  ";
        case 2: return ".. ";
        case 3: return "...";
        case 4: return ".. ";
        case 5: return ".  ";
    }
    return "...";
}

static void drawHeader(const char* title) {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print(title);
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
}

// ─────────────────────────────────────────────────────────────────────────────
static void drawStatusPage(const float f[Profile::N_FEATURES], const DiagnosisResult& d, const DatabaseComparison& db) {
    if (!oledReady) return;

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    const char* statusStr =
        (d.status == HEALTHY) ? "HEALTHY" :
        (d.status == FAULTY)  ? "FAULT"   :
        (d.status == AMBIENT) ? "AMBIENT" : "UNKNOWN";

    if (d.status == FAULTY) {
        display.fillRect(0, 0, 128, 16, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
    }

    display.setTextSize(2);
    display.setCursor(2, 1);
    display.print(statusStr);

    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 18);
    display.print(d.label);

    display.setCursor(0, 27);
    display.printf("Conf: %.1f%%", d.confidence * 100.0);

    display.drawFastHLine(0, 36, 128, SSD1306_WHITE);

    // Display Secondary Database Match
    display.setCursor(0, 39);
    display.print(db.matchName); 
    display.setCursor(0, 49);
    display.printf("DB Error: %.1f", db.matchZScore);

    const float scoreNorm = (float)constrain(d.anomalyScore / 4.0, 0.0, 1.0);
    const int   scoreW    = (int)(scoreNorm * 128.0f);
    display.drawFastHLine(0, 63, 128, SSD1306_WHITE);
    if (scoreW > 0)
        display.drawFastHLine(0, 62, scoreW, SSD1306_WHITE);

    display.display();
}

static void drawFeaturePages(const float f[Profile::N_FEATURES], const DiagnosisResult& d, const DatabaseComparison& db) {
    if (!oledReady) return;

    const unsigned long now = millis();
    if (now - lastPageSwitchMs >= kPageIntervalMs) {
        pageIndex = (pageIndex + 1) % 4;
        lastPageSwitchMs = now;
    }

    if (pageIndex == 3) {
        drawStatusPage(f, d, db);
        return;
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Features  ");
    display.print(pageIndex + 1);
    display.print("/4");

    switch (pageIndex) {
        case 0:
            display.setTextSize(1); display.setCursor(0, 12); display.print("RMS");
            display.setTextSize(2); display.setCursor(0, 20); display.printf("%.1f", f[0]);
            display.setTextSize(1); display.setCursor(0, 38); display.print("PEAK Hz");
            display.setTextSize(2); display.setCursor(0, 46); display.printf("%.0f", f[2]);
            break;

        case 1:
            display.setTextSize(1); display.setCursor(0, 12); display.print("KURT");
            display.setTextSize(2); display.setCursor(0, 20); display.printf("%.2f", f[3]);
            display.setTextSize(1); display.setCursor(0, 38); display.print("CREST");
            display.setTextSize(2); display.setCursor(0, 46); display.printf("%.2f", f[4]);
            break;

        case 2:
            display.setTextSize(1); display.setCursor(0, 12); display.print("CENTROID Hz");
            display.setTextSize(2); display.setCursor(0, 20); display.printf("%.0f", f[5]);
            display.setTextSize(1); display.setCursor(0, 38); display.print("ZCR");
            display.setTextSize(2); display.setCursor(0, 46); display.printf("%.4f", f[6]);
            break;
    }
    display.display();
}

static void serialLog(const float f[Profile::N_FEATURES], const DiagnosisResult& d) {
    const char* statusStr =
        (d.status == HEALTHY) ? "HEALTHY" :
        (d.status == FAULTY)  ? "FAULTY"  :
        (d.status == AMBIENT) ? "AMBIENT" : "UNKNOWN";

    Serial.printf("%.3f,%.3f,%.3f,%.5f,%.5f,%.3f,%.6f,%s,%s,%.3f,%.3f\n",
                  f[0], f[1], f[2], f[3], f[4], f[5], f[6],
                  statusStr, d.label, d.confidence, d.anomalyScore);
}

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(2000000);
    delay(150);

    Wire.begin(1, 2);
    oledReady = display.begin(SSD1306_SWITCHCAPVCC, kOledAddress);

    oled_boot_animation(display, oledReady);

    pinMode(0, INPUT_PULLUP);

    if (!featureExtractor.begin()) {
        Serial.println("ERR: Feature extractor init failed");
    } else {
        Serial.println("# Industrial Doctor v2.1 — Two-Stage Calibration");
        Serial.println("# rms,rmsStab,peakHz,kurt,crest,centroid,zcr,status,label,confidence,anomalyScore");
    }

    calibMenu.begin(display);
    calibMenu.setEngine(&engine);

    // Hook menu triggers to engine state
    calibMenu.onStartAmbient([](){ engine.startAmbientCalibration(); });
    calibMenu.onStartHealthy([](){ engine.startHealthyCalibration(); });

    calibMenu.activate(); 
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    const MotorFeatures features = featureExtractor.getNextFeatures();
    float f[Profile::N_FEATURES];
    features.toArray(f);

    // Always feed window so calibration works while the menu is open
    engine.feedWindow(f);

    if (calibMenu.isActive()) {
        calibMenu.loop();
        return;
    }

    // Normal Operation
    const ZScore z = engine.getAnomalyScore(f, engine.getHealthyProfile());
    const DiagnosisResult diag = voter.vote(z, f, engine);
    const DatabaseComparison dbMatch = voter.compareWithGlobalDatabase(f);

    serialLog(f, diag);
    drawFeaturePages(f, diag, dbMatch);

    delay(50); 
}