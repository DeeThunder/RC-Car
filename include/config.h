#pragma once

// ╔══════════════════════════════════════════════════════════════╗
// ║                  config.h — Central Config                   ║
// ║  All hardware pins, tunable constants, and feature flags     ║
// ║  live here. Never scatter magic numbers through the code.    ║
// ╚══════════════════════════════════════════════════════════════╝

#include <cstdint>

#include "secret.h"

// ── PS4 Controller ────────────────────────────────────────────
namespace PS4Config {
    constexpr int16_t     DEAD_ZONE   = 20;   // joystick dead zone (0–128)
}

// ── L298N Motor Driver Pins ───────────────────────────────────
namespace MotorPins {
    // Left motors (front-left + rear-left wired in parallel)
    constexpr uint8_t ENA = 25;   // PWM speed control — left
    constexpr uint8_t IN1 = 26;   // Direction A
    constexpr uint8_t IN2 = 27;   // Direction B

    // Right motors (front-right + rear-right wired in parallel)
    constexpr uint8_t ENB = 14;   // PWM speed control — right
    constexpr uint8_t IN3 = 32;   // Direction A
    constexpr uint8_t IN4 = 33;   // Direction B
}

// ── Motor PWM Config ──────────────────────────────────────────
namespace MotorPWM {
    constexpr uint32_t FREQUENCY   = 1000;   // Hz
    constexpr uint8_t  RESOLUTION  = 8;      // bits → 0..255
    constexpr uint8_t  CHANNEL_L   = 0;
    constexpr uint8_t  CHANNEL_R   = 1;
    constexpr uint8_t  MAX_SPEED   = 255;
}

// ── MPU6050 I²C Pins ──────────────────────────────────────────
namespace IMUPins {
    constexpr uint8_t SDA = 21;
    constexpr uint8_t SCL = 22;
}

// ── GPS Serial Pins ───────────────────────────────────────────
namespace GPSPins {
    constexpr uint8_t  RX   = 16;
    constexpr uint8_t  TX   = 17;
    constexpr uint32_t BAUD = 9600;
}

// ── Battery Monitor (ADC) ─────────────────────────────────────
namespace BatteryConfig {
    constexpr uint8_t  ADC_PIN        = 34; // ADC1_CH6 (Safe with WiFi)
    // Voltage divider: 100kΩ (R1) + 30kΩ (R2) → scale factor
    constexpr float    DIVIDER_RATIO  = (100.0f + 30.0f) / 30.0f;
    constexpr float    ADC_VREF       = 3.3f;
    constexpr uint16_t ADC_MAX        = 4095;
    constexpr float    BATTERY_MAX_V  = 12.6f;  // 3S Li-ion full (4.2V/cell)
    constexpr float    BATTERY_MIN_V  = 9.0f;   // 3S Li-ion cutoff (3.0V/cell)
    constexpr uint8_t  SAMPLE_COUNT   = 16;      // oversample for noise
}

// ── FreeRTOS Task Config ──────────────────────────────────────
namespace TaskConfig {
    // Stack sizes (bytes for ESP-IDF)
    constexpr uint32_t SENSOR_STACK    = 4096;
    constexpr uint32_t CONTROL_STACK   = 4096;
    constexpr uint32_t TELEMETRY_STACK = 8192;
    constexpr uint32_t GPS_STACK       = 4096;

    // Priorities (higher = more urgent; keep control highest)
    constexpr uint8_t CONTROL_PRIORITY   = 5;
    constexpr uint8_t SENSOR_PRIORITY    = 4;
    constexpr uint8_t GPS_PRIORITY       = 3;
    constexpr uint8_t TELEMETRY_PRIORITY = 2;

    // Core affinity
    constexpr uint8_t CORE_0 = 0;   // WiFi/BT lives here
    constexpr uint8_t CORE_1 = 1;   // App tasks here

    // Loop periods (ms)
    constexpr uint32_t CONTROL_PERIOD   = 10;    // 100 Hz
    constexpr uint32_t SENSOR_PERIOD    = 20;    // 50 Hz
    constexpr uint32_t GPS_PERIOD       = 100;   // 10 Hz
    constexpr uint32_t TELEMETRY_PERIOD = 100;   // 10 Hz (Power Optimized)
}

// ── Haptic Feedback Config ────────────────────────────────────
// Uses MPU6050 acceleration data already in TelemetryStore.
// No extra hardware needed — sensorTask populates ax/ay/az at 50Hz.
namespace HapticConfig {
    // Acceleration deviation thresholds from 1g (m/s²)
    // delta = |sqrt(ax²+ay²+az²) - 9.81|
    constexpr float LIGHT_IMPACT_THRESHOLD  = 3.0f;
    constexpr float MEDIUM_IMPACT_THRESHOLD = 8.0f;
    constexpr float HEAVY_IMPACT_THRESHOLD  = 15.0f;

    // Rumble durations (ms) — non-blocking, millis() based
    constexpr uint32_t RUMBLE_LIGHT_MS  = 80;
    constexpr uint32_t RUMBLE_MEDIUM_MS = 180;
    constexpr uint32_t RUMBLE_HEAVY_MS  = 400;

    // PS4.setRumble(large_motor 0-255, small_motor 0-255)
    // Large = low-freq thud,  Small = high-freq buzz
    constexpr uint8_t LIGHT_LARGE  =   0;  constexpr uint8_t LIGHT_SMALL  =  80;
    constexpr uint8_t MEDIUM_LARGE = 100;  constexpr uint8_t MEDIUM_SMALL = 120;
    constexpr uint8_t HEAVY_LARGE  = 255;  constexpr uint8_t HEAVY_SMALL  = 200;

    // Continuous speed buzz above this GPS speed (km/h)
    constexpr float   SPEED_RUMBLE_THRESHOLD = 8.0f;
    constexpr uint8_t SPEED_RUMBLE_LARGE     =  0;
    constexpr uint8_t SPEED_RUMBLE_SMALL     = 30;
}

// ── Web Server ────────────────────────────────────────────────
namespace WebConfig {
    constexpr uint16_t HTTP_PORT = 80;
    constexpr uint16_t WS_PORT  = 80;
}
