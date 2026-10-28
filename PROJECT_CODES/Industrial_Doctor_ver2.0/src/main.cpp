// ============================================================
//  INDUSTRIAL DOCTOR v2.0  —  main.cpp
//
//  Display states:
//    [A] Boot animation        boot_animation.h (logo + SYSTEM ONLINE)
//    [B] Warming up            Logo + "WARMING UP..." while motor settles
//    [C] Calibrating           Progress bar + window count
//    [D] Rolling baseline      Animated EMA settling indicator
//    [E] Normal operation      4 rotating pages + dedicated status page
//
//  Serial output format (CSV):
//    rms, rmsStab, peakHz, kurt, crest, centroid, zcr,
//    STATUS, label, confidence, anomalyScore
// ============================================================
#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include "boot_animation.h"
#include "feature_extraction.h"
#include "anomaly_engine.h"
#include "diagnosis_voter.h"

// ── Hardware ───────────────────────────────────────────────────────────────────
namespace {
    constexpr int     kScreenWidth   = 128;
    constexpr int     kScreenHeight  = 64;
    constexpr uint8_t kOledAddress   = 0x3C;
}

// ── Globals ────────────────────────────────────────────────────────────────────
static Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &Wire, -1);
static FeatureExtractor  featureExtractor;
static AnomalyEngine     engine;
static DiagnosisVoter    voter;
static bool              oledReady       = false;

// Page rotation for normal operation
static uint8_t        pageIndex       = 0;
static unsigned long  lastPageSwitchMs = 0;
static constexpr unsigned long kPageIntervalMs = 1800;

// Dot animation shared across calibration screens
static uint8_t       dotTick    = 0;
static unsigned long lastDotMs  = 0;

// ─────────────────────────────────────────────────────────────────────────────
//  DISPLAY HELPERS
// ─────────────────────────────────────────────────────────────────────────────

static void updateDots() {
    if (millis() - lastDotMs >= 380) {
        dotTick = (dotTick + 1) % 6;
        lastDotMs = millis();
    }
}

