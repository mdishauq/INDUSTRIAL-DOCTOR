// ============================================================================
//  INDUSTRIAL DOCTOR v2.1 — FREE RTOS SYSTEM ARCHITECTURE WITH BOOT ANIMATION
//  main.cpp
// ============================================================================
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#include "feature_extraction.h"
#include "anomaly_engine.h"
#include "inspection_menu.h"
#include "boot_animation.h" // Your high-fidelity logo reveal asset header

// ── Hardware IO Allocation map ───────────────────────────────────────────────
#define JOY_Y_PIN 5  // Restored back to your working hardware pin layout
#define SW_PIN 15    // User button (active LOW)

// ── FreeRTOS Synchronization Handles ────────────────────────────────────────
QueueHandle_t      xAudioQueue  = NULL;
SemaphoreHandle_t  xDataMutex   = NULL;

// ── Shared Memory Framework ─────────────────────────────────────────────────
struct SharedDiagnosticData {
    float           features[8];
    DiagnosisResult diagnosis;
};
SharedDiagnosticData globalSystemData;

// ── Global Object Instantiations ─────────────────────────────────────────────
Adafruit_SSD1306  display(128, 64, &Wire, -1);
FeatureExtractor  extractor;
DiagnosisVoter    voter;
AnomalyEngine     anomaly;
InspectionMenu    menu(display);

