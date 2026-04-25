#pragma once

// ╔══════════════════════════════════════════════════════════════╗
// ║              telemetry.h — Shared State Store                ║
// ║                                                              ║
// ║  All tasks read/write through this struct protected by a     ║
// ║  FreeRTOS mutex. No global variables scattered elsewhere.    ║
// ╚══════════════════════════════════════════════════════════════╝

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstdint>

// ── Raw telemetry payload ─────────────────────────────────────
struct TelemetryData {
    // IMU
    float ax = 0, ay = 0, az = 0;       // m/s²
    float gx = 0, gy = 0, gz = 0;       // °/s
    float roll = 0, pitch = 0;           // degrees

    // GPS
    double  latitude  = 0.0;
    double  longitude = 0.0;
    float   speed_kmh = 0.0f;
    float   altitude  = 0.0f;
    uint8_t satellites = 0;
    bool    gps_valid  = false;

    // Motor / control
    int16_t left_speed  = 0;    // -255..255
    int16_t right_speed = 0;
    bool    ps4_connected = false;
    uint8_t drive_mode  = 0;    // 0=tank, 1=arcade
    uint8_t gear        = 1;    // 1..3

    // Battery
    float   battery_voltage = 0.0f;
    uint8_t battery_percent = 0;

    // System
    uint32_t uptime_s  = 0;
    float    heap_free = 0.0f;   // KB
    uint8_t  ui_page   = 0;      // 0=Analog Dash, 1=Tracker Hub
};

// ── Thread-safe telemetry store ───────────────────────────────
class TelemetryStore {
public:
    static TelemetryStore& instance() {
        static TelemetryStore inst;
        return inst;
    }

    // Write a full update (from sensor/control tasks)
    void write(const TelemetryData& data) {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            _data = data;
            xSemaphoreGive(_mutex);
        }
    }

    // Read a snapshot (from telemetry/web task)
    TelemetryData read() {
        TelemetryData snap;
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            snap = _data;
            xSemaphoreGive(_mutex);
        }
        return snap;
    }

    // Partial update helpers — avoids full copy for single fields
    void setMotors(int16_t l, int16_t r) {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            _data.left_speed  = l;
            _data.right_speed = r;
            xSemaphoreGive(_mutex);
        }
    }

    void setPS4Connected(bool v) {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            _data.ps4_connected = v;
            xSemaphoreGive(_mutex);
        }
    }

    void setIMU(float ax, float ay, float az,
                float gx, float gy, float gz,
                float roll, float pitch) {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            _data.ax = ax; _data.ay = ay; _data.az = az;
            _data.gx = gx; _data.gy = gy; _data.gz = gz;
            _data.roll = roll; _data.pitch = pitch;
            xSemaphoreGive(_mutex);
        }
    }

    void setGPS(double lat, double lng, float spd,
                float alt, uint8_t sats, bool valid) {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            _data.latitude   = lat;
            _data.longitude  = lng;
            _data.speed_kmh  = spd;
            _data.altitude   = alt;
            _data.satellites = sats;
            _data.gps_valid  = valid;
            xSemaphoreGive(_mutex);
        }
    }

    void setBattery(float voltage, uint8_t percent) {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            _data.battery_voltage = voltage;
            _data.battery_percent = percent;
            xSemaphoreGive(_mutex);
        }
    }

private:
    TelemetryStore() {
        _mutex = xSemaphoreCreateMutex();
    }
    TelemetryData   _data;
    SemaphoreHandle_t _mutex = nullptr;
};

// Convenience alias
#define Telemetry TelemetryStore::instance()
