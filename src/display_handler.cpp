#include <Arduino.h>
#include <Wire.h>
#include "display_handler.h"
#include "config.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

DisplayHandler::DisplayHandler() 
    : _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET) {}

// ─────────────────────────────────────────────────────────────
bool DisplayHandler::begin() {
    // We assume Wire.begin() has already been called in IMUHandler::begin()
    if (!_display.begin(SSD1306_SWITCHCAPVCC, DisplayConfig::ADDR)) {
        Serial.println(F("[Display] SSD1306 allocation failed"));
        return false;
    }
    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);
    _display.display();
    return true;
}

// ─────────────────────────────────────────────────────────────
void DisplayHandler::drawSplash() {
    _display.clearDisplay();
    _display.setTextSize(2);
    _display.setCursor(10, 10);
    _display.println("DEETHUNDER");
    _display.setTextSize(1);
    _display.setCursor(10, 35);
    _display.println("Radio-Silent Cockpit");
    _display.setCursor(10, 48);
    _display.println("System Ready...");
    _display.display();
}

// ─────────────────────────────────────────────────────────────
void DisplayHandler::drawDashboard(const TelemetryData& snap) {
    _display.clearDisplay();
    
    drawHeader(snap);
    drawCenter(snap);
    drawFooter(snap);
    
    _display.display();
}

// ─────────────────────────────────────────────────────────────
void DisplayHandler::drawPairingMode() {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setCursor(0, 0);
    _display.println("--- PAIRING MODE ---");
    _display.setCursor(0, 20);
    _display.println("1. Hold SHARE + PS");
    _display.setCursor(0, 32);
    _display.println("2. Release on flash");
    _display.setCursor(0, 50);
    _display.println("Wait for BT light...");
    _display.display();
}

// ─────────────────────────────────────────────────────────────
void DisplayHandler::drawHeader(const TelemetryData& snap) {
    // Gear on left
    _display.setTextSize(1);
    _display.setCursor(0, 0);
    _display.print("GEAR:");
    _display.print(snap.gear);
    
    // Battery on right
    _display.setCursor(80, 0);
    _display.print(snap.battery_voltage, 1);
    _display.print("V");
    
    // Battery fuel bar
    float pct = (snap.battery_voltage - 9.0) / (12.6 - 9.0);
    if (pct < 0) pct = 0; if (pct > 1) pct = 1;
    _display.drawRect(80, 10, 45, 6, SSD1306_WHITE);
    _display.fillRect(82, 12, (int)(41 * pct), 2, SSD1306_WHITE);
    
    _display.drawLine(0, 18, 128, 18, SSD1306_WHITE);
}

// ─────────────────────────────────────────────────────────────
void DisplayHandler::drawCenter(const TelemetryData& snap) {
    _display.setTextSize(3);
    _display.setCursor(20, 25);
    
    // Virtual speed (derived from RPM/PWM usually, here just a placeholder)
    // We'll use throttle % as a proxy for speed in this minimal view
    int speed = abs(snap.left_speed + snap.right_speed) / 5; // roughly 0-50
    if (speed < 10) _display.print("0");
    _display.print(speed);
    
    _display.setTextSize(1);
    _display.print(" km/h");
}

// ─────────────────────────────────────────────────────────────
void DisplayHandler::drawFooter(const TelemetryData& snap) {
    _display.drawLine(0, 50, 128, 50, SSD1306_WHITE);
    
    // Tilt levels
    _display.setTextSize(1);
    _display.setCursor(0, 55);
    _display.print("R:"); _display.print((int)snap.roll);
    _display.print(" P:"); _display.print((int)snap.pitch);
    
    // BT indicator
    if (snap.ps4_connected) {
        _display.setCursor(100, 55);
        _display.print("BT");
    }
}
