// =============================================================================
//  calibration_menu.cpp  —  Industrial Doctor v2.1  |  Two-Stage Calibration UI
// =============================================================================

#include "calibration_menu.h"
#include <Preferences.h>

// ─────────────────────────────────────────────────────────────────────────────
CalibrationMenu::CalibrationMenu()
    : _display(nullptr), _engine(nullptr),
      _active(false), _state(MenuState::MAIN), _activeCal(CalType::NONE),
      _calSel(0), _ambientDone(false), _healthyDone(false),
      _lastDebounceMs(0), _lastRawBtn(HIGH), _stableBtn(HIGH),
      _lastJoyMs(0), _calDoneAt(0), _lastRenderMs(0)
{}

void CalibrationMenu::begin(Adafruit_SSD1306& display) {
    _display    = &display;
    pinMode(PIN_SW, INPUT_PULLUP);
    analogReadResolution(12);
    _lastRawBtn = digitalRead(PIN_SW);
    _stableBtn  = _lastRawBtn;
    _lastJoyMs  = millis();
}

void CalibrationMenu::setEngine(AnomalyEngine* engine) { _engine = engine; }
void CalibrationMenu::onStartAmbient(std::function<void()> cb) { _cbAmbient = cb; }
void CalibrationMenu::onStartHealthy(std::function<void()> cb) { _cbHealthy = cb; }

void CalibrationMenu::activate() {
    _active     = true;
    _state      = MenuState::MAIN;
    _calSel     = 0;
    _calDoneAt  = 0;
}
void CalibrationMenu::deactivate() { _active = false; }
bool CalibrationMenu::isActive()   const { return _active; }

void CalibrationMenu::loop() {
    if (!_active) return;

    readButtons();

    // Auto-return when calibration reaches 100%
    if (_state == MenuState::RUNNING_CAL && _engine) {
        if (_engine->calibProgress() >= 1.0f) {
            if (_calDoneAt == 0) _calDoneAt = millis();          
            if (millis() - _calDoneAt > 1500) {                  
                notifyCalibrationDone();                          
            }
        } else {
            _calDoneAt = 0;   
        }
    }

    unsigned long now = millis();
    if (now - _lastRenderMs > 50) {   
        render();
        _lastRenderMs = now;
    }
}

void CalibrationMenu::readButtons() {
    unsigned long now = millis();

    int raw = digitalRead(PIN_SW);
    
    // The main loop runs at ~80ms per cycle due to I2S blocking.
    // Trigger on falling edge immediately, use a cooldown to debounce.
    if (_stableBtn == HIGH && raw == LOW) {
        if (now - _lastDebounceMs > 200) {  // 200ms cooldown
            onButtonPress();
            _lastDebounceMs = now;
        }
    }
    _stableBtn = raw;

    if (now - _lastJoyMs > JOY_REPEAT_MS) {
        int vry = analogRead(PIN_VRY);
        if (vry < JOY_LOW) {
            onJoyUp();
            _lastJoyMs = now;
        } else if (vry > JOY_HIGH) {
            onJoyDown();
            _lastJoyMs = now;
        }
    }
}

void CalibrationMenu::onJoyUp() {
    if (_state == MenuState::SELECT_CAL && _calSel > 0) _calSel--;
}

void CalibrationMenu::onJoyDown() {
    if (_state == MenuState::SELECT_CAL && _calSel < 2) _calSel++;
}

void CalibrationMenu::onButtonPress() {
    switch (_state) {
        case MenuState::MAIN:
            _state  = MenuState::SELECT_CAL;
            _calSel = 0;
            break;

        case MenuState::SELECT_CAL:
            if (_calSel == 0 && !_ambientDone) {
                _activeCal = CalType::AMBIENT;
                _state     = MenuState::RUNNING_CAL;
                _calDoneAt = 0;
                if (_cbAmbient) _cbAmbient();
            } else if (_calSel == 1 && !_healthyDone) {
                _activeCal = CalType::HEALTHY;
                _state     = MenuState::RUNNING_CAL;
                _calDoneAt = 0;
                if (_cbHealthy) _cbHealthy();
            } else if (_calSel == 2) {
                if (allCalibrationsDone()) {
                    deactivate();
                } else {
                    _state = MenuState::MAIN;
                }
            }
            break;

        case MenuState::RUNNING_CAL:
            break;
    }
}