static bool i2cDevicePresent(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

static bool isButtonPressed() {
    return digitalRead(SW_PIN) == LOW;
}

static void waitForButtonRelease() {
    while (isButtonPressed()) {
        delay(10);
    }
}

static int readJoystickNav() {
    static uint32_t lastNavTime = 0;
    if (millis() - lastNavTime < 200) return 0;

    int joy = analogRead(JOY_Y_PIN);
    if (joy > 3000) {
        lastNavTime = millis();
        return -1;
    }
    if (joy < 1000) {
        lastNavTime = millis();
        return 1;
    }
    return 0;
}

static void drawHeader(Adafruit_SSD1306& d, const char* title) {
    d.clearDisplay();
    d.setTextSize(1);
    d.setTextColor(SSD1306_WHITE);
    d.setCursor(0, 0);
    d.print(title);
    d.drawFastHLine(0, 10, 128, SSD1306_WHITE);
}

static void drawMessage(Adafruit_SSD1306& d, const char* line1, const char* line2) {
    drawHeader(d, "CALIBRATION");
    d.setCursor(0, 18);
    d.print(line1);
    if (line2 != nullptr) {
        d.setCursor(0, 30);
        d.print(line2);
    }
    d.display();
}

static void drawProgress(Adafruit_SSD1306& d, const char* title, int current, int total) {
    drawHeader(d, title);
    d.setCursor(0, 18);
    d.printf("%d / %d", current, total);
    d.drawRect(0, 40, 128, 12, SSD1306_WHITE);

    int fillWidth = (int)((124.0f * current) / (float)total);
    if (fillWidth > 0) {
        d.fillRect(2, 42, fillWidth, 8, SSD1306_WHITE);
    }
    d.display();
}

static void drawCalibrationMenu(Adafruit_SSD1306& d, int selection, bool ambientDone, bool healthyDone) {
    drawHeader(d, "CALIBRATION MENU");

    d.setCursor(0, 16);
    d.print(selection == 0 ? "> " : "  ");
    d.print("Ambient Cal");
    if (ambientDone) d.print(" [OK]");

    d.setCursor(0, 28);
    d.print(selection == 1 ? "> " : "  ");
    d.print("Healthy Cal");
    if (healthyDone) d.print(" [OK]");

    d.setCursor(0, 40);
    d.print(selection == 2 ? "> " : "  ");
    d.print("Start Monitoring");
    if (!(ambientDone && healthyDone)) d.print(" [LOCK]");

    d.setCursor(0, 56);
    d.print("Press button");
    d.display();
}

static bool runAmbientCalibration(Adafruit_SSD1306& d, FeatureExtractor& extractor, DiagnosisVoter& voter, float& ambientGuardrail) {
    drawMessage(d, "Warming up I2S...", nullptr);

    int warmupCount = 0;
    while (warmupCount < 15) {
        MotorFeatures warmUpFrame = extractor.getNextFeatures();
        if (warmUpFrame.dataValid) {
            warmupCount++;
            delay(30);
        }
    }

    const int ambientSamples = 20;
    float totalAmbientRms = 0.0f;
    int captured = 0;

    while (captured < ambientSamples) {
        MotorFeatures calibFrame = extractor.getNextFeatures();
        if (calibFrame.dataValid) {
            totalAmbientRms += calibFrame.rms;
            captured++;
            drawProgress(d, "AMBIENT CAL", captured, ambientSamples);
            delay(50);
        }
    }

    float averageAmbientRms = totalAmbientRms / ambientSamples;
    if (averageAmbientRms < 1.0f || isnan(averageAmbientRms)) {
        averageAmbientRms = 35.0f;
    }

    voter.setAmbientThreshold(averageAmbientRms);
    ambientGuardrail = constrain(averageAmbientRms * 1.30f, 20.0f, 120.0f);

    drawMessage(d, "Ambient complete", nullptr);
    delay(800);
    return true;
}

static bool runHealthyCalibration(Adafruit_SSD1306& d, FeatureExtractor& extractor, AnomalyEngine& anomaly, float ambientGuardrail) {
    if (ambientGuardrail <= 0.0f) return false;

    drawMessage(d, "Start motor now", "Waiting for spin...");
    anomaly.beginCalibration();

    const int healthySamples = 150;
    const uint32_t healthyTimeoutMs = 90000;
    const float detectRmsMin = ambientGuardrail * 1.10f;
    const float captureRmsMin = ambientGuardrail * 1.05f;
    const int detectNeeded = 5;

    int captured = 0;
    uint32_t startTime = millis();
    int detectCount = 0;
    bool motorDetected = false;

    while (captured < healthySamples && (millis() - startTime) < healthyTimeoutMs) {
        MotorFeatures calibFrame = extractor.getNextFeatures();
        if (!calibFrame.dataValid) continue;

        float f[8];
        calibFrame.toArray(f);
        if (!motorDetected) {
            if (f[0] >= detectRmsMin) {
                detectCount++;
            } else {
                detectCount = 0;
            }

            if (detectCount >= detectNeeded) {
                motorDetected = true;
                drawMessage(d, "Collecting...", nullptr);
            }
            delay(30);
            continue;
        }

        if (f[0] < captureRmsMin) {
            delay(20);
            continue;
        }

        anomaly.addSample(f);
        captured++;
        drawProgress(d, "HEALTHY CAL", captured, healthySamples);
        delay(30);
    }

    bool ok = (captured >= healthySamples) && anomaly.finalizeCalibration();
    if (ok) {
        drawMessage(d, "Healthy complete", nullptr);
    } else {
        anomaly.beginCalibration();
        drawMessage(d, "Healthy failed", "Try again");
    }
    delay(800);
    return ok;
}

static void runCalibrationWizard(Adafruit_SSD1306& d, FeatureExtractor& extractor, DiagnosisVoter& voter, AnomalyEngine& anomaly) {
    bool ambientDone = false;
    bool healthyDone = false;
    float ambientGuardrail = 0.0f;

    drawMessage(d, "Press button", "to start");
    while (!isButtonPressed()) {
        delay(20);
    }
    waitForButtonRelease();

    int selection = 0;
    bool startMonitoring = false;
    while (!startMonitoring) {
        drawCalibrationMenu(d, selection, ambientDone, healthyDone);

        int nav = readJoystickNav();
        if (nav != 0) {
            selection += nav;
            if (selection < 0) selection = 2;
            if (selection > 2) selection = 0;
        }

        if (isButtonPressed()) {
            waitForButtonRelease();

            if (selection == 0) {
                ambientDone = runAmbientCalibration(d, extractor, voter, ambientGuardrail);
            } else if (selection == 1) {
                if (!ambientDone) {
                    drawMessage(d, "Run ambient first", nullptr);
                    delay(800);
                } else {
                    healthyDone = runHealthyCalibration(d, extractor, anomaly, ambientGuardrail);
                }
            } else {
                if (!(ambientDone && healthyDone)) {
                    drawMessage(d, "Finish both", "calibrations");
                    delay(800);
                } else {
                    startMonitoring = true;
                }
            }
        }

        delay(20);
    }

    drawMessage(d, "Calibration done", "Starting monitor...");
    delay(800);
}

// ── Task Declarations ────────────────────────────────────────────────────────
void TaskAcquire(void *pvParameters);
void TaskProcess(void *pvParameters);
void TaskUI(void *pvParameters);

void setup() {
    Serial.begin(115200);
    Wire.begin(1, 2);
    Wire.setClock(400000);
    delay(50);

    uint8_t oledAddr = 0x3C;
    bool oledFound = i2cDevicePresent(0x3C);
    if (!oledFound && i2cDevicePresent(0x3D)) {
        oledAddr = 0x3D;
        oledFound = true;
    }
    if (!oledFound) {
        Serial.println("OLED not found on I2C. Check SDA/SCL pins and address.");
        while (true) {
            delay(1000);
        }
    }

    // 1. Initialize Display Hardware
    if(!display.begin(SSD1306_SWITCHCAPVCC, oledAddr, false, false)) {
        Serial.println(F("OLED Allocation Failed"));
        while(true);
    }
    
    // 2. Play Industrial Doctor v2.0 Cinematic Boot Sequence
    // This blocks setup() cleanly for ~4 seconds, running through all 8 phases
    Serial.println("Executing Boot Asset Sequence...");
    oled_boot_animation(display, true); 
    Serial.println("System Core Fully Online.");

    // 3. Post-Boot Subsystem Initializations
    extractor.begin();
    menu.begin();

    pinMode(SW_PIN, INPUT_PULLUP);
    runCalibrationWizard(display, extractor, voter, anomaly);

    // 4. Initialize Thread Synchronization Tools
    xAudioQueue = xQueueCreate(3, sizeof(MotorFeatures));
    xDataMutex  = xSemaphoreCreateMutex();

    if (xAudioQueue == NULL || xDataMutex == NULL) {
        Serial.println("FreeRTOS Resource Generation Aborted");
        while(true);
    }

    // 5. Spawn FreeRTOS Pipeline Tasks across cores
    xTaskCreatePinnedToCore(
        TaskAcquire,     "AcquireTask", 
        4096,            NULL, 
        3,               NULL, 
        0                // Core 0 handles hardware I2S data ingestion
    );

    xTaskCreatePinnedToCore(
        TaskProcess,     "ProcessTask", 
        8192,            NULL, 
        2,               NULL, 
        1                // Core 1 handles mathematical processing
    );

    xTaskCreatePinnedToCore(
        TaskUI,          "UITask", 
        4096,            NULL, 
        1,               NULL, 
        1                // Core 1 handles drawing routines and joystick checking
    );
}

void loop() {
    // Left completely blank. FreeRTOS takes full control of scheduler loops.
    vTaskDelete(NULL);
}

// ── FreeRTOS Task Implementations ───────────────────────────────────────────

void TaskAcquire(void *pvParameters) {
    (void) pvParameters;
    while(true) {
        MotorFeatures rawBufferFrame = extractor.getNextFeatures();
        if (rawBufferFrame.dataValid) {
            xQueueSend(xAudioQueue, &rawBufferFrame, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void TaskProcess(void *pvParameters) {
    (void) pvParameters;
    MotorFeatures processingFrame;
    float localFeatures[8];

    while(true) {
        if (xQueueReceive(xAudioQueue, &processingFrame, portMAX_DELAY) == pdTRUE) {
            processingFrame.toArray(localFeatures);
            float anomalyScore = anomaly.score(localFeatures);
            DiagnosisResult computedResult = voter.voteEnsemble(localFeatures, anomalyScore);

            if (xSemaphoreTake(xDataMutex, portMAX_DELAY) == pdTRUE) {
                memcpy(globalSystemData.features, localFeatures, sizeof(localFeatures));
                globalSystemData.diagnosis = computedResult;
                xSemaphoreGive(xDataMutex);
            }
        }
    }
}

void TaskUI(void *pvParameters) {
    (void) pvParameters;
    float uiFeatures[8];
    DiagnosisResult uiDiag;

    while(true) {
        // Read your physical analog joystick on GPIO 5
        int joyY = analogRead(JOY_Y_PIN);
        menu.handleJoystick(joyY);

        // Safely extract the background calculations data block
        if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            memcpy(uiFeatures, globalSystemData.features, sizeof(uiFeatures));
            uiDiag = globalSystemData.diagnosis;
            xSemaphoreGive(xDataMutex);
        }

        // Draw the real-time selected joystick menu screen
        menu.render(uiFeatures, uiDiag);

        // Limit UI thread loop to 20Hz refresh interval rate
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}