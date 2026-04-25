#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "telemetry.h"

// ─────────────────────────────────────────────────────────────
// Bare-Metal Display Engine (SSD1306)
// Zero external libraries. 100% custom raster math and I2C.
// ─────────────────────────────────────────────────────────────

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define BUFFER_SIZE   1024

class DisplayHandler {
public:
    DisplayHandler();

    // Hardware Interface
    bool begin(uint8_t i2cAddr = 0x3C);
    void update(); // Blast buffer to I2C

    // Primitive Graphics Engine
    void clear();
    void drawPixel(int16_t x, int16_t y, bool color = true);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool color = true);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, bool color = true);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool color = true);
    void drawCircle(int16_t x0, int16_t y0, int16_t r, bool color = true);
    
    // Typography Engine
    void printChar(int16_t x, int16_t y, char c);
    void printText(int16_t x, int16_t y, const char* text);
    void printBigDigit(int16_t x, int16_t y, char c); // Custom 12x16 font

    // UI Dashboards (The Pages)
    void drawSplash();
    void drawAnalogDashboard(const TelemetryData& snap); // Page 1
    void drawTrackerDashboard(const TelemetryData& snap); // Page 2

private:
    uint8_t _addr;
    uint8_t _buffer[BUFFER_SIZE];

    // Low-level command sender
    void sendCommand(uint8_t command);

    // UI Sub-components
    void drawPhoneBattery(int16_t x, int16_t y, float voltage);
    void drawAnalogDial(int speed_kmh);
};
