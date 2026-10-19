#pragma once
// ============================================================================
//  INDUSTRIAL DOCTOR v2.1 — FREE RTOS SYSTEM ARCHITECTURE
//  inspection_menu.h
// ============================================================================
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "diagnosis_voter.h"

class InspectionMenu {
public:
    InspectionMenu(Adafruit_SSD1306& displayRef);
    void begin();
    
    // Joystick Navigation inputs
    void handleJoystick(int joystickVal);
    
    // Renders the specific active dashboard view
    void render(const float f[8], const DiagnosisResult& diag);

private:
    enum DashMode {
        DASH_MAIN_OUTPUT = 0, // Direct diagnosis & confidence meters
        DASH_LIVE_VALUES = 1  // Live calculated features array lists
    };

    Adafruit_SSD1306& _display;
    DashMode _currentDash;
    int _scrollOffset;        // For scrolling through features on Dash 1
    uint32_t _lastNavTime;

    void drawHeader(const char* title);
    void drawProgressBar(int x, int y, int width, int height, float percentage);
};