void CalibrationMenu::notifyCalibrationDone() {
    String prefix = _voltagePrefix.length() > 0 ? _voltagePrefix : "UNKN";
    
    if (_activeCal == CalType::AMBIENT) {
        _ambientDone = true;
        if (_engine) saveProfile(_engine->getAmbientProfile(), (prefix + "_AMBIENT").c_str());
    }
    if (_activeCal == CalType::HEALTHY) {
        _healthyDone = true;
        if (_engine) saveProfile(_engine->getHealthyProfile(), (prefix + "_HEALTHY").c_str());
    }
    _activeCal = CalType::NONE;
    _calDoneAt = 0;
    
    if (allCalibrationsDone()) {
        deactivate();
    } else {
        _state = MenuState::SELECT_CAL;
    }
}

void CalibrationMenu::drawBold(const char* text, int x, int y) {
    _display->setCursor(x,     y); _display->print(text);
    _display->setCursor(x + 1, y); _display->print(text);
}

void CalibrationMenu::drawCenteredBold(const char* text, int y) {
    int w = strlen(text) * 6;
    int x = max(0, (128 - w) / 2);
    drawBold(text, x, y);
}

void CalibrationMenu::drawHeader(const char* title) {
    _display->fillRect(0, 0, 128, 11, SSD1306_WHITE);
    _display->setTextSize(1);
    _display->setTextColor(SSD1306_BLACK);
    int w = strlen(title) * 6;
    int x = max(0, (128 - w) / 2);
    _display->setCursor(x,     2); _display->print(title);
    _display->setCursor(x + 1, 2); _display->print(title);
    _display->setTextColor(SSD1306_WHITE);
}

void CalibrationMenu::drawMenuItem(const char* label, int y, bool selected, bool done) {
    _display->setTextSize(1);
    if (selected) {
        _display->fillRect(0, y - 1, 128, 11, SSD1306_WHITE);
        _display->setTextColor(SSD1306_BLACK);
    } else {
        _display->setTextColor(SSD1306_WHITE);
    }

    _display->setCursor(2, y);
    _display->print(selected ? '>' : ' ');

    _display->setCursor(10,     y); _display->print(label);
    _display->setCursor(10 + 1, y); _display->print(label);

    if (done) {
        _display->setCursor(100, y);
        _display->print("[+]");
    }
    _display->setTextColor(SSD1306_WHITE);
}

void CalibrationMenu::drawProgressBar(int x, int y, int w, int h, float progress) {
    float p    = constrain(progress, 0.0f, 1.0f);
    int   fill = (int)((w - 2) * p);
    _display->drawRect(x, y, w, h, SSD1306_WHITE);
    if (fill > 0) {
        _display->fillRect(x + 1, y + 1, fill, h - 2, SSD1306_WHITE);
    }
}

void CalibrationMenu::renderMain() {
    _display->clearDisplay();
    drawHeader("INDUSTRIAL DOCTOR");

    _display->drawRect(3,  13, 122, 34, SSD1306_WHITE);
    _display->drawRect(5,  15, 118, 30, SSD1306_WHITE);

    _display->setTextSize(1);
    _display->setTextColor(SSD1306_WHITE);
    drawCenteredBold("START", 20);
    drawCenteredBold("CALIBRATION", 31);

    if ((millis() / 550) % 2) {
        drawCenteredBold(">> CLICK TO BEGIN <<", 52);
    }
}

void CalibrationMenu::renderSelectCal() {
    _display->clearDisplay();
    drawHeader("SELECT CALIBRATION");

    drawMenuItem("CALIBRATE AMBIENT", 14, _calSel == 0, _ambientDone);
    drawMenuItem("CALIBRATE HEALTHY", 26, _calSel == 1, _healthyDone);
    
    if (allCalibrationsDone()) {
        drawMenuItem("START DIAGNOSIS >", 38, _calSel == 2, false);
    } else {
        drawMenuItem("< BACK", 38, _calSel == 2, false);
    }

    _display->setTextSize(1);
    _display->setTextColor(SSD1306_WHITE);
    drawCenteredBold("JOY:NAV  BTN:SELECT", 55);
}