// Returns an animated dot string cycling: "" → "." → ".." → "..." → ".." → "."
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
//  [B] WARMING UP screen
//  Shown during CAL_WARMUP — motor is stabilising, voltage mode not yet known
// ─────────────────────────────────────────────────────────────────────────────
static void drawWarmingUp() {
    if (!oledReady) return;
    updateDots();

    display.clearDisplay();
    drawHeader("INDUSTRIAL DOCTOR v2");

    display.setTextSize(1);
    display.setCursor(0, 14);
    display.print("WARMING UP");
    display.print(animDots());

    display.setCursor(0, 26);
    display.print("Detecting motor mode");

    // Small animated "heartbeat" indicator
    static uint8_t hbPhase = 0;
    hbPhase = (hbPhase + 1) % 32;
    const int8_t hbY[32] = {
        0, 0, 0, 0, -2, -4, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, -2, -4, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    const int baseY = 50;
    for (int x = 0; x < 128; x++) {
        int y = baseY + (int)hbY[(x / 4 + hbPhase) % 32];
        display.drawPixel(x, constrain(y, 0, 63), SSD1306_WHITE);
    }

    display.setCursor(0, 55);
    display.print("Hold motor steady...");
    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
//  [C] CALIBRATING screen
//  Shown during CAL_COLLECTING — progress bar + percentage + window count
// ─────────────────────────────────────────────────────────────────────────────
static void drawCalibrating(float progress, VoltageMode vmode) {
    if (!oledReady) return;
    updateDots();

    display.clearDisplay();
    drawHeader("INDUSTRIAL DOCTOR v2");

    // Voltage mode badge (top-right)
    display.setCursor(86, 0);
    display.print(vmode == MODE_6V ? "[ 6V ]" :
                  vmode == MODE_8V ? "[ 8V ]" : "[    ]");

    // Status label
    display.setTextSize(1);
    display.setCursor(0, 14);
    display.print("CALIBRATING");
    display.print(animDots());

    // Progress bar  — 5px padding each side, 8px tall
    const int barX = 5, barY = 26, barW = 118, barH = 8;
    display.drawRect(barX, barY, barW, barH, SSD1306_WHITE);
    const int filled = (int)(progress * (float)(barW - 2));
    if (filled > 0)
        display.fillRect(barX + 1, barY + 1, filled, barH - 2, SSD1306_WHITE);

    // Percentage
    display.setCursor(0, 38);
    display.printf("%.0f%%  Building baseline", progress * 100.0f);

    display.setCursor(0, 50);
    display.print("Keep motor running...");

    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
//  [D] ROLLING BASELINE screen
//  Shown during CAL_SETTLING — EMA is running, environment being absorbed
// ─────────────────────────────────────────────────────────────────────────────
static void drawRollingBaseline(VoltageMode vmode) {
    if (!oledReady) return;
    updateDots();

    display.clearDisplay();
    drawHeader("INDUSTRIAL DOCTOR v2");

    display.setCursor(86, 0);
    display.print(vmode == MODE_6V ? "[ 6V ]" : "[ 8V ]");

    display.setTextSize(1);
    display.setCursor(0, 14);
    display.print("ROLLING BASELINE");
    display.print(animDots());

    // Scrolling sine-wave: simulates the EMA "syncing" to the environment
    static uint8_t waveOffset = 0;
    waveOffset = (waveOffset + 3) % 128;
    for (int x = 0; x < 128; x++) {
        // Two overlapping sine waves with slight phase offset for visual depth
        float angle1 = (x + waveOffset) * 0.12f;
        float angle2 = (x + waveOffset) * 0.07f + 1.2f;
        int   y1     = 38 + (int)(6.0f * sinf(angle1));
        int   y2     = 38 + (int)(3.0f * sinf(angle2));
        display.drawPixel(x, constrain(y1, 0, 63), SSD1306_WHITE);
        display.drawPixel(x, constrain(y2, 0, 63), SSD1306_WHITE);
    }

    display.setCursor(0, 52);
    display.print("Adapting to env...");
    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
//  [E1] STATUS PAGE — the primary output view
//
//  Layout:
//    Row 0–16  Main status (AMBIENT / HEALTHY / FAULT) — inverted if FAULT
//    Row 18    Subclass label ("8V Mode", "Friction / Wear", etc.)
//    Row 27    Confidence percentage
//    Row 37    Divider line
//    Row 40    RMS + ZCR
//    Row 50    PeakHz + Centroid
// ─────────────────────────────────────────────────────────────────────────────
static void drawStatusPage(const MotorFeatures& f, const DiagnosisResult& d) {
    if (!oledReady) return;

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // ── Main status bar ────────────────────────────────────────────────────────
    // FAULT → white background + black text (inverted) for maximum visibility
    // HEALTHY → normal
    // AMBIENT → dimmed (we still show it, just no inversion)

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

    // Reset text colour after possible inversion
    display.setTextColor(SSD1306_WHITE);

    // ── Subclass + confidence ──────────────────────────────────────────────────
    display.setTextSize(1);
    display.setCursor(0, 18);
    display.print(d.label);

    display.setCursor(0, 27);
    display.printf("Conf: %.1f%%", d.confidence * 100.0);

    // ── Divider ────────────────────────────────────────────────────────────────
    display.drawFastHLine(0, 36, 128, SSD1306_WHITE);

    // ── Feature mini-readout ───────────────────────────────────────────────────
    display.setTextSize(1);
    display.setCursor(0, 39);
    display.printf("RMS:%4.0f ZCR:%.3f", f.rms, f.zcr);

    display.setCursor(0, 49);
    display.printf("Hz:%4.0f  Ctr:%4.0f", f.peakFreq, f.spectralCentroid);

    // ── Anomaly score bar (bottom strip) ──────────────────────────────────────
    // Thin bar showing how close the combined score is to the fault threshold
    const float scoreNorm  = (float)constrain(d.anomalyScore / 4.0, 0.0, 1.0);
    const int   scoreW     = (int)(scoreNorm * 128.0f);
    display.drawFastHLine(0, 63, 128, SSD1306_WHITE);
    if (scoreW > 0)
        display.drawFastHLine(0, 62, scoreW, SSD1306_WHITE);

    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
//  [E2] FEATURE PAGES — 3 rotating data pages + status page
// ─────────────────────────────────────────────────────────────────────────────
static void drawFeaturePages(const MotorFeatures& f, const DiagnosisResult& d) {
    if (!oledReady) return;

    // Rotate page every kPageIntervalMs
    const unsigned long now = millis();
    if (now - lastPageSwitchMs >= kPageIntervalMs) {
        pageIndex = (pageIndex + 1) % 4;
        lastPageSwitchMs = now;
    }

    // Page 3 delegates to the status page
    if (pageIndex == 3) {
        drawStatusPage(f, d);
        return;
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // Page header with index
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Features  ");
    display.print(pageIndex + 1);
    display.print("/4");

    if (!f.dataValid) {
        display.setCursor(0, 24);
        display.print("No valid data");
        display.display();
        return;
    }

    switch (pageIndex) {
        case 0:  // RMS + PeakHz
            display.setTextSize(1); display.setCursor(0, 12); display.print("RMS");
            display.setTextSize(2); display.setCursor(0, 20); display.printf("%.1f", f.rms);
            display.setTextSize(1); display.setCursor(0, 38); display.print("PEAK Hz");
            display.setTextSize(2); display.setCursor(0, 46); display.printf("%.0f", f.peakFreq);
            break;

        case 1:  // Kurtosis + Crest Factor
            display.setTextSize(1); display.setCursor(0, 12); display.print("KURT");
            display.setTextSize(2); display.setCursor(0, 20); display.printf("%.2f", f.kurtosis);
            display.setTextSize(1); display.setCursor(0, 38); display.print("CREST");
            display.setTextSize(2); display.setCursor(0, 46); display.printf("%.2f", f.crestFactor);
            break;

        case 2:  // Centroid + ZCR
            display.setTextSize(1); display.setCursor(0, 12); display.print("CENTROID Hz");
            display.setTextSize(2); display.setCursor(0, 20); display.printf("%.0f", f.spectralCentroid);
            display.setTextSize(1); display.setCursor(0, 38); display.print("ZCR");
            display.setTextSize(2); display.setCursor(0, 46); display.printf("%.4f", f.zcr);
            break;
    }

    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
//  SERIAL LOGGING
// ─────────────────────────────────────────────────────────────────────────────
static void serialLog(const MotorFeatures& f, const DiagnosisResult& d) {
    if (!f.dataValid) {
        Serial.println("INVALID");
        return;
    }

    const char* statusStr =
        (d.status == HEALTHY) ? "HEALTHY" :
        (d.status == FAULTY)  ? "FAULTY"  :
        (d.status == AMBIENT) ? "AMBIENT" : "UNKNOWN";

    Serial.printf("%.3f,%.3f,%.3f,%.5f,%.5f,%.3f,%.6f,%s,%s,%.3f,%.3f\n",
                  f.rms,
                  f.rmsStab,
                  f.peakFreq,
                  f.kurtosis,
                  f.crestFactor,
                  f.spectralCentroid,
                  f.zcr,
                  statusStr,
                  d.label,
                  d.confidence,
                  d.anomalyScore);
}

// ─────────────────────────────────────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(2000000);
    delay(150);

    Wire.begin(1, 2);
    oledReady = display.begin(SSD1306_SWITCHCAPVCC, kOledAddress);

    // ── [A] Boot animation — shows logo + SYSTEM ONLINE ───────────────────────
    oled_boot_animation(display, oledReady);

    // ── Initialise feature extractor ──────────────────────────────────────────
    if (!featureExtractor.begin()) {
        Serial.println("ERR: Feature extractor init failed");
    } else {
        Serial.println("# Industrial Doctor v2.0 — ready");
        Serial.println("# rms,rmsStab,peakHz,kurt,crest,centroid,zcr,"
                       "status,label,confidence,anomalyScore");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  LOOP
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    const MotorFeatures features = featureExtractor.getNextFeatures();

    // Feed to engine — advances the calibration state machine
    engine.feedWindow(features);

    const CalibrationState calState = engine.state();
    const VoltageMode      vmode    = engine.voltageMode();

    // ── [B] Warming up ─────────────────────────────────────────────────────────
    if (calState == CAL_WARMUP) {
        drawWarmingUp();
        return;
    }

    // ── [C] Calibrating ───────────────────────────────────────────────────────
    if (calState == CAL_COLLECTING) {
        drawCalibrating(engine.calibProgress(), vmode);
        return;
    }

    // ── Score every window (valid in SETTLING and READY) ──────────────────────
    const ZScoreFrame     zframe = engine.scoreWindow(features);
    const DiagnosisResult diag   = voter.vote(zframe, features, vmode, engine);

    // ── [D] Rolling baseline settling ─────────────────────────────────────────
    if (calState == CAL_SETTLING) {
        drawRollingBaseline(vmode);
        // Log to serial during settling so you can verify the baseline is working
        if (features.dataValid) {
            Serial.printf("# SETTLING: rms=%.1f zcr=%.4f centroid=%.1f score=%.3f\n",
                          features.rms, features.zcr,
                          features.spectralCentroid, zframe.combined);
        }
        return;
    }

    // ── [E] Normal operation ───────────────────────────────────────────────────
    serialLog(features, diag);
    drawFeaturePages(features, diag);

    delay(150);
}