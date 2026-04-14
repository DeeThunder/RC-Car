#pragma once

// ╔══════════════════════════════════════════════════════════════╗
// ║              gps_handler.h                                   ║
// ║  Bare-metal NMEA parser for GPS module on UART1.             ║
// ║  Removes dependency on TinyGPS++ to save flash and heap.     ║
// ╚══════════════════════════════════════════════════════════════╝

#include <HardwareSerial.h>

struct GPSReading {
    double  latitude   = 0.0;
    double  longitude  = 0.0;
    float   speed_kmh  = 0.0f;
    float   altitude_m = 0.0f;
    uint8_t satellites = 0;
    bool    valid      = false;
    bool    has_fix    = false;
};

class GPSHandler {
public:
    void begin();

    // Call frequently — drains UART buffer and parses NMEA
    void update();

    // Return latest fix
    GPSReading read();

    bool hasFix() { return _data.has_fix; }

    // ── Public for Unit Testing ──────────────────────────────
    void parseSentence(char* line);

private:
    HardwareSerial _serial{1};   // UART1
    GPSReading _data;

    // Parser state
    char _buffer[128];
    uint8_t _idx = 0;

    void parseGPGGA(char* line);
    void parseGPRMC(char* line);
    
    double parseDegrees(const char* term, const char* dir);
};
