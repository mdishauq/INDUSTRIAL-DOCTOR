// ============================================================================
//  INDUSTRIAL DOCTOR v2.1 — FREE RTOS SYSTEM ARCHITECTURE
//  inspection_menu.cpp
// ============================================================================
#include "inspection_menu.h"

InspectionMenu::InspectionMenu(Adafruit_SSD1306& displayRef) 
    : _display(displayRef), _currentDash(DASH_MAIN_OUTPUT), _scrollOffset(0), _lastNavTime(0) {}

void InspectionMenu::begin() {
    _display.setTextWrap(false);
}

void InspectionMenu::handleJoystick(int joystickVal) {
    // Debounce navigation inputs to prevent page stuttering
    if (millis() - _lastNavTime < 250) return;

    if (joystickVal > 3000) { // Joystick UP
        if (_currentDash == DASH_LIVE_VALUES && _scrollOffset > 0) {
            _scrollOffset--;
        } else if (_currentDash == DASH_LIVE_VALUES && _scrollOffset == 0) {
            _currentDash = DASH_MAIN_OUTPUT;
            _lastNavTime = millis();
        }
    } 
    else if (joystickVal < 1000) { // Joystick DOWN
        if (_currentDash == DASH_MAIN_OUTPUT) {
            _currentDash = DASH_LIVE_VALUES;
            _scrollOffset = 0;
            _lastNavTime = millis();
        } else if (_currentDash == DASH_LIVE_VALUES && _scrollOffset < 4) {
            _scrollOffset++; // Scroll down through features list
        }
    }
}

void InspectionMenu::drawHeader(const char* title) {
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.print(title);
    _display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
}

void InspectionMenu::drawProgressBar(int x, int y, int width, int height, float percentage) {
    _display.drawRect(x, y, width, height, SSD1306_WHITE);
    int fillWidth = (int)((width - 4) * constrain(percentage, 0.0f, 1.0f));
    if (fillWidth > 0) {
        _display.fillRect(x + 2, y + 2, fillWidth, height - 4, SSD1306_WHITE);
    }
}

void InspectionMenu::render(const float f[8], const DiagnosisResult& diag) {
    _display.clearDisplay();

    if (_currentDash == DASH_MAIN_OUTPUT) {
        drawHeader("DIAGNOSIS DASHBOARD");
        
        // Large status output string display
        _display.setTextSize(2);
        _display.setCursor(0, 16);
        _display.print(diag.label);
        
        // Clean confidence bar drawing layout
        _display.setTextSize(1);
        _display.setCursor(0, 36);
        _display.printf("Confidence: %d%%", (int)(diag.confidence * 100.0f));
        drawProgressBar(0, 48, 128, 10, diag.confidence);
    } 
    else if (_currentDash == DASH_LIVE_VALUES) {
        drawHeader("LIVE REAL-TIME FEATURES");
        
        const char* featureNames[8] = {
            "RMS Energy", "RMS Stability", "Peak Freq", "Kurtosis",
            "Crest Factor", "Spec Centroid", "ZCR Chaos", "Envelope RMS"
        };
        const char* units[8] = {"", "", "Hz", "", "", "Hz", "", ""};

        // Render 4 lines of features based on the current scroll position
        _display.setTextSize(1);
        int startRow = _scrollOffset;
        for (int i = 0; i < 4; i++) {
            int idx = startRow + i;
            if (idx >= 8) break;
            
            _display.setCursor(0, 14 + (i * 10));
            _display.printf("%s: %.2f %s", featureNames[idx], f[idx], units[idx]);
        }

        // Overlay the active evaluation state onto the bottom row of the screen
        _display.drawFastHLine(0, 54, 128, SSD1306_WHITE);
        _display.setCursor(0, 56);
        _display.printf("State -> %s", diag.label);
    }

    _display.display();
}