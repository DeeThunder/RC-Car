#include "display_handler.h"
#include "display_fonts.h"
#include "config.h"
#include <math.h>
#include <string.h>

// SSD1306 Initialization commands
const uint8_t init_sequence[] = {
    0xAE, // Display OFF (sleep mode)
    0x20, 0x00, // Set Memory Addressing Mode (0x00 = Horizontal Addressing Mode)
    0xB0, // Set Page Start Address for Page Addressing Mode, 0-7
    0xC8, // Set COM Output Scan Direction (remaps Y)
    0x00, // Set low column address
    0x10, // Set high column address
    0x40, // Set start line address
    0x81, 0xFF, // Set contrast control register
    0xA1, // Set Segment Re-map (remaps X)
    0xA6, // Set normal display (0xA7 for inverse)
    0xA8, 0x3F, // Set multiplex ratio (1 to 64)
    0xA4, // 0xa4 = Output follows RAM content; 0xa5 = Output ignores RAM
    0xD3, 0x00, // Set display offset
    0xD5, 0xF0, // Set display clock divide ratio/oscillator frequency
    0xD9, 0x22, // Set pre-charge period
    0xDA, 0x12, // Set com pins hardware configuration
    0xDB, 0x20, // Set vcomh
    0x8D, 0x14, // Set DC-DC enable (Charge pump)
    0xAF  // Display ON in normal mode
};

DisplayHandler::DisplayHandler() : _addr(DisplayConfig::ADDR) {
    clear();
}

bool DisplayHandler::begin(uint8_t i2cAddr) {
    _addr = i2cAddr;
    delay(10);
    // Send init sequence
    for (uint8_t i = 0; i < sizeof(init_sequence); i++) {
        sendCommand(init_sequence[i]);
    }
    clear();
    update();
    return true;
}

void DisplayHandler::sendCommand(uint8_t command) {
    Wire.beginTransmission(_addr);
    Wire.write(0x00); // Co = 0, D/C = 0
    Wire.write(command);
    Wire.endTransmission();
}

void DisplayHandler::update() {
    // SSD1306 requires resetting the column and page bounds before writing buffer
    sendCommand(0x21); // Set Column Address
    sendCommand(0);    // Start
    sendCommand(127);  // End

    sendCommand(0x22); // Set Page Address
    sendCommand(0);    // Start
    sendCommand(7);    // End (64 rows / 8)

    // Blast the 1024-byte buffer over I2C in chunks of 16 (Wire library limit is usually 32)
    for (uint16_t i = 0; i < BUFFER_SIZE; i += 16) {
        Wire.beginTransmission(_addr);
        Wire.write(0x40); // Data mode
        for (uint8_t j = 0; j < 16; j++) {
            Wire.write(_buffer[i + j]);
        }
        Wire.endTransmission();
    }
}

void DisplayHandler::clear() {
    memset(_buffer, 0, BUFFER_SIZE);
}

// ── Primitive Graphics Engine ───────────────────────────────

void DisplayHandler::drawPixel(int16_t x, int16_t y, bool color) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return;
    if (color) {
        _buffer[x + (y / 8) * SCREEN_WIDTH] |=  (1 << (y & 7));
    } else {
        _buffer[x + (y / 8) * SCREEN_WIDTH] &= ~(1 << (y & 7));
    }
}

// Bresenham's algorithm
void DisplayHandler::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool color) {
    int16_t dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int16_t dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy, e2;

    while (true) {
        drawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void DisplayHandler::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, bool color) {
    drawLine(x, y, x + w - 1, y, color);
    drawLine(x, y + h - 1, x + w - 1, y + h - 1, color);
    drawLine(x, y, x, y + h - 1, color);
    drawLine(x + w - 1, y, x + w - 1, y + h - 1, color);
}

void DisplayHandler::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool color) {
    for (int16_t i = x; i < x + w; i++) {
        drawLine(i, y, i, y + h - 1, color);
    }
}

// ── Typography Engine ────────────────────────────────────────

void DisplayHandler::printChar(int16_t x, int16_t y, char c) {
    if (c < 32 || c > 93) return; // Out of bounds of our minimal font
    const uint8_t* ptr = font5x7[c - 32];
    for (int8_t i = 0; i < 5; i++) {
        uint8_t line = ptr[i];
        for (int8_t j = 0; j < 8; j++) {
            if (line & 0x1) drawPixel(x + i, y + j);
            line >>= 1;
        }
    }
}

void DisplayHandler::printText(int16_t x, int16_t y, const char* text) {
    int16_t cursor_x = x;
    while (*text) {
        printChar(cursor_x, y, *text);
        cursor_x += 6; // 5px width + 1px spacing
        text++;
    }
}

