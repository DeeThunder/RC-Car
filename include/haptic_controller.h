#pragma once

// ╔══════════════════════════════════════════════════════════════╗
// ║              haptic_controller.h                             ║
// ║  Translates physical events (impacts, speed) into PS4       ║
// ║  rumble commands. Reads IMU data from TelemetryStore —       ║
// ║  NO direct sensor access, reuses MPU6050 data already        ║
// ║  populated by sensorTask at 50Hz.                            ║
// ║                                                              ║
// ║  Non-blocking: rumble auto-stops via millis() timer.         ║
// ╚══════════════════════════════════════════════════════════════╝

#include <PS4Controller.h>
#include <cmath>
#include "config.h"

enum class RumbleEvent {
    NONE,
    LIGHT_IMPACT,
    MEDIUM_IMPACT,
    HEAVY_IMPACT,
    SPEED_BUZZ
};

class HapticController {
public:
    // Call every controlTask loop iteration (10ms / 100Hz)
    // ax, ay, az in m/s² — taken from TelemetryStore snapshot
    // speed_kmh    — taken from TelemetryStore GPS reading
    void update(float ax, float ay, float az, float speed_kmh) {
        if (!PS4.isConnected()) return;

        uint32_t now = millis();

        // ── Impact detection from acceleration magnitude ──────
        // Compute net magnitude then subtract 1g (9.81 m/s²) to get
        // the deviation caused by a real physical impact event.
        // At rest on a flat surface: mag ≈ 9.81, delta ≈ 0
        float mag   = sqrtf(ax*ax + ay*ay + az*az);
        float delta = fabsf(mag - 9.81f);

        RumbleEvent evt = RumbleEvent::NONE;
        if      (delta >= HapticConfig::HEAVY_IMPACT_THRESHOLD)  evt = RumbleEvent::HEAVY_IMPACT;
        else if (delta >= HapticConfig::MEDIUM_IMPACT_THRESHOLD) evt = RumbleEvent::MEDIUM_IMPACT;
        else if (delta >= HapticConfig::LIGHT_IMPACT_THRESHOLD)  evt = RumbleEvent::LIGHT_IMPACT;

        // ── Trigger a new impact rumble (higher priority wins) ─
        // Only re-trigger if the new event is stronger or current
        // rumble has already expired.
        bool currentExpired = (_activeEvent != RumbleEvent::NONE) &&
                              ((now - _rumbleStartMs) >= _rumbleDurationMs);

        if (evt != RumbleEvent::NONE &&
            (currentExpired || evt > _activeEvent)) {
            triggerImpact(evt, now);
        }

        // ── Speed buzz — continuous low rumble while driving fast
        // Only active when no impact rumble is running
        if (evt == RumbleEvent::NONE) {
            bool aboveThreshold = (speed_kmh >= HapticConfig::SPEED_RUMBLE_THRESHOLD);

            if (aboveThreshold && _activeEvent == RumbleEvent::NONE && !_speedBuzzing) {
                PS4.setRumble(HapticConfig::SPEED_RUMBLE_LARGE,
                              HapticConfig::SPEED_RUMBLE_SMALL);
                _speedBuzzing = true;

            } else if (!aboveThreshold && _speedBuzzing) {
                PS4.setRumble(0, 0);
                _speedBuzzing = false;
            }
        }

        // ── Auto-stop impact rumble after its duration ─────────
        if (_activeEvent != RumbleEvent::NONE && currentExpired) {
            PS4.setRumble(0, 0);
            _activeEvent = RumbleEvent::NONE;

            // Resume speed buzz immediately if still fast enough
            if (speed_kmh >= HapticConfig::SPEED_RUMBLE_THRESHOLD) {
                PS4.setRumble(HapticConfig::SPEED_RUMBLE_LARGE,
                              HapticConfig::SPEED_RUMBLE_SMALL);
                _speedBuzzing = true;
            }
        }
    }

    // Call on emergency stop or controller disconnect
    void stopAll() {
        if (PS4.isConnected()) PS4.setRumble(0, 0);
        _activeEvent  = RumbleEvent::NONE;
        _speedBuzzing = false;
    }

private:
    RumbleEvent _activeEvent      = RumbleEvent::NONE;
    uint32_t    _rumbleStartMs    = 0;
    uint32_t    _rumbleDurationMs = 0;
    bool        _speedBuzzing     = false;

    void triggerImpact(RumbleEvent evt, uint32_t now) {
        uint8_t  large = 0, small = 0;
        uint32_t dur   = 0;

        switch (evt) {
            case RumbleEvent::LIGHT_IMPACT:
                large = HapticConfig::LIGHT_LARGE;
                small = HapticConfig::LIGHT_SMALL;
                dur   = HapticConfig::RUMBLE_LIGHT_MS;
                break;
            case RumbleEvent::MEDIUM_IMPACT:
                large = HapticConfig::MEDIUM_LARGE;
                small = HapticConfig::MEDIUM_SMALL;
                dur   = HapticConfig::RUMBLE_MEDIUM_MS;
                break;
            case RumbleEvent::HEAVY_IMPACT:
                large = HapticConfig::HEAVY_LARGE;
                small = HapticConfig::HEAVY_SMALL;
                dur   = HapticConfig::RUMBLE_HEAVY_MS;
                break;
            default: return;
        }

        PS4.setRumble(large, small);
        _activeEvent      = evt;
        _rumbleStartMs    = now;
        _rumbleDurationMs = dur;
        _speedBuzzing     = false;  // cancel speed buzz during impact
    }
};
