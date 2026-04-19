#pragma once

// ============================================================
// ps4_handler.h
// Wraps Bluepad32 library. Exposes clean input struct and
// an on-demand pairing trigger for the web dashboard.
// ============================================================

#include <Bluepad32.h>
#include "config.h"

struct PS4Input {
    int8_t  lx = 0, ly = 0;       // Left stick  -128..127
    int8_t  rx = 0, ry = 0;       // Right stick -128..127
    uint8_t l2 = 0, r2 = 0;       // Analog triggers 0..255
    uint8_t dpad = 0;             // D-Pad bitmask (0x01=Up, 0x02=Down, etc)
    bool    cross    = false;
    bool    circle   = false;
    bool    square   = false;
    bool    triangle = false;
    bool    l1 = false, r1 = false;
    bool    options  = false;
    bool    connected = false;
};

class PS4Handler {
public:
    void begin() {
        Serial.printf("[BP32] Initialising Bluepad32. Free heap: %u\n", ESP.getFreeHeap());

        BP32.setup(&PS4Handler::onConnectedController,
                   &PS4Handler::onDisconnectedController);

        // ── Do NOT forget Bluetooth keys on boot ─────────────────

        Serial.println("[BP32] Ready. Controller will reconnect automatically on power-on.");
        Serial.println("[BP32] To pair a NEW controller: use the dashboard button.");
    }

    // ----------------------------------------------------------
    // triggerPairing()
    // Called by the web server when the user presses the
    // "Pair New Controller" button on the dashboard.
    // ----------------------------------------------------------
    void triggerPairing() {
        Serial.println("[BP32] Pairing mode triggered — clearing saved keys...");
        BP32.forgetBluetoothKeys();
        _pairingMode      = true;
        _pairingStartedMs = millis();
        Serial.println("[BP32] Hold Share + PS on the controller NOW (60-second window).");
    }

    // Returns true while the 60-second pairing window is active.
    bool isPairingMode() const {
        if (!_pairingMode) return false;
        if (millis() - _pairingStartedMs > 60000UL) {
            _pairingMode = false;
            Serial.println("[BP32] Pairing window expired.");
        }
        return _pairingMode;
    }

    // ----------------------------------------------------------
    // read()  — must be called every control loop tick
    // ----------------------------------------------------------
    PS4Input read() {
        BP32.update();  // pump the Bluetooth stack

        // Auto-exit pairing mode once a controller actually connects
        if (_pairingMode && _controller && _controller->isConnected()) {
            Serial.println("[BP32] New controller paired successfully!");
            _pairingMode = false;
        }

        PS4Input in;
        if (!_controller || !_controller->isConnected()) {
            return in;   // connected defaults to false
        }

        in.connected = true;

        // Bluepad32 axes: -512..511  →  scale to -128..127
        in.lx = (int8_t)(_controller->axisX()  / 4);
        in.ly = (int8_t)(-_controller->axisY() / 4);   // invert Y for RC logic
        in.rx = (int8_t)(_controller->axisRX() / 4);
        in.ry = (int8_t)(-_controller->axisRY() / 4);

        // Triggers: 0..1023  →  0..255
        in.l2 = (uint8_t)(_controller->brake()    / 4);
        in.r2 = (uint8_t)(_controller->throttle() / 4);

        in.dpad = _controller->dpad();

        // Face buttons (Cross=A, Circle=B, Square=X, Triangle=Y)
        in.cross    = _controller->a();
        in.circle   = _controller->b();
        in.square   = _controller->x();
        in.triangle = _controller->y();

        in.l1 = _controller->l1();
        in.r1 = _controller->r1();

        // Options / Start / Home  →  generic toggle flag
        in.options = (_controller->miscButtons() > 0) || _controller->thumbR();

        return in;
    }

    bool isConnected() const {
        return _controller && _controller->isConnected();
    }

    // ── Sensory Telemetry (Haptics & LEDs) ───────────────────
    
    void updateHaptics(const TelemetryData& snap) {
        if (!_controller || !_controller->isConnected()) return;

        // 1. Light Bar Battery Indicator
        // Green (12.6V) -> Yellow -> Red (10.5V)
        uint8_t r = 0, g = 0, b = 0;
        if (snap.battery_voltage > 11.5f) {
            g = 255; // Stable Green
        } else if (snap.battery_voltage > 10.8f) {
            r = 255; g = 255; // Warning Yellow
        } else {
            r = 255; // Critical Red
            // Flash red if critically low (< 9.6V)
            if (snap.battery_voltage < 9.8f && (millis() % 500 < 250)) r = 0;
        }
        _controller->setColorLED(r, g, b);

        // 2. Impact Detection
        // Rumble if total acceleration exceeds 3.5G (standard gravity is 1.0)
        float totalAccel = sqrt(snap.ax*snap.ax + snap.ay*snap.ay + snap.az*snap.az);
        if (totalAccel > 3.5f) {
            _controller->playDualRumble(0, 300, 0, 255); // Heavy thud
        }
    }

    void setRumble(uint8_t small, uint8_t large, uint8_t durationMs = 250) {
        if (_controller && _controller->isConnected()) {
            _controller->playDualRumble(0, durationMs, small, large);
        }
    }

private:
    static ControllerPtr  _controller;
    static int           _failCount;       // Counter for GAP connection failures
    mutable bool          _pairingMode      = false;
    mutable unsigned long _pairingStartedMs = 0;

    static void onConnectedController(ControllerPtr ctl) {
        if (_controller == nullptr) {
            Serial.printf("[BP32] Controller connected: %s\n",
                          ctl->getModelName().c_str());
            _controller = ctl;
            _failCount  = 0; // Reset counter on success
        } else {
            Serial.println("[BP32] Rejected extra controller (single-pad mode).");
            ctl->disconnect();
        }
    }

    static void onDisconnectedController(ControllerPtr ctl) {
        if (_controller == ctl) {
            Serial.println("[BP32] Controller disconnected.");
            _controller = nullptr;
        } else {
            // If we get a disconnect event for a device that never fully "connected"
            // it's likely part of the GAP connection failure loop.
            _failCount++;
            if (_failCount >= 5) {
                Serial.println("[BP32] Loop detected (5+ fails) — Force clearing keys!");
                BP32.forgetBluetoothKeys();
                _failCount = 0;
            }
        }
    }
};
