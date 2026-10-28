#pragma once
// =============================================================================
//  calibration_menu.h  —  Industrial Doctor  |  OLED Calibration UI
//  Rewritten: fixed button debounce bug, full state-machine menu.
// =============================================================================

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <functional>
#include "anomaly_engine.h"

// ─── Menu states ──────────────────────────────────────────────────────────────
enum class MenuState : uint8_t {
    MAIN,        // Splash: "START CALIBRATION"
    SELECT_CAL,  // Pick: Ambient / Healthy / Back
    RUNNING_CAL  // Progress bar for active calibration
};

enum class CalType : uint8_t {
    NONE,
    AMBIENT,
    HEALTHY
};

// =============================================================================
class CalibrationMenu {
public:
    CalibrationMenu();

    // ── Setup ─────────────────────────────────────────────────────────────────
    // Call during Arduino setup(); pass in an already-initialised display.
    void begin(Adafruit_SSD1306& display);

    // Provide engine pointer so progress/state can be read (optional but needed
    // for the progress screen).
    void setEngine(AnomalyEngine* engine);

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    // Call activate() to show the menu; call loop() every Arduino loop().
    void activate();
    void deactivate();
    bool isActive() const;
    void loop();

    // ── Callbacks ─────────────────────────────────────────────────────────────
    // Fired when the user confirms a calibration option.
    void onStartAmbient(std::function<void()> cb);
    void onStartHealthy(std::function<void()> cb);

    // ── Completion hook ───────────────────────────────────────────────────────
    // Call this from your main code when the engine signals calibration done.
    // Alternatively, the menu auto-detects engine->calibProgress() >= 1.0f.
    void notifyCalibrationDone();

    bool ambientDone() const;
    bool healthyDone() const;
    bool allCalibrationsDone() const;
    
    // ── Persistence ───────────────────────────────────────────────────────────
    // Save a profile to persistent storage (Preferences)
    void saveProfile(const Profile& profile, const char* name);

private:
    // Input
    void readButtons();
    void onJoyUp();
    void onJoyDown();
    void onButtonPress();

    // Renderers (one per MenuState)
    void render();
    void renderMain();
    void renderSelectCal();
    void renderRunning();

    // Draw helpers
    void drawHeader(const char* title);
    void drawMenuItem(const char* label, int y, bool selected, bool done);
    void drawProgressBar(int x, int y, int w, int h, float progress);
    void drawBold(const char* text, int x, int y);
    void drawCenteredBold(const char* text, int y);

    // ── Hardware ──────────────────────────────────────────────────────────────
    static constexpr int PIN_SW   = 15;   // Joystick switch (active LOW → GND)
    static constexpr int PIN_VRY  = 5;    // Vertical axis (12-bit ADC)
    static constexpr int JOY_LOW  = 1400; // Joystick pushed "up"
    static constexpr int JOY_HIGH = 2600; // Joystick pushed "down"

    static constexpr unsigned long DEBOUNCE_MS   = 30;   // button debounce
    static constexpr unsigned long JOY_REPEAT_MS = 220;  // nav repeat rate

    // ── Peripherals ───────────────────────────────────────────────────────────
    Adafruit_SSD1306* _display;
    AnomalyEngine*    _engine;

    // ── State ─────────────────────────────────────────────────────────────────
    bool      _active;
    MenuState _state;
    CalType   _activeCal;
    int       _calSel;       // 0 = Ambient  |  1 = Healthy  |  2 = Back

    bool      _ambientDone;  // shows [+] badge when true
    bool      _healthyDone;

    // ── Button debounce — FIXED ───────────────────────────────────────────────
    // KEY FIX: _lastRawBtn  = most-recent digitalRead (updated immediately)
    //          _stableBtn   = confirmed stable state (updated only after DEBOUNCE_MS)
    // This separation ensures the falling-edge press event is never missed.
    unsigned long _lastDebounceMs;
    int           _lastRawBtn;
    int           _stableBtn;

    // ── Joystick rate limiter ─────────────────────────────────────────────────
    unsigned long _lastJoyMs;

    // ── Auto-return after calibration finishes ────────────────────────────────
    unsigned long _calDoneAt;  // non-zero while we are in the 1.5-s hold window

    // ── Callbacks ─────────────────────────────────────────────────────────────
    std::function<void()> _cbAmbient;
    std::function<void()> _cbHealthy;

    // ── Render throttle ───────────────────────────────────────────────────────
    unsigned long _lastRenderMs;
};