void CalibrationMenu::renderRunning() {
    _display->clearDisplay();

    const char* hdr = (_activeCal == CalType::AMBIENT) ? "AMBIENT CAL" : "HEALTHY CAL";
    drawHeader(hdr);

    float progress = _engine ? _engine->calibProgress() : 0.0f;
    drawProgressBar(4, 14, 120, 14, progress);

    char pctBuf[8];
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", (int)(progress * 100.0f));
    _display->setTextSize(2);
    _display->setTextColor(SSD1306_WHITE);
    int tw = strlen(pctBuf) * 12;
    int tx = (128 - tw) / 2;
    _display->setCursor(tx,     31); _display->print(pctBuf);
    _display->setCursor(tx + 1, 31); _display->print(pctBuf);

    _display->setTextSize(1);
    _display->setTextColor(SSD1306_WHITE);

    const char* stStr = "";
    if (_engine) {
        CalibrationState s = _engine->getState();
        if      (s == CAL_AMBIENT_SETTLING)   stStr = "Collecting...";
        else if (s == CAL_AMBIENT_READY)      stStr = "Ambient done!";
        else if (s == CAL_HEALTHY_SETTLING)   stStr = "Collecting...";
        else if (s == CAL_HEALTHY_READY)      stStr = "Complete!";
    }
    if (_calDoneAt != 0) stStr = "COMPLETE!";

    int sw = strlen(stStr) * 6;
    _display->setCursor((128 - sw) / 2, 54);
    _display->print(stStr);
}

void CalibrationMenu::render() {
    if (!_display) return;
    switch (_state) {
        case MenuState::MAIN:        renderMain();      break;
        case MenuState::SELECT_CAL:  renderSelectCal(); break;
        case MenuState::RUNNING_CAL: renderRunning();   break;
    }
    _display->display();
}

void CalibrationMenu::saveProfile(const Profile& profile, const char* name) {
    Preferences prefs;
    prefs.begin("inddoctor", false);
    String b = String(name) + ":";

    const char* featureNames[Profile::N_FEATURES] = {
        "rms", "rmsStab", "peak", "kurt", "crest", "cent", "zcr", "env"
    };

    for (int i = 0; i < Profile::N_FEATURES; i++) {
        prefs.putFloat((b + featureNames[i] + "_mean").c_str(), profile.means[i]);
        prefs.putFloat((b + featureNames[i] + "_std").c_str(),  profile.stds[i]);
    }
    prefs.end();
}

bool CalibrationMenu::loadProfile(Profile& profile, const char* name) {
    Preferences prefs;
    if (!prefs.begin("inddoctor", true)) return false;
    String b = String(name) + ":";

    const char* featureNames[Profile::N_FEATURES] = {
        "rms", "rmsStab", "peak", "kurt", "crest", "cent", "zcr", "env"
    };

    bool exists = true;
    for (int i = 0; i < Profile::N_FEATURES; i++) {
        String keyMean = b + featureNames[i] + "_mean";
        String keyStd = b + featureNames[i] + "_std";
        if (!prefs.isKey(keyMean.c_str()) || !prefs.isKey(keyStd.c_str())) {
            exists = false;
            break;
        }
        profile.means[i] = prefs.getFloat(keyMean.c_str(), profile.means[i]);
        profile.stds[i]  = prefs.getFloat(keyStd.c_str(), profile.stds[i]);

        // Sanity check to detect precision-corrupted or NaN profiles
        if (isnan(profile.means[i]) || isnan(profile.stds[i]) || profile.stds[i] < 0.0f || profile.stds[i] > 1e8f) {
            exists = false;
            break;
        }
    }
    prefs.end();
    return exists;
}

bool CalibrationMenu::ambientDone()         const { return _ambientDone; }
bool CalibrationMenu::healthyDone()         const { return _healthyDone; }
bool CalibrationMenu::allCalibrationsDone() const { return _ambientDone && _healthyDone; }