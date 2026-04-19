#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "telemetry.h"

// ─────────────────────────────────────────────────────────────
// Physical Dashboard Handler
// Wraps the user's Adafruit_SSD1306 instance with custom
// "Driving" and "Booting" screens.
// ─────────────────────────────────────────────────────────────

class DisplayHandler {
public:
    DisplayHandler();

    bool begin();
    
    // Screens
    void drawSplash();
    void drawDashboard(const TelemetryData& snap);
    void drawPairingMode(); // WiFi-free pairing instructions

private:
    Adafruit_SSD1306 _display;

    // UI Helpers
    void drawHeader(const TelemetryData& snap);
    void drawCenter(const TelemetryData& snap);
    void drawFooter(const TelemetryData& snap);
};
