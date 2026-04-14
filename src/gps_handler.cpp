#include <Arduino.h>
#include "gps_handler.h"
#include "config.h"
#include <cstring>
#include <cstdlib>

void GPSHandler::begin() {
    _serial.begin(GPSPins::BAUD, SERIAL_8N1, GPSPins::RX, GPSPins::TX);
    Serial.println("[GPS] Bare-metal handler ready on UART1");
}

void GPSHandler::update() {
    while (_serial.available() > 0) {
        char c = _serial.read();
        if (c == '$') {
            _idx = 0;
        } else if (c == '\r' || c == '\n') {
            if (_idx > 0) {
                _buffer[_idx] = '\0';
                parseSentence(_buffer);
                _idx = 0;
            }
        } else if (_idx < sizeof(_buffer) - 1) {
            _buffer[_idx++] = c;
        }
    }
}

GPSReading GPSHandler::read() {
    return _data;
}

void GPSHandler::parseSentence(char* line) {
    if (strncmp(line, "GPGGA", 5) == 0 || strncmp(line, "GNGGA", 5) == 0) {
        parseGPGGA(line);
    } else if (strncmp(line, "GPRMC", 5) == 0 || strncmp(line, "GNRMC", 5) == 0) {
        parseGPRMC(line);
    }
}

// $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
void GPSHandler::parseGPGGA(char* line) {
    char* p = line;
    int col = 0;
    char* term;
    
    char* latStr = nullptr;
    char* latDir = nullptr;
    char* lonStr = nullptr;
    char* lonDir = nullptr;

    while ((term = strsep(&p, ",")) != nullptr) {
        switch (col) {
            case 2: latStr = term; break;
            case 3: latDir = term; break;
            case 4: lonStr = term; break;
            case 5: lonDir = term; break;
            case 6: _data.has_fix = (atoi(term) > 0); break;
            case 7: _data.satellites = (uint8_t)atoi(term); break;
            case 9: _data.altitude_m = (float)atof(term); break;
        }
        col++;
    }

    if (latStr && latDir && lonStr && lonDir && _data.has_fix) {
        _data.latitude = parseDegrees(latStr, latDir);
        _data.longitude = parseDegrees(lonStr, lonDir);
        _data.valid = true;
    }
}

// $GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
void GPSHandler::parseGPRMC(char* line) {
    char* p = line;
    int col = 0;
    char* term;

    while ((term = strsep(&p, ",")) != nullptr) {
        switch (col) {
            case 2: _data.has_fix = (*term == 'A'); break;
            case 7: _data.speed_kmh = (float)atof(term) * 1.852f; break; // knots to km/h
        }
        col++;
    }
}

double GPSHandler::parseDegrees(const char* term, const char* dir) {
    if (!term || !*term) return 0.0;
    double raw = atof(term);
    int degrees = (int)(raw / 100);
    double minutes = raw - (degrees * 100);
    double dec = degrees + (minutes / 60.0);
    if (*dir == 'S' || *dir == 'W') dec = -dec;
    return dec;
}