void DisplayHandler::printBigDigit(int16_t x, int16_t y, char c) {
    if (c < '0' || c > '9') return;
    const uint8_t* ptr = font12x16_digits[c - '0'];

    for (int8_t col = 0; col < 12; col++) {
        // Top 8 pixels
        uint8_t line = ptr[col];
        for (int8_t row = 0; row < 8; row++) {
            if (line & 0x1) drawPixel(x + col, y + row);
            line >>= 1;
        }
        // Bottom 8 pixels
        line = ptr[col + 12];
        for (int8_t row = 0; row < 8; row++) {
            if (line & 0x1) drawPixel(x + col, y + row + 8);
            line >>= 1;
        }
    }
}

// ── UI Components ──────────────────────────────────────────

void DisplayHandler::drawPhoneBattery(int16_t x, int16_t y, float voltage) {
    // Battery Shell
    drawRect(x, y, 18, 8);
    drawLine(x + 18, y + 2, x + 18, y + 5); // Nub

    // Calculate fill
    float pct = (voltage - BatteryConfig::BATTERY_MIN_V) / 
                (BatteryConfig::BATTERY_MAX_V - BatteryConfig::BATTERY_MIN_V);
    if (pct < 0) pct = 0; if (pct > 1) pct = 1;
    
    int fillPixels = (int)(pct * 14.0f);
    if (pct > 0.05f && fillPixels < 1) fillPixels = 1;
    
    fillRect(x + 2, y + 2, fillPixels, 4);

    // Print text percentage
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", (int)(pct * 100));
    printText(x - 24, y + 1, buf);
}

void DisplayHandler::drawAnalogDial(int speed_kmh) {
    int cx = 64;
    int cy = 52;
    int r  = 35;

    // Draw the half-circle arc using basic points
    for (int i = 180; i >= 0; i -= 5) {
        float rad = i * 3.14159f / 180.0f;
        int px = cx + (int)(cos(rad) * r);
        int py = cy - (int)(sin(rad) * r);
        drawPixel(px, py);
        
        // Draw tick marks every 30 degrees
        if (i % 30 == 0) {
            int px_in = cx + (int)(cos(rad) * (r - 4));
            int py_in = cy - (int)(sin(rad) * (r - 4));
            drawLine(px_in, py_in, px, py);
        }
    }

    // Speed labels
    printText(cx - r - 6, cy - 8, "0");
    printText(cx + r + 2, cy - 8, "30");

    // Center HUB
    fillRect(cx - 2, cy - 1, 5, 3);

    // Calculate Needle Angle (0 kmh = 180 deg, 30 kmh = 0 deg)
    float speed_clamped = speed_kmh > 30 ? 30 : speed_kmh;
    float angle_deg = 180.0f - (speed_clamped * (180.0f / 30.0f));
    float rad = angle_deg * 3.14159f / 180.0f;
    
    int nx = cx + (int)(cos(rad) * (r - 2));
    int ny = cy - (int)(sin(rad) * (r - 2));
    drawLine(cx, cy, nx, ny);

    // Dynamic digital speed mirror
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", speed_kmh);
    if (speed_kmh < 10) printBigDigit(cx - 6, cy - 25, buf[0]);
    else {
        printBigDigit(cx - 13, cy - 25, buf[0]);
        printBigDigit(cx + 1, cy - 25, buf[1]);
    }
}

// ── Dashboard Pages ─────────────────────────────────────────

void DisplayHandler::drawSplash() {
    clear();
    printText(10, 10, "DEETHUNDER RC");
    printText(10, 30, "Bare-Metal Engine");
    printText(10, 48, "System Ready.");
    update();
}

void DisplayHandler::drawAnalogDashboard(const TelemetryData& snap) {
    clear();

    // 1. Header (Gear + Battery)
    char gearBuf[8];
    snprintf(gearBuf, sizeof(gearBuf), "G: %d", snap.gear);
    printText(0, 0, gearBuf);
    drawPhoneBattery(106, 0, snap.battery_voltage);

    // 2. Analog Speedometer
    drawAnalogDial((int)snap.speed_kmh);

    // 3. Footer (IMU + BT)
    char footerBuf[32];
    snprintf(footerBuf, sizeof(footerBuf), "R:%d  P:%d", (int)snap.roll, (int)snap.pitch);
    printText(0, 56, footerBuf);
    
    if (snap.ps4_connected) printText(110, 56, "BT");

    update();
}

void DisplayHandler::drawTrackerDashboard(const TelemetryData& snap) {
    clear();

    printText(0, 0, "TELEMETRY HUB");
    drawLine(0, 9, 128, 9);

    char buf[32];
    
    // GPS Coordinates
    snprintf(buf, sizeof(buf), "LAT: %.6f", snap.latitude);
    printText(0, 14, buf);
    
    snprintf(buf, sizeof(buf), "LNG: %.6f", snap.longitude);
    printText(0, 24, buf);

    // Sats and Alt
    snprintf(buf, sizeof(buf), "SATS: %d  ALT: %dm", snap.satellites, (int)snap.altitude);
    printText(0, 38, buf);
    
    drawLine(0, 48, 128, 48);

    // Footer Info
    snprintf(buf, sizeof(buf), "UPTIME: %ds", snap.uptime_s);
    printText(0, 54, buf);
    if (!snap.ps4_connected) printText(90, 54, "NO PAD");

    update();
